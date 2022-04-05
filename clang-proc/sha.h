/*
 Copyright (C) 2008 The Android Open Source Project

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// Copyright 2008 Google Inc. All Rights Reserved.
// Author: mschilder@google.com (Marius Schilder)

#ifndef _EMBEDDED_SHA_H_
#define _EMBEDDED_SHA_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct SHA_CTX {
  uint64_t count;
  uint32_t state[5];
  union {
    uint8_t b[64];
    uint32_t w[16];
  } buf;
} SHA_CTX;

void SHA_init(SHA_CTX* ctx);
void SHA_update(SHA_CTX* ctx, const void* data, int len);
const uint8_t* SHA_final(SHA_CTX* ctx);

// Convenience method. Returns digest parameter value.
const uint8_t* SHA(const void* data, int len, uint8_t* digest);

#define SHA_DIGEST_SIZE 20

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // _EMBEDDED_SHA_H_
