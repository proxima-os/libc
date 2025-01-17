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

#include "ryu/ryu_low_level.h"
#include "ryu/common.h"
#include "ryu/d2s_full_table.h"
#include "ryu/d2s_intrinsics.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#define DOUBLE_MANTISSA_BITS 52
#define DOUBLE_EXPONENT_BITS 11
#define DOUBLE_BIAS 1023

// A floating decimal representing m * 10^e.
typedef struct floating_decimal_64 {
    uint64_t mantissa;
    // Decimal exponent's range is -324 to 308
    // inclusive, and can fit in a short if needed.
    int32_t exponent;
} floating_decimal_64;

static inline uint32_t internal_bias(const uint32_t mantissaBits, const uint32_t exponentBits) {
    return (1u << (exponentBits - 1)) - 1 + mantissaBits + 2;
}

static inline floating_decimal_64 d2d(const uint64_t ieeeMantissa, const uint32_t ieeeExponent, const uint32_t bias) {
    const int32_t e2 = (ieeeExponent == 0 ? 1 : ieeeExponent) - bias;
    const uint64_t m2 = ieeeMantissa;
    const bool even = (m2 & 1) == 0;
    const bool acceptBounds = even;

    // Step 2: Determine the interval of valid decimal representations.
    const uint64_t mv = 4 * m2;
    // Implicit bool -> int conversion. True is 1, false is 0.
    const uint32_t mmShift = m2 != (1ull << DOUBLE_MANTISSA_BITS) || ieeeExponent <= 1;
    // We would compute mp and mm like this:
    // uint64_t mp = 4 * m2 + 2;
    // uint64_t mm = mv - 1 - mmShift;

    // Step 3: Convert to a decimal power base using 128-bit arithmetic.
    uint64_t vr, vp, vm;
    int32_t e10;
    bool vmIsTrailingZeros = false;
    bool vrIsTrailingZeros = false;
    if (e2 >= 0) {
        // I tried special-casing q == 0, but there was no effect on performance.
        // This expression is slightly faster than max(0, log10Pow2(e2) - 1).
        const uint32_t q = log10Pow2(e2) - (e2 > 3);
        e10 = (int32_t)q;
        const int32_t k = DOUBLE_POW5_INV_BITCOUNT + pow5bits((int32_t)q) - 1;
        const int32_t i = -e2 + (int32_t)q + k;
        vr = mulShiftAll64(m2, DOUBLE_POW5_INV_SPLIT[q], i, &vp, &vm, mmShift);
        if (q <= 21) {
            // This should use q <= 22, but I think 21 is also safe. Smaller values
            // may still be safe, but it's more difficult to reason about them.
            // Only one of mp, mv, and mm can be a multiple of 5, if any.
            const uint32_t mvMod5 = ((uint32_t)mv) - 5 * ((uint32_t)div5(mv));
            if (mvMod5 == 0) {
                vrIsTrailingZeros = multipleOfPowerOf5(mv, q);
            } else if (acceptBounds) {
                // Same as min(e2 + (~mm & 1), pow5Factor(mm)) >= q
                // <=> e2 + (~mm & 1) >= q && pow5Factor(mm) >= q
                // <=> true && pow5Factor(mm) >= q, since e2 >= q.
                vmIsTrailingZeros = multipleOfPowerOf5(mv - 1 - mmShift, q);
            } else {
                // Same as min(e2 + 1, pow5Factor(mp)) >= q.
                vp -= multipleOfPowerOf5(mv + 2, q);
            }
        }
    } else {
        // This expression is slightly faster than max(0, log10Pow5(-e2) - 1).
        const uint32_t q = log10Pow5(-e2) - (-e2 > 1);
        e10 = (int32_t)q + e2;
        const int32_t i = -e2 - (int32_t)q;
        const int32_t k = pow5bits(i) - DOUBLE_POW5_BITCOUNT;
        const int32_t j = (int32_t)q - k;
        vr = mulShiftAll64(m2, DOUBLE_POW5_SPLIT[i], j, &vp, &vm, mmShift);
        if (q <= 1) {
            // {vr,vp,vm} is trailing zeros if {mv,mp,mm} has at least q trailing 0 bits.
            // mv = 4 * m2, so it always has at least two trailing 0 bits.
            vrIsTrailingZeros = true;
            if (acceptBounds) {
                // mm = mv - 1 - mmShift, so it has 1 trailing 0 bit iff mmShift == 1.
                vmIsTrailingZeros = mmShift == 1;
            } else {
                // mp = mv + 2, so it always has at least one trailing 0 bit.
                --vp;
            }
        } else if (q < 63) { // TODO(ulfjack): Use a tighter bound here.
            // We want to know if the full product has at least q trailing zeros.
            // We need to compute min(p2(mv), p5(mv) - e2) >= q
            // <=> p2(mv) >= q && p5(mv) - e2 >= q
            // <=> p2(mv) >= q (because -e2 >= q)
            vrIsTrailingZeros = multipleOfPowerOf2(mv, q);
        }
    }

    // Step 4: Find the shortest decimal representation in the interval of valid representations.
    int32_t removed = 0;
    uint8_t lastRemovedDigit = 0;
    uint64_t output;
    // On average, we remove ~2 digits.
    if (vmIsTrailingZeros || vrIsTrailingZeros) {
        // General case, which happens rarely (~0.7%).
        for (;;) {
            const uint64_t vpDiv10 = div10(vp);
            const uint64_t vmDiv10 = div10(vm);
            if (vpDiv10 <= vmDiv10) {
                break;
            }
            const uint32_t vmMod10 = ((uint32_t)vm) - 10 * ((uint32_t)vmDiv10);
            const uint64_t vrDiv10 = div10(vr);
            const uint32_t vrMod10 = ((uint32_t)vr) - 10 * ((uint32_t)vrDiv10);
            vmIsTrailingZeros &= vmMod10 == 0;
            vrIsTrailingZeros &= lastRemovedDigit == 0;
            lastRemovedDigit = (uint8_t)vrMod10;
            vr = vrDiv10;
            vp = vpDiv10;
            vm = vmDiv10;
            ++removed;
        }

        if (vmIsTrailingZeros) {
            for (;;) {
                const uint64_t vmDiv10 = div10(vm);
                const uint32_t vmMod10 = ((uint32_t)vm) - 10 * ((uint32_t)vmDiv10);
                if (vmMod10 != 0) {
                    break;
                }
                const uint64_t vpDiv10 = div10(vp);
                const uint64_t vrDiv10 = div10(vr);
                const uint32_t vrMod10 = ((uint32_t)vr) - 10 * ((uint32_t)vrDiv10);
                vrIsTrailingZeros &= lastRemovedDigit == 0;
                lastRemovedDigit = (uint8_t)vrMod10;
                vr = vrDiv10;
                vp = vpDiv10;
                vm = vmDiv10;
                ++removed;
            }
        }

        if (vrIsTrailingZeros && lastRemovedDigit == 5 && vr % 2 == 0) {
            // Round even if the exact number is .....50..0.
            lastRemovedDigit = 4;
        }
        // We need to take vr + 1 if vr is outside bounds or we need to round up.
        output = vr + ((vr == vm && (!acceptBounds || !vmIsTrailingZeros)) || lastRemovedDigit >= 5);
    } else {
        // Specialized for the common case (~99.3%). Percentages below are relative to this.
        bool roundUp = false;
        const uint64_t vpDiv100 = div100(vp);
        const uint64_t vmDiv100 = div100(vm);
        if (vpDiv100 > vmDiv100) { // Optimization: remove two digits at a time (~86.2%).
            const uint64_t vrDiv100 = div100(vr);
            const uint32_t vrMod100 = ((uint32_t)vr) - 100 * ((uint32_t)vrDiv100);
            roundUp = vrMod100 >= 50;
            vr = vrDiv100;
            vp = vpDiv100;
            vm = vmDiv100;
            removed += 2;
        }
        // Loop iterations below (approximately), without optimization above:
        // 0: 0.03%, 1: 13.8%, 2: 70.6%, 3: 14.0%, 4: 1.40%, 5: 0.14%, 6+: 0.02%
        // Loop iterations below (approximately), with optimization above:
        // 0: 70.6%, 1: 27.8%, 2: 1.40%, 3: 0.14%, 4+: 0.02%
        for (;;) {
            const uint64_t vpDiv10 = div10(vp);
            const uint64_t vmDiv10 = div10(vm);
            if (vpDiv10 <= vmDiv10) {
                break;
            }
            const uint64_t vrDiv10 = div10(vr);
            const uint32_t vrMod10 = ((uint32_t)vr) - 10 * ((uint32_t)vrDiv10);
            roundUp = vrMod10 >= 5;
            vr = vrDiv10;
            vp = vpDiv10;
            vm = vmDiv10;
            ++removed;
        }

        // We need to take vr + 1 if vr is outside bounds or we need to round up.
        output = vr + (vr == vm || roundUp);
    }
    const int32_t exp = e10 + removed;

    floating_decimal_64 fd;
    fd.exponent = exp;
    fd.mantissa = output;
    return fd;
}

struct floating_decimal generic_binary_to_decimal(
        const uint64_t ieeeBits,
        const uint32_t mantissaBits,
        const uint32_t exponentBits
) {
    assert(mantissaBits < 64);
    assert(exponentBits < 64);
    assert(mantissaBits + exponentBits + 1 <= 64);

    const bool ieeeSign = ((ieeeBits >> (mantissaBits + exponentBits)) & 1) != 0;
    uint64_t ieeeMantissa = ieeeBits & ((1ull << mantissaBits) - 1);
    const uint32_t ieeeExponent = (uint32_t)((ieeeBits >> mantissaBits) & ((1u << exponentBits) - 1));
    // Case distinction; exit early for the easy cases.
    if (ieeeExponent == ((1u << exponentBits) - 1u) || (ieeeExponent == 0 && ieeeMantissa == 0)) {
        struct floating_decimal result;
        result.mantissa = 0;
        result.exponent = 0;
        result.sign = ieeeSign;
        result.type = ieeeMantissa ? RYU_NAN : (ieeeExponent ? RYU_INFINITY : RYU_VALUE);
        return result;
    }

    if (ieeeExponent != 0) {
        ieeeMantissa |= (1ull << mantissaBits);
    }

    const uint32_t bias = internal_bias(mantissaBits, exponentBits);
    const struct floating_decimal_64 v = d2d(ieeeMantissa, ieeeExponent, bias);
    struct floating_decimal result;
    result.mantissa = v.mantissa;
    result.exponent = v.exponent;
    result.sign = ieeeSign;
    result.type = RYU_VALUE;
    return result;
}
