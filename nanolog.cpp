// Copyright 2019 Stanford Univeristy
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

// This implements the interface to NanoLog's compression algorithm

#include "compiler.h"
#include "varint.h"
#include "NanoLog/runtime/Packer.h"

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
uint64_t nanolog_encode(const vector<uint64_t> &in, uint8_t *out_orig)
{
    using namespace BufferUtils;

    char *out = reinterpret_cast<char*>(out_orig);
    uint64_t roundedSize = 2*(in.size()/2);

    for (int i = 0; i < roundedSize; i += 2) {
        TwoNibbles *nibbles = reinterpret_cast<TwoNibbles*>(out);
        ++out;

        nibbles->first = pack<uint64_t>(&out, in[i]);
        nibbles->second = pack<uint64_t>(&out, in[i+1]);
    }

    // Last element was left out
    if (roundedSize != in.size()) {
        TwoNibbles *nibbles = reinterpret_cast<TwoNibbles*>(out);
        ++out;

        nibbles->first = pack<uint64_t>(&out, in.back());
    }

    return out - reinterpret_cast<char*>(out_orig);
}

void nanolog_decode(const uint8_t *in_orig, uint64_t *out, size_t count)
{
    using namespace BufferUtils;

    size_t roundedSize = 2*(count/2);
    const char *in = reinterpret_cast<const char*>(in);

    for (int i = 0; i < roundedSize; i += 2) {
        const TwoNibbles *nibbles = reinterpret_cast<const TwoNibbles*>(in);
        ++in;

        *out = unpack<uint64_t>(&in, nibbles->first);
        ++out;

        *out = unpack<uint64_t>(&in, nibbles->second);
        ++out;
    }

    // Last element was left out
    if (roundedSize != count) {
        TwoNibbles *nibbles = reinterpret_cast<TwoNibbles*>(out);
        ++out;

        nibbles->first = unpack<uint64_t>(&in, nibbles->first);
    }
}

const codec_descriptor nanolog_codec = {
    .name = "NanoLog", .encoder = nanolog_encode, .decoder = nanolog_decode,
};