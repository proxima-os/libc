// Copyright 2018 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.
#ifndef RYU_D2S_INTRINSICS_H
#define RYU_D2S_INTRINSICS_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static inline uint64_t umul128(const uint64_t a, const uint64_t b, uint64_t* const productHi) {
  // The casts here help MSVC to avoid calls to the __allmul library function.
  const uint32_t aLo = (uint32_t)a;
  const uint32_t aHi = (uint32_t)(a >> 32);
  const uint32_t bLo = (uint32_t)b;
  const uint32_t bHi = (uint32_t)(b >> 32);

  const uint64_t b00 = (uint64_t)aLo * bLo;
  const uint64_t b01 = (uint64_t)aLo * bHi;
  const uint64_t b10 = (uint64_t)aHi * bLo;
  const uint64_t b11 = (uint64_t)aHi * bHi;

  const uint32_t b00Lo = (uint32_t)b00;
  const uint32_t b00Hi = (uint32_t)(b00 >> 32);

  const uint64_t mid1 = b10 + b00Hi;
  const uint32_t mid1Lo = (uint32_t)(mid1);
  const uint32_t mid1Hi = (uint32_t)(mid1 >> 32);

  const uint64_t mid2 = b01 + mid1Lo;
  const uint32_t mid2Lo = (uint32_t)(mid2);
  const uint32_t mid2Hi = (uint32_t)(mid2 >> 32);

  const uint64_t pHi = b11 + mid1Hi + mid2Hi;
  const uint64_t pLo = ((uint64_t)mid2Lo << 32) | b00Lo;

  *productHi = pHi;
  return pLo;
}

static inline uint64_t shiftright128(const uint64_t lo, const uint64_t hi, const uint32_t dist) {
  // We don't need to handle the case dist >= 64 here (see above).
  assert(dist < 64);
  assert(dist > 0);
  return (hi << (64 - dist)) | (lo >> dist);
}

static inline uint64_t div5(const uint64_t x) {
  return x / 5;
}

static inline uint64_t div10(const uint64_t x) {
  return x / 10;
}

static inline uint64_t div100(const uint64_t x) {
  return x / 100;
}

static inline uint64_t div1e8(const uint64_t x) {
  return x / 100000000;
}

static inline uint64_t div1e9(const uint64_t x) {
  return x / 1000000000;
}

static inline uint32_t mod1e9(const uint64_t x) {
  return (uint32_t) (x - 1000000000 * div1e9(x));
}

static inline uint32_t pow5Factor(uint64_t value) {
  const uint64_t m_inv_5 = 14757395258967641293u; // 5 * m_inv_5 = 1 (mod 2^64)
  const uint64_t n_div_5 = 3689348814741910323u;  // #{ n | n = 0 (mod 2^64) } = 2^64 / 5
  uint32_t count = 0;
  for (;;) {
    assert(value != 0);
    value *= m_inv_5;
    if (value > n_div_5)
      break;
    ++count;
  }
  return count;
}

// Returns true if value is divisible by 5^p.
static inline bool multipleOfPowerOf5(const uint64_t value, const uint32_t p) {
  // I tried a case distinction on p, but there was no performance difference.
  return pow5Factor(value) >= p;
}

// Returns true if value is divisible by 2^p.
static inline bool multipleOfPowerOf2(const uint64_t value, const uint32_t p) {
  assert(value != 0);
  assert(p < 64);
  // __builtin_ctzll doesn't appear to be faster here.
  return (value & ((1ull << p) - 1)) == 0;
}

// We need a 64x128-bit multiplication and a subsequent 128-bit shift.
// Multiplication:
//   The 64-bit factor is variable and passed in, the 128-bit factor comes
//   from a lookup table. We know that the 64-bit factor only has 55
//   significant bits (i.e., the 9 topmost bits are zeros). The 128-bit
//   factor only has 124 significant bits (i.e., the 4 topmost bits are
//   zeros).
// Shift:
//   In principle, the multiplication result requires 55 + 124 = 179 bits to
//   represent. However, we then shift this value to the right by j, which is
//   at least j >= 115, so the result is guaranteed to fit into 179 - 115 = 64
//   bits. This means that we only need the topmost 64 significant bits of
//   the 64x128-bit multiplication.
//
// There are several ways to do this:
// 1. Best case: the compiler exposes a 128-bit type.
//    We perform two 64x64-bit multiplications, add the higher 64 bits of the
//    lower result to the higher result, and shift by j - 64 bits.
//
//    We explicitly cast from 64-bit to 128-bit, so the compiler can tell
//    that these are only 64-bit inputs, and can map these to the best
//    possible sequence of assembly instructions.
//    x64 machines happen to have matching assembly instructions for
//    64x64-bit multiplications and 128-bit shifts.
//
// 2. Second best case: the compiler exposes intrinsics for the x64 assembly
//    instructions mentioned in 1.
//
// 3. We only have 64x64 bit instructions that return the lower 64 bits of
//    the result, i.e., we have to use plain C.
//    Our inputs are less than the full width, so we have three options:
//    a. Ignore this fact and just implement the intrinsics manually.
//    b. Split both into 31-bit pieces, which guarantees no internal overflow,
//       but requires extra work upfront (unless we change the lookup table).
//    c. Split only the first factor into 31-bit pieces, which also guarantees
//       no internal overflow, but requires extra work since the intermediate
//       results are not perfectly aligned.

// Best case: use 128-bit type.
static inline uint64_t mulShift64(const uint64_t m, const uint64_t* const mul, const int32_t j) {
  const __uint128_t b0 = ((__uint128_t) m) * mul[0];
  const __uint128_t b2 = ((__uint128_t) m) * mul[1];
  return (uint64_t) (((b0 >> 64) + b2) >> (j - 64));
}

static inline uint64_t mulShiftAll64(const uint64_t m, const uint64_t* const mul, const int32_t j,
  uint64_t* const vp, uint64_t* const vm, const uint32_t mmShift) {
//  m <<= 2;
//  uint128_t b0 = ((uint128_t) m) * mul[0]; // 0
//  uint128_t b2 = ((uint128_t) m) * mul[1]; // 64
//
//  uint128_t hi = (b0 >> 64) + b2;
//  uint128_t lo = b0 & 0xffffffffffffffffull;
//  uint128_t factor = (((uint128_t) mul[1]) << 64) + mul[0];
//  uint128_t vpLo = lo + (factor << 1);
//  *vp = (uint64_t) ((hi + (vpLo >> 64)) >> (j - 64));
//  uint128_t vmLo = lo - (factor << mmShift);
//  *vm = (uint64_t) ((hi + (vmLo >> 64) - (((uint128_t) 1ull) << 64)) >> (j - 64));
//  return (uint64_t) (hi >> (j - 64));
  *vp = mulShift64(4 * m + 2, mul, j);
  *vm = mulShift64(4 * m - 1 - mmShift, mul, j);
  return mulShift64(4 * m, mul, j);
}

#endif // RYU_D2S_INTRINSICS_H
