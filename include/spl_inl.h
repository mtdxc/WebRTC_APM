/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This header file includes the inline functions in
// the fix point signal processing library.

#ifndef COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_SPL_INL_H_
#define COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_SPL_INL_H_

#include "signal_processing_library.h"

extern const int8_t kWebRtcSpl_CountLeadingZeros32_Table[64];

// Don't call this directly except in tests!
static __inline int WebRtcSpl_CountLeadingZeros32_NotBuiltin(uint32_t n) {
    // Normalize n by rounding up to the nearest number that is a sequence of 0
    // bits followed by a sequence of 1 bits. This number has the same number of
    // leading zeros as the original n. There are exactly 33 such values.
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;

    // Multiply the modified n with a constant selected (by exhaustive search)
    // such that each of the 33 possible values of n give a product whose 6 most
    // significant bits are unique. Then look up the answer in the table.
    return kWebRtcSpl_CountLeadingZeros32_Table[(n * 0x8c0b2891) >> 26];
}


// Returns the number of leading zero bits in the argument.
static __inline int WebRtcSpl_CountLeadingZeros32(uint32_t n) {
#ifdef __GNUC__
    assert(sizeof(unsigned int) == sizeof(uint32_t));
    return n == 0 ? 32 : __builtin_clz(n);
#else
    return WebRtcSpl_CountLeadingZeros32_NotBuiltin(n);
#endif
}

#ifdef WEBRTC_ARCH_ARM_V7
#include "spl_inl_armv7.h"
#else

#if defined(MIPS32_LE)
#include "spl_inl_mips.h"
#endif

#if !defined(MIPS32_LE)

static __inline int16_t WebRtcSpl_GetSizeInBits(uint32_t n) {
    return 32 - WebRtcSpl_CountLeadingZeros32(n);
}

// Return the number of steps a can be left-shifted without overflow,
// or 0 if a == 0.
static __inline int16_t WebRtcSpl_NormW32(int32_t a) {
    return a == 0 ? 0 : WebRtcSpl_CountLeadingZeros32(a < 0 ? ~a : a) - 1;
}

// Return the number of steps a can be left-shifted without overflow,
// or 0 if a == 0.
static __inline int16_t WebRtcSpl_NormU32(uint32_t a) {
    return a == 0 ? 0 : WebRtcSpl_CountLeadingZeros32(a);
}

// Return the number of steps a can be left-shifted without overflow,
// or 0 if a == 0.
static __inline int16_t WebRtcSpl_NormW16(int16_t a) {
    const int32_t a32 = a;
    return a == 0 ? 0 : WebRtcSpl_CountLeadingZeros32(a < 0 ? ~a32 : a32) - 17;
}

static __inline int16_t WebRtcSpl_SatW32ToW16(int32_t value32) {
    int16_t out16 = (int16_t) value32;

    if (value32 > 32767)
        out16 = 32767;
    else if (value32 < -32768)
        out16 = -32768;

    return out16;
}

static __inline int32_t WebRtcSpl_AddSatW32(int32_t a, int32_t b) {
    // Do the addition in unsigned numbers, since signed overflow is undefined
    // behavior.
    const int32_t sum = (int32_t) ((uint32_t) a + (uint32_t) b);

    // a + b can't overflow if a and b have different signs. If they have the
    // same sign, a + b also has the same sign iff it didn't overflow.
    if ((a < 0) == (b < 0) && (a < 0) != (sum < 0)) {
        // The direction of the overflow is obvious from the sign of a + b.
        return sum < 0 ? INT32_MAX : INT32_MIN;
    }
    return sum;
}

static __inline int16_t WebRtcSpl_AddSatW16(int16_t a, int16_t b) {
    return WebRtcSpl_SatW32ToW16((int32_t) a + (int32_t) b);
}


#endif  // #if !defined(MIPS32_LE)

#endif  // WEBRTC_ARCH_ARM_V7

#endif  // COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_SPL_INL_H_
