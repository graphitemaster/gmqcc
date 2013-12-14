/*
 * Copyright (C) 2012, 2013
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

/*
 * This is a version of the Murmur3 hashing function optimized for various
 * compilers/architectures. It uses the traditional Murmur2 mix stagin
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

/*
 * Some rotation tricks:
 *  MSVC one shaves off six instructions, where GCC optimized one for
 *  x86 and amd64 shaves off four instructions. Native methods are often
 *  optimized rather well at -O3, but not at -O2.
 */
#if defined(_MSC_VER)
#   define HASH_ROTL32(X, Y) _rotl((X), (Y))
#else
static GMQCC_FORCEINLINE uint32_t hash_rotl32(volatile uint32_t x, int8_t r) {
#if defined (__GNUC__) && (defined(__i386__) || defined(__amd64__))
    __asm__ __volatile__ ("roll %1,%0" : "+r"(x) : "c"(r));
    return x;
#else /* ! (defined(__GNUC__) && (defined(__i386__) || defined(__amd64__))) */
    return (x << r) | (x >> (32 - r));
#endif
}
#   define HASH_ROTL32(X, Y) hash_rotl32((volatile uint32_t)(X), (Y))
#endif /* !(_MSC_VER) */

static GMQCC_FORCEINLINE uint32_t hash_mix32(uint32_t hash) {
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
#define HASH_MASK1 0xCC9E2D51
#define HASH_MASK2 0x1B873593
#define HASH_SEED  0x9747B28C

#if PLATFORM_BYTE_ORDER == GMQCC_BYTE_ORDER_LITTLE
#   define HASH_NATIVE_SAFEREAD(PTR) (*((uint32_t*)(PTR)))
#elif PLATFORM_BYTE_ORDER == GMQCC_BYTE_ORDER_BIG
#   if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR >= 3))
#       define HASH_NATIVE_SAFEREAD(PTR) (__builtin_bswap32(*((uint32_t*)(PTR))))
#   endif
#endif
/* Process individual bytes at this point since the endianess isn't known. */
#ifndef HASH_NATIVE_SAFEREAD
#   define HASH_NATIVE_SAFEREAD(PTR) ((PTR)[0] | (PTR)[1] << 8 | (PTR)[2] << 16 | (PTR)[3] << 24)
#endif

#define HASH_NATIVE_BLOCK(H, K)                        \
    do {                                               \
        K *= HASH_MASK1;                               \
        K  = HASH_ROTL32(K, 15);                       \
        K *= HASH_MASK2;                               \
        H ^= K;                                        \
        H  = HASH_ROTL32(H, 13);                       \
        H  = H * 5 + 0xE6546B64;                       \
    } while (0)

#define HASH_NATIVE_BYTES(COUNT, H, C, N, PTR, LENGTH) \
    do {                                               \
        int i = COUNT;                                 \
        while (i--) {                                  \
            C = C >> 8 | *PTR++ << 24;                 \
            N++;                                       \
            LENGTH--;                                  \
            if (N == 4) {                              \
                HASH_NATIVE_BLOCK(H, C);               \
                N = 0;                                 \
            }                                          \
        }                                              \
    } while (0)

/*
 * Highly unrolled at per-carry bit granularity instead of per-block granularity. This will achieve the
 * highest possible instruction level parallelism.
 */
static GMQCC_FORCEINLINE void hash_native_process(uint32_t *ph1, uint32_t *carry, const void *key, int length) {
    uint32_t h1 = *ph1;
    uint32_t c  = *carry;

    const uint8_t *ptr = (uint8_t*)key;
    const uint8_t *end;

    /* carry count from low 2 bits of carry value */
    int n = c & 3;

    /*
     * Unaligned word accesses are safe in LE. Thus we can obtain a little
     * more speed.
     */
#   if PLATFORM_BYTE_ORDER == GMQCC_BYTE_ORDER_LITTLE
    /* Consume carry bits */
    int it = (4 - n) & 3;
    if (it && it <= length)
        HASH_NATIVE_BYTES(it, h1, c, n, ptr, length);

    /* word size chunk consumption */
    end = ptr + length/4*4;
    for (; ptr < end; ptr += 4) {
        uint32_t k1 = HASH_NATIVE_SAFEREAD(ptr);
        HASH_NATIVE_BLOCK(h1, k1);
    }
#   else
    /*
     * Unsafe to assume unaligned word accesses. Thus we'll need to consume
     * to alignment then process in aligned block chunks.
     */
    uint32_t k1;
    int it = -(long)ptr & 3;
    if (it && it <= length)
        HASH_NATIVE_BYTES(it, h1, c, n, ptr, length);

    /*
     * Alignment has been reached, deal with aligned blocks, specializing for
     * all possible carry counts.
     */
    end = ptr + length / 4 * 4;
    switch (n) {
        case 0:
            for (; ptr < end; ptr += 4) {
                k1 = HASH_NATIVE_SAFEREAD(ptr);
                HASH_NATIVE_BLOCK(h1, k1);
            }
            break;

        case 1:
            for (; ptr < end; ptr += 4) {
                k1  = c >> 24;
                c   = HASH_NATIVE_SAFEREAD(ptr);
                k1 |= c << 8;
                HASH_NATIVE_BLOCK(h1, k1);
            }
            break;

        case 2:
            for (; ptr < end; ptr += 4) {
                k1  = c >> 16;
                c   = HASH_NATIVE_SAFEREAD(ptr);
                k1 |= c << 16;
                HASH_NATIVE_BLOCK(h1, k1);
            }
            break;

        case 3:
            for (; ptr < end; ptr += 4) {
                k1  = c >> 8;
                c   = HASH_NATIVE_SAFEREAD(ptr);
                k1 |= c << 24;
                HASH_NATIVE_BLOCK(h1, k1);
            }
            break;
    }
#endif /* misaligned reads */

    /*
     * Advanced over 32-bit chunks, this can possibly leave 1..3 bytes of
     * additional trailing content to process.
     */
    length -= length/4*4;

    HASH_NATIVE_BYTES(length, h1, c, n, ptr, length);

    *ph1   = h1;
    *carry = (c & ~0xFF) | n;
}

static GMQCC_FORCEINLINE uint32_t hash_native_result(uint32_t hash, uint32_t carry, size_t length) {
    uint32_t k1;
    int n = carry & 3;
    if (GMQCC_LIKELY(n)) {
        k1    = carry >> (4 - n) * 8;
        k1   *= HASH_MASK1;
        k1    = HASH_ROTL32(k1, 15);
        k1   *= HASH_MASK2;
        hash ^= k1;
    }
    hash ^= length;
    hash  = hash_mix32(hash);

    return hash;
}

static GMQCC_FORCEINLINE uint32_t hash_native(const void *GMQCC_RESTRICT key, size_t length) {
    uint32_t hash  = HASH_SEED;
    uint32_t carry = 0;

    /* Seperate calls for inliner to deal with */
    hash_native_process(&hash, &carry, key, length);
    return hash_native_result(hash, carry, length);
}

/*
 * Inline assembly optimized SSE version for when SSE is present via CPUID
 * or the host compiler has __SSE__. This is about 16 cycles faster than
 * native at -O2 for GCC and 11 cycles for -O3.
 *
 *  Tested with -m32 on a Phenom II X4 with:
 *      gcc version 4.8.1 20130725 (prerelease) (GCC)
 */
#if defined(__GNUC__) && defined(__i386__)
static GMQCC_FORCEINLINE uint32_t hash_sse(const void *GMQCC_RESTRICT key, size_t length) {
    uint32_t ret;
    __asm__ __volatile__ (
        "   mov %%eax, %%ebx\n"
        "   mov %2, %%eax\n"
        "   movd %%eax, %%xmm7\n"
        "   shufps $0, %%xmm7, %%xmm7\n"
        "   mov %3, %%eax\n"
        "   movd %%eax, %%xmm6\n"
        "   shufps $0, %%xmm6, %%xmm6\n"
        "   lea (%%esi, %%ecx, 1), %%edi\n"
        "   jmp 2f\n"
        "1:\n"
        "   movaps (%%esi), %%xmm0\n"
        "   pmulld %%xmm7, %%xmm0\n"
        "   movaps %%xmm0, %%xmm2\n"
        "   pslld $15, %%xmm0\n"
        "   psrld $17, %%xmm2\n"
        "   orps %%xmm2, %%xmm0\n"
        "   pmulld %%xmm6, %%xmm0\n"
        "   movd %%xmm0, %%eax\n"
        "   xor %%eax, %%ebx\n"
        "   rol $13, %%ebx\n"
        "   imul $5, %%ebx\n"
        "   add $0xE6546B64, %%ebx\n"
        "   shufps $0x39, %%xmm0, %%xmm0\n"
        "   movd %%xmm0, %%eax\n"
        "   xor %%eax, %%ebx\n"
        "   rol $13, %%ebx\n"
        "   imul $5, %%ebx\n"
        "   add $0xE6546B64, %%ebx\n"
        "   shufps $0x39, %%xmm0, %%xmm0\n"
        "   movd %%xmm0, %%eax\n"
        "   xor %%eax, %%ebx\n"
        "   rol $13, %%ebx\n"
        "   imul $5, %%ebx\n"
        "   add $0xE6546B64, %%ebx\n"
        "   shufps $0x39, %%xmm0, %%xmm0\n"
        "   movd %%xmm0, %%eax\n"
        "   xor %%eax, %%ebx\n"
        "   rol $13, %%ebx\n"
        "   imul $5, %%ebx\n"
        "   add $0xE6546B64, %%ebx\n"
        "   add $16, %%esi\n"
        "2:\n"
        "   cmp %%esi, %%edi\n"
        "   jne 1b\n"
        "   xor %%ecx, %%ebx\n"
        "   mov %%ebx, %%eax\n"
        "   shr $16, %%ebx\n"
        "   xor %%ebx, %%eax\n"
        "   imul $0x85EBCA6b, %%eax\n"
        "   mov %%eax, %%ebx\n"
        "   shr $13, %%ebx\n"
        "   xor %%ebx, %%eax\n"
        "   imul $0xC2B2AE35, %%eax\n"
        "   mov %%eax, %%ebx\n"
        "   shr $16, %%ebx\n"
        "   xor %%ebx, %%eax\n"
        :   "=a" (ret)

        :   "a" (HASH_SEED),
            "i" (HASH_MASK1),
            "i" (HASH_MASK2),
            "S" (key),
            "c" (length)

        :   "%ebx",
            "%edi"
    );
    return ret;
}
#endif

#if defined (__GNUC__) && defined(__i386__) && !defined(__SSE__)
/*
 * Emulate MSVC _cpuid intrinsic for GCC/MinGW/Clang, this will be used
 * to determine if we should use the SSE route.
 */
static GMQCC_FORCEINLINE void hash_cpuid(int *lanes, int entry) {
    __asm__ __volatile__ (
        "cpuid"
        :   "=a"(lanes[0]),
            "=b"(lanes[1]),
            "=c"(lanes[2]),
            "=d"(lanes[3])

        :   "a" (entry)
    );
}

#endif /* !(defined(__GNUC__) && defined(__i386__) */

static uint32_t hash_entry(const void *GMQCC_RESTRICT key, size_t length) {
/*
 * No host SSE instruction set assumed do runtime test instead. This
 * is for MinGW32 mostly which doesn't define SSE.
 */
#if defined (__GNUC__) && defined(__i386__) && !defined(__SSE__)
    static bool memoize = false;
    static bool sse     = false;

    if (GMQCC_UNLIKELY(!memoize)) {
        /*
         * Only calculate SSE one time, thus it's unlikely that this branch
         * is taken more than once.
         */
        static int lanes[4];
        hash_cpuid(lanes, 0);
        /*
         * It's very likely that lanes[0] will contain a value unless it
         * isn't a modern x86.
         */
        if (GMQCC_LIKELY(*lanes >= 1))
            sse = (lanes[3] & ((int)1 << 25)) != 0;
        memoize = true;
    }

    return (GMQCC_LIKELY(sse))
                ? hash_sse(key, length);
                : hash_native(key, length);
/*
 * Same as above but this time host compiler was defined with SSE support.
 * This handles MinGW32 builds for i686+
 */
#elif defined (__GNUC__) && defined(__i386__) && defined(__SSE__)
    return hash_sse(key, length);
#else
    /*
     * Go the native route which itself is highly optimized as well for
     * unaligned load/store when dealing with LE.
     */
    return hash_native(key, length);
#endif
}

#define HASH_LEN_ALIGN      (sizeof(size_t))
#define HASH_LEN_ONES       ((size_t)-1/UCHAR_MAX)
#define HASH_LEN_HIGHS      (HASH_LEN_ONES * (UCHAR_MAX / 2 + 1))
#define HASH_LEN_HASZERO(X) (((X)-HASH_LEN_ONES) & ~(X) & HASH_LEN_HIGHS)

size_t hash(const char *key) {
    const char   *s = key;
    const char   *a = s;
    const size_t *w;

    /* Align for fast staging */
    for (; (uintptr_t)s % HASH_LEN_ALIGN; s++) {
        /* Quick stage if terminated before alignment */
        if (!*s)
            return hash_entry(key, s-a);
    }

    /*
     * Efficent staging of words for string length calculation, this is
     * faster than ifunc resolver of strlen call.
     *
     * On a x64 this becomes literally two masks, and a quick skip through
     * bytes along the string with the following masks:
     *      movabs $0xFEFEFEFEFEFEFEFE,%r8
     *      movabs $0x8080808080808080,%rsi
     */
    for (w = (const void *)s; !HASH_LEN_HASZERO(*w); w++);
    for (s = (const void *)w; *s; s++);

    return hash_entry(key, s-a);
}

#undef HASH_LEN_HASZERO
#undef HASH_LEN_HIGHS
#undef HASH_LEN_ONES
#undef HASH_LEN_ALIGN
#undef HASH_SEED
#undef HASH_MASK2
#undef HASH_MASK1
#undef HASH_ROTL32
#undef HASH_NATIVE_BLOCK
#undef HASH_NATIVE_BYTES
#undef HASH_NATIVE_SAFEREAD
