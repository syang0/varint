// Copyright 2016 Jakob Stoklund Olesen
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cassert>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

#include "zlib.h"

#include "compiler.h"
#include "varint.h"

using namespace std;

// How many uint64_t numbers in the vector we're compressing.
const size_t N = 1000000;

/**
 * Generate n log-uniform random numbers with maximum value
 * equal to 2^maxBits
 *
 * \param maxBits
 *      restricts the maximum value to the second power of this number
 * \param minBits
 *      restrics the minimum value to the second power of this number
 *
 */
vector<uint64_t> gen_log_uniform(int minBits = 0, int maxBits=64) {
  default_random_engine gen;
  uniform_real_distribution<double> dist(minBits * log(2), maxBits * log(2));
  vector<uint64_t> vec;

  vec.reserve(N);
  while (vec.size() < N)
    vec.push_back(exp(dist(gen)));
  return vec;
}

// Stores the benchmark result of a single algorithm for a single run
struct TestResult {
  std::string algorithmName;
  double encodeSecs;
  double decodeSecs;
  uint64_t inputBytes;
  uint64_t outputBytes;
};

// Stores the result of multiple algorithms given a single input dataset
struct TestSuite {
  int minBits;
  int maxBits;
  std::vector<TestResult> results;
};

// Read test vector from a file.
//
// Format: One unsigned number per line.
vector<uint64_t> read_test_vector(const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    perror(filename);
    exit(1);
  }

  vector<uint64_t> vec;
  while (!feof(f)) {
    uint64_t val;
    int rc = fscanf(f, "%" SCNu64 "\n", &val);
    if (rc == 1) {
      vec.push_back(val);
    } else if (rc == EOF) {
      if (ferror(f)) {
        perror(filename);
        exit(1);
      } else {
        break;
      }
    }
  }
  return vec;
}

double time_decode(const uint8_t *in, vector<uint64_t> &out, decoder_func func,
                   unsigned repetitions=1) {
  using namespace chrono;
  auto before = high_resolution_clock::now();
  for (unsigned n = 0; n < repetitions; n++)
    func(in, out.data(), out.size());
  auto after = high_resolution_clock::now();
  double secs = duration_cast<nanoseconds>(after - before).count()/repetitions;
  return secs/1.0e9;
}

TestResult do_codec(const codec_descriptor &codec,
                const vector<uint64_t> &numbers,
                uint8_t *scratch)
{
  vector<uint64_t> decompressed(numbers.size());
  codec.encoder(numbers, scratch); // Warmup decode

  using namespace chrono;
  auto before = high_resolution_clock::now();
  uint64_t encodedSize = codec.encoder(numbers, scratch);
  auto after = high_resolution_clock::now();
  double encode_secs = duration_cast<nanoseconds>(after - before).count()/1.0e9;


  double dtime = time_decode(scratch, decompressed, codec.decoder);
  assert(decompressed == numbers);

  // printf("%-15s: %10.2f %10.02f %10.2f\r\n",
  //           codec.name,
  //           double(encodedSize) / numbers.size(),
  //           (sizeof(uint64_t)*numbers.size()/encode_secs)/1.0e6,
  //           (encodedSize/(dtime))/1.0e6);

  return {codec.name, encode_secs, dtime, sizeof(numbers.front())*numbers.size(), encodedSize};
}

/**
 * Compresses an array of numbers using libz's
 *
 * \param numbers
 *    Input compression array
 * \param input bytes
 *    bytes to read from numbers array
 * \param scratch
 *    Output buffer
 * \param outputBytes
 *    Bytes allocated for the output array
 * \param level
 *    gzip compression level [0,9]
 */
TestResult do_libz(const uint64_t *numbers,
                      size_t inputBytes,
                      uint8_t *scratch,
                      size_t outputBytes,
                      int level)
{
  uint64_t compressedSize = outputBytes;
  compress2(scratch, &compressedSize,
              reinterpret_cast<const Bytef*>(numbers), inputBytes,
              level); // Warmup decode

  using namespace chrono;
  auto before = high_resolution_clock::now();
  compressedSize = outputBytes;
  compress2(scratch, &compressedSize,
              reinterpret_cast<const Bytef*>(numbers), inputBytes,
              level); // Warmup decode
  auto after = high_resolution_clock::now();
  double encode_secs = duration_cast<nanoseconds>(after - before).count()/1.0e9;

  std::string name = "libz-" + std::to_string(level);
  return {name, encode_secs, 0.0, inputBytes, compressedSize};
}

void printResults(std::vector<TestSuite> tests) {
  printf("\r\n# Compressing 64-bit numbers with uniform random bits between n and m\r\n");
  printf("# Each column represent \"input speed (MB/s)\"\r\n");
  printf("#%-15s", "Algo / bits");
  for (TestSuite tr : tests)
    printf("  %2d - %2d", tr.minBits, tr.maxBits);
  printf("\r\n");

  for (int j = 0; j < tests.front().results.size(); ++j) {
    printf("%-16s", tests.front().results[j].algorithmName.c_str());
    for (int i = 0; i < tests.size(); ++i) {
      TestResult results = tests[i].results[j];
      printf(" %8.2f", results.inputBytes/(1.0e6*results.encodeSecs));
    }
    printf("\r\n");
  }


  printf("\r\n# Compressing 64-bit numbers with uniform random bits between n and m\r\n");
  printf("# Each column represent \"avg bits per number\"\r\n");
  printf("#%-15s", "Algo / bits");
  for (TestSuite tr : tests)
    printf("  %2d - %2d", tr.minBits, tr.maxBits);
  printf("\r\n");

  for (int j = 0; j < tests.front().results.size(); ++j) {
    printf("%-16s", tests.front().results[j].algorithmName.c_str());
    for (int i = 0; i < tests.size(); ++i) {
      TestResult results = tests[i].results[j];
      printf(" %8.2f", results.outputBytes/(1.0*N));
    }
    printf("\r\n");
  }
}

int main(int argc, const char *argv[]) {
  // stores input and output numbers
  vector<uint64_t> numbers, decompressed;

  if (argc == 2) {
    numbers = read_test_vector(argv[1]);
    printf("#Read %zu integers from %s.\n", numbers.size(), argv[1]);
  } else {
    numbers = gen_log_uniform();
    printf("#Generated %zu log-uniform integers.\n", numbers.size());
  }

  // Allocate an output buffer 2x the size of our input numbers
  size_t allocSize = 2*sizeof(uint64_t)*numbers.size();
  uint8_t *scratch = static_cast<uint8_t*>(std::malloc(allocSize));
  if (scratch == nullptr) {
    printf("Could not allocate a temporary buffer of size %lu bytes\r\n",
              allocSize);
    exit(1);
  }

  std::vector<TestSuite> randomUpTo;
  for (int i = 8; i <= 64; i += 8) {
    // printf("\r\n# With uniform random number of significant bits up to %d\r\n", i);
    // printf("#%-14s %10s %10s %10s\r\n", "Algorithm", "bytes/int", "Encode MB/s", "Decode MB/s");

    numbers = gen_log_uniform(0, i);
    std::vector<TestResult> randomRun;
    printf("# Doing uniform up to %d bits\r\n", i);
    randomRun.push_back(do_codec(prefix_codec, numbers, scratch));
    randomRun.push_back(do_codec(leb128_codec, numbers, scratch));
    randomRun.push_back(do_codec(lesqlite2_codec, numbers, scratch));
    randomRun.push_back(do_codec(lesqlite_codec, numbers, scratch));
    randomRun.push_back(do_codec(nanolog_codec, numbers, scratch));
    randomRun.push_back(do_libz(numbers.data(), numbers.size()*sizeof(numbers.front()), scratch, allocSize, 1));
    randomRun.push_back(do_libz(numbers.data(), numbers.size()*sizeof(numbers.front()), scratch, allocSize, 9));
    randomUpTo.push_back({0, i,randomRun});
  }

  std::vector<TestSuite> randomBetween;
  for (int i = 8; i <= 64; i += 8) {
    // printf("\r\n# With uniform random number of significant between %d and %d\r\n", i-8, i);
    // printf("#%-14s %10s %10s %10s\r\n", "Algorithm", "bytes/int", "Encode MB/s", "Decode MB/s");

    numbers = gen_log_uniform(i-8, i);
    std::vector<TestResult> randomRun;
    printf("# Doing range between %d and %d bits\r\n", i-8, i);
    randomRun.push_back(do_codec(prefix_codec, numbers, scratch));
    randomRun.push_back(do_codec(leb128_codec, numbers, scratch));
    randomRun.push_back(do_codec(lesqlite2_codec, numbers, scratch));
    randomRun.push_back(do_codec(lesqlite_codec, numbers, scratch));
    randomRun.push_back(do_codec(nanolog_codec, numbers, scratch));
    randomRun.push_back(do_libz(numbers.data(), numbers.size()*sizeof(numbers.front()), scratch, allocSize, 1));
    randomRun.push_back(do_libz(numbers.data(), numbers.size()*sizeof(numbers.front()), scratch, allocSize, 9));

    randomBetween.push_back({i-8, i, randomRun});
  }

  // Print Results.
  printResults(randomUpTo);
  printResults(randomBetween);

  // printf("\r\n# Compressing 64-bit numbers with uniform random bits between n and m\r\n");
  // printf("# Each column represent \"speed (MB/s)\"\r\n");
  // printf("#%-6s", "Bits");
  // for (TestResult tr : randomBetween.front().results)
  //   printf(" %10s", tr.algorithmName);
  // printf("\r\n");

  // for (TestSuite ts : randomBetween) {
  //   printf("%2d - %2d", ts.minBits, ts.maxBits);
  //   for (TestResult tr: ts.results)
  //     printf(" %10.2f", (tr.inputBytes/tr.encodeSecs)/1.0e6);

  //   printf("\r\n");
  // }


  // printf("T(LEB128) / T(PrefixVarint) = %.3f.\n", leb128 / prefix);
  // printf("T(LEB128) / T(leSQLite) = %.3f.\n", leb128 / sqlite);
  // printf("T(LEB128) / T(leSQLite2) = %.3f.\n", leb128 / sqlite2);
}
