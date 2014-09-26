/*
 * Copyright (C) 2012, 2013, 2014
 *     Dale Weiler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "gmqcc.h"
#include <limits.h>
#include <string.h>

#if defined(_MSC_VER)
#   define HASH_ROTL32(X, Y) _rotl((X), (Y))
#elif defined (__GNUC__) && (defined(__i386__) || defined(__amd64__))
    static GMQCC_FORCEINLINE uint32_t hash_rotl32(volatile uint32_t x, int8_t r) {
        __asm__ __volatile__ ("roll %1,%0" : "+r"(x) : "c"(r));
        return x;
    }
#   define HASH_ROTL32(X, Y) hash_rotl32((volatile uint32_t)(X), (Y))
#else
#   define HASH_ROTL32(X, Y) (((X) << (Y)) | ((X) >> (32 - (Y))))
#endif

/*
 * This is a version of the Murmur3 hashing function optimized for various
 * compilers/architectures. It uses the traditional Murmur2 mix staging
 * but fixes the mix staging inner loops.
 *
 * Murmur 2 contains an inner loop such as:
 * while (l >= 4) {
 *      u32 k = *(u32*)d;
 *      k *= m;
 *      k ^= k >> r;
 *      k *= m;
 *
 *      h *= m;
 *      h ^= k;
 *      d += 4;
 *      l -= 4;
 * }
 *
 * The two u32s that form the key are the same value for x
 * this premix stage will perform the same results for both values. Unrolled
 * this produces just:
 *  x *= m;
 *  x ^= x >> r;
 *  x *= m;
 *
 *  h *= m;
 *  h ^= x;
 *  h *= m;
 *  h ^= x;
 *
 * This appears to be fine, except what happens when m == 1? well x
 * cancels out entierly, leaving just:
 *  x ^= x >> r;
 *  h ^= x;
 *  h ^= x;
 *
 * So all keys hash to the same value, but how often does m == 1?
 * well, it turns out testing x for all possible values yeilds only
 * 172,013,942 unique results instead of 2^32. So nearly ~4.6 bits
 * are cancelled out on average!
 *
 * This means we have a 14.5% higher chance of collision. This is where
 * Murmur3 comes in to save the day.
 */
static GMQCC_FORCEINLINE uint32_t hash_murmur_mix32(uint32_t hash) {
    hash ^= hash >> 16;
    hash *= 0x85EBCA6B;
    hash ^= hash >> 13;
    hash *= 0xC2B2AE35;
    hash ^= hash >> 16;
    return hash;
}

/*
 * These constants were calculated with SMHasher to determine the best
 * case senario for Murmur3:
 *  http://code.google.com/p/smhasher/
 */
#define HASH_MURMUR_MASK1 0xCC9E2D51
#define HASH_MURMUR_MASK2 0x1B873593
#define HASH_MURMUR_SEED  0x9747B28C

#if PLATFORM_BYTE_ORDER == GMQCC_BYTE_ORDER_LITTLE
#   define HASH_MURMUR_SAFEREAD(PTR) (*((uint32_t*)(PTR)))
#elif PLATFORM_BYTE_ORDER == GMQCC_BYTE_ORDER_BIG
#   if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR >= 3))
#       define HASH_MURMUR_SAFEREAD(PTR) (__builtin_bswap32(*((uint32_t*)(PTR))))
#   endif
#endif
/* Process individual bytes at this point since the endianess isn't known. */
#ifndef HASH_MURMUR_SAFEREAD
#   define HASH_MURMUR_SAFEREAD(PTR) ((PTR)[0] | (PTR)[1] << 8 | (PTR)[2] << 16 | (PTR)[3] << 24)
#endif

#define HASH_MURMUR_BLOCK(H, K)                        \
    do {                                               \
        K *= HASH_MURMUR_MASK1;                        \
        K  = HASH_ROTL32(K, 15);                       \
        K *= HASH_MURMUR_MASK2;                        \
        H ^= K;                                        \
        H  = HASH_ROTL32(H, 13);                       \
        H  = H * 5 + 0xE6546B64;                       \
    } while (0)
#define HASH_MURMUR_BYTES(COUNT, H, C, N, PTR, LENGTH) \
    do {                                               \
        int i = COUNT;                                 \
        while (i--) {                                  \
            C = C >> 8 | *PTR++ << 24;                 \
            N++;                                       \
            LENGTH--;                                  \
            if (N == 4) {                              \
                HASH_MURMUR_BLOCK(H, C);               \
                N = 0;                                 \
            }                                          \
        }                                              \
    } while (0)
#define HASH_MURMUR_TAIL(P, Z, H, C, N, PTR, LEN)      \
    do {                                               \
        LEN -= LEN/4*4;                                \
        HASH_MURMUR_BYTES(LEN, H, C, N, PTR, LEN);     \
        *P = H;                                        \
        *Z = ((C) & ~0xFF) | (N);                      \
    } while (0)

#if PLATFORM_BYTE_ORDER == GMQCC_BYTE_ORDER_LITTLE
static GMQCC_FORCEINLINE void hash_murmur_process(uint32_t *ph1, uint32_t *carry, const void *key, int length) {
    uint32_t h1 = *ph1;
    uint32_t c  = *carry;

    const uint8_t *ptr = (uint8_t*)key;
    const uint8_t *end;

    int n  = c & 3;
    int it = (4 - n) & 3;
    if (it && it <= length)
        HASH_MURMUR_BYTES(it, h1, c, n, ptr, length);

    end = ptr + length/4*4;
    for (; ptr < end; ptr += 4) {
        uint32_t k1 = HASH_MURMUR_SAFEREAD(ptr);
        HASH_MURMUR_BLOCK(h1, k1);
    }
    HASH_MURMUR_TAIL(ph1, carry, h1, c, n, ptr, length);
}
#else
static GMQCC_FORCEINLINE void hash_murmur_process(uint32_t *ph1, uint32_t *carry, const void *key, int length) {
    uint32_t k1;
    uint32_t h1 = *ph1;
    uint32_t c  = *carry;

    const uint8_t *ptr = (uint8_t*)key;
    const uint8_t *end;

    int n  = c & 3;
    int it = -(long)ptr & 3;
    if (it && it <= length)
        HASH_MURMUR_BYTES(it, h1, c, n, ptr, length);

    end = ptr + length / 4 * 4;
    switch (n) {
        case 0:
            for (; ptr < end; ptr += 4) {
                k1 = HASH_MURMUR_SAFEREAD(ptr);
                HASH_MURMUR_BLOCK(h1, k1);
            }
            break;

#       define NEXT(N, RIGHT, LEFT)                  \
            case N:                                  \
                for (; ptr < end; ptr += 4) {        \
                    k1  = c >> RIGHT;                \
                    c   = HASH_MURMUR_SAFEREAD(ptr); \
                    k1 |= c << LEFT;                 \
                    HASH_MURMUR_BLOCK(h1, k1);       \
                }                                    \
                break
        NEXT(1, 24, 8);
        NEXT(2, 16, 16);
        NEXT(3, 8,  24);
        #undef NEXT
    }
    HASH_MURMUR_TAIL(ph1, carry, h1, c, n, ptr, length);
}
#endif

static GMQCC_FORCEINLINE uint32_t hash_murmur_result(uint32_t hash, uint32_t carry, size_t length) {
    uint32_t k1;
    int n = carry & 3;
    if (GMQCC_LIKELY(n)) {
        k1    = carry >> (4 - n) * 8;
        k1   *= HASH_MURMUR_MASK1;
        k1    = HASH_ROTL32(k1, 15);
        k1   *= HASH_MURMUR_MASK2;
        hash ^= k1;
    }
    hash ^= length;
    hash  = hash_murmur_mix32(hash);

    return hash;
}

static GMQCC_FORCEINLINE uint32_t hash_murmur(const void *GMQCC_RESTRICT key, size_t length) {
    uint32_t hash  = HASH_MURMUR_SEED;
    uint32_t carry = 0;
    hash_murmur_process(&hash, &carry, key, length);
    return hash_murmur_result(hash, carry, length);
}

size_t hash(const char *key) {
#if 0
    const char   *s = key;
    const char   *a = s;
    const size_t *w;

    for (; (uintptr_t)s % sizeof(size_t); s++)
        if (!*s)
            return hash_murmur((const void *)key, s-a);

    for (w = (const size_t*)s; !((*w-(size_t)-1/UCHAR_MAX) & ~*w & ((size_t)-1/UCHAR_MAX) * (UCHAR_MAX / 2 + 1)); w++);
    for (s = (const char *)w; *s; s++);

    return hash_murmur((const void *)key, s-a);
#else
    return hash_murmur((const void *)key, strlen(key));
#endif
}

#undef HASH_ROTL32
#undef HASH_MURMUR_MASK1
#undef HASH_MURMUR_MASK2
#undef HASH_MURMUR_SEED
#undef HASH_MURMUR_SAFEREAD
#undef HASH_MURMUR_BLOCK
#undef HASH_MURMUR_BYTES
#undef HASH_MURMUR_TAIL
