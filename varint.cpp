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

#include "compiler.h"
#include "varint.h"

using namespace std;

// Generate n log-uniform random numbers.
vector<uint64_t> gen_log_uniform() {

  // How many uint64_t numbers in the vector we're compressing.
  const size_t N = 1000000;

  // Maximum significant bits in the log-uniform random numbers.
  const unsigned random_bits = 64;

  default_random_engine gen;
  uniform_real_distribution<double> dist(0.0, random_bits * log(2));
  vector<uint64_t> vec;

  vec.reserve(N);
  while (vec.size() < N)
    vec.push_back(exp(dist(gen)));
  return vec;
}

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
  double secs = duration_cast<nanoseconds>(after - before).count();
  return secs/1.0e9;
}

double measure_decode(const uint8_t *in, vector<uint64_t> &out, decoder_func func) {
  return time_decode(in, out, func, 1);;
}

double do_codec(const codec_descriptor &codec,
                const vector<uint64_t> &numbers,
                uint8_t *scratch)
{
  vector<uint64_t> decompressed(numbers.size());

  using namespace chrono;
  auto before = high_resolution_clock::now();
  uint64_t encodedSize = codec.encoder(numbers, scratch);
  auto after = high_resolution_clock::now();
  double encode_secs = duration_cast<nanoseconds>(after - before).count()/1.0e9;


  double dtime = time_decode(scratch, decompressed, codec.decoder);
  assert(decompressed == numbers);

  printf("%-15s: %.3f bytes/integer, encode speed: %.2f MB/s, decode speed %.2f MB/s\r\n",
            codec.name,
            double(encodedSize) / numbers.size(),
            (sizeof(uint64_t)*numbers.size()/encode_secs)/1.0e6,
            (encodedSize/(dtime))/1.0e6);

  return dtime;
}

int main(int argc, const char *argv[]) {
  vector<uint64_t> numbers;

  if (argc == 2) {
    numbers = read_test_vector(argv[1]);
    printf("Read %zu integers from %s.\n", numbers.size(), argv[1]);
  } else {
    numbers = gen_log_uniform();
    printf("Generated %zu log-uniform integers.\n", numbers.size());
  }

  // Allocate an output buffer 2x the size of our input numbers
  uint8_t *scratch = static_cast<uint8_t*>(std::malloc(2*sizeof(uint64_t)*numbers.size()));
  if (scratch == nullptr) {
    printf("Could not allocate a temporary buffer of size %lu bytes\r\n",
              2*sizeof(uint64_t)*numbers.size());
    exit(1);
  }

  double leb128 = do_codec(leb128_codec, numbers, scratch);
  double prefix = do_codec(prefix_codec, numbers, scratch);
  double sqlite = do_codec(lesqlite_codec, numbers, scratch);
  double sqlite2 = do_codec(lesqlite2_codec, numbers, scratch);

  // printf("T(LEB128) / T(PrefixVarint) = %.3f.\n", leb128 / prefix);
  // printf("T(LEB128) / T(leSQLite) = %.3f.\n", leb128 / sqlite);
  // printf("T(LEB128) / T(leSQLite2) = %.3f.\n", leb128 / sqlite2);
}
