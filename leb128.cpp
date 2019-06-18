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

// This implements the standard LEB128 codec.

#include "compiler.h"
#include "varint.h"

using namespace std;

/**
 * Encodes a vector of 64-bit numbers into a char buffer.
 * Note: For safety, ensure that the *out buffer is at least
 * 2x the size of the input vector.
 *
 * \param in
 *    Buffer of uint64_t's to compress
 *
 * \param[out] out
 *    Buffer to write compressed data to
 *
 * \return
 *    Numbers of bytes written to *out
 */
uint64_t leb128_encode2(const vector<uint64_t> &in, uint8_t *out) {
  uint8_t *out_orig = out;
  for (auto x : in) {
    while (x > 127) {
      *out = (x | 0x80);
      ++out;
      x >>= 7;
    }
    *out = (x);
    ++out;
  }
  return out - out_orig;
}

vector<uint8_t> leb128_encode(const vector<uint64_t> &in) {
  vector<uint8_t> out;
  for (auto x : in) {
    while (x > 127) {
      out.push_back(x | 0x80);
      x >>= 7;
    }
    out.push_back(x);
  }
  return out;
}

// Warning: This is not production quality code. Range checks on input and
// output data are missing, and maliciously formed input can cause undefined
// behavior by shifting too far.
void leb128_decode(const uint8_t *in, uint64_t *out, size_t count) {
  while (count-- > 0) {
    uint8_t byte = *in++;
    if (LIKELY(byte < 128)) {
      *out++ = byte;
      continue;
    }
    uint64_t value = byte & 0x7f;
    unsigned shift = 7;
    do {
      byte = *in++;
      value |= uint64_t(byte & 0x7f) << shift;
      shift += 7;
    } while (byte >= 128);
    *out++ = value;
  }
}

const codec_descriptor leb128_codec = {
    .name = "LEB128", .encoder = leb128_encode2, .decoder = leb128_decode,
};
