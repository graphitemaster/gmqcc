/*
 * Copyright (C) 2012, 2013
 *     Dale Weiler
 *     Wolfgang Bumiller
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
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "gmqcc.h"

void util_debug(const char *area, const char *ms, ...) {
    va_list  va;
    if (!OPTS_OPTION_BOOL(OPTION_DEBUG))
        return;

    if (!strcmp(area, "MEM") && !OPTS_OPTION_BOOL(OPTION_MEMCHK))
        return;

    va_start(va, ms);
    con_out ("[%s] ", area);
    con_vout(ms, va);
    va_end  (va);
}

/*
 * only required if big endian .. otherwise no need to swap
 * data.
 */
#if PLATFORM_BYTE_ORDER == GMQCC_BYTE_ORDER_BIG
    static GMQCC_INLINE void util_swap16(uint16_t *d, size_t l) {
        while (l--) {
            d[l] = (d[l] << 8) | (d[l] >> 8);
        }
    }

    static GMQCC_INLINE void util_swap32(uint32_t *d, size_t l) {
        while (l--) {
            uint32_t v;
            v = ((d[l] << 8) & 0xFF00FF00) | ((d[l] >> 8) & 0x00FF00FF);
            d[l] = (v << 16) | (v >> 16);
        }
    }

    /* Some strange system doesn't like constants that big, AND doesn't recognize an ULL suffix
     * so let's go the safe way
     */
    static GMQCC_INLINE void util_swap64(uint32_t *d, size_t l) {
        /*
        while (l--) {
            uint64_t v;
            v = ((d[l] << 8) & 0xFF00FF00FF00FF00) | ((d[l] >> 8) & 0x00FF00FF00FF00FF);
            v = ((v << 16) & 0xFFFF0000FFFF0000) | ((v >> 16) & 0x0000FFFF0000FFFF);
            d[l] = (v << 32) | (v >> 32);
        }
        */
        size_t i;
        for (i = 0; i < l; i += 2) {
            uint32_t v1 = d[i];
            d[i] = d[i+1];
            d[i+1] = v1;
            util_swap32(d+i, 2);
        }
    }
#endif

void util_endianswap(void *_data, size_t length, unsigned int typesize) {
#   if PLATFORM_BYTE_ORDER == -1 /* runtime check */
    if (*((char*)&typesize))
        return;
#else
    /* prevent unused warnings */
    (void) _data;
    (void) length;
    (void) typesize;

#   if PLATFORM_BYTE_ORDER == GMQCC_BYTE_ORDER_LITTLE
        return;
#   else
        switch (typesize) {
            case 1: return;
            case 2:
                util_swap16((uint16_t*)_data, length>>1);
                return;
            case 4:
                util_swap32((uint32_t*)_data, length>>2);
                return;
            case 8:
                util_swap64((uint32_t*)_data, length>>3);
                return;

            default: exit(EXIT_FAILURE); /* please blow the fuck up! */
        }
#   endif
#endif
}

/*
 * CRC algorithms vary in the width of the polynomial, the value of said polynomial,
 * the initial value used for the register, weather the bits of each byte are reflected
 * before being processed, weather the algorithm itself feeds input bytes through the
 * register or XORs them with a byte from one end and then straight into the table, as
 * well as (but not limited to the idea of reflected versions) where the final register
 * value becomes reversed, and finally weather the value itself is used to XOR the final
 * register value.  AS such you can already imagine how painfully annoying CRCs are,
 * of course we stand to target Quake, which expects it's certian set of rules for proper
 * calculation of a CRC.
 *
 * In most traditional CRC algorithms on uses a reflected table driven method where a value
 * or register is reflected if it's bits are swapped around it's center.  For example:
 * take the bits 0101 is the 4-bit reflection of 1010, and respectfully 0011 would be the
 * reflection of 1100. Quake however expects a NON-Reflected CRC on the output, but still
 * requires a final XOR on the values (0xFFFF and 0x0000) this is a standard CCITT CRC-16
 * which I respectfully as a programmer don't agree with.
 *
 * So now you know what we target, and why we target it, despite how unsettling it may seem
 * but those are what Quake seems to request.
 */

static const uint16_t util_crc16_table[] = {
    0x0000,     0x1021,     0x2042,     0x3063,     0x4084,     0x50A5,
    0x60C6,     0x70E7,     0x8108,     0x9129,     0xA14A,     0xB16B,
    0xC18C,     0xD1AD,     0xE1CE,     0xF1EF,     0x1231,     0x0210,
    0x3273,     0x2252,     0x52B5,     0x4294,     0x72F7,     0x62D6,
    0x9339,     0x8318,     0xB37B,     0xA35A,     0xD3BD,     0xC39C,
    0xF3FF,     0xE3DE,     0x2462,     0x3443,     0x0420,     0x1401,
    0x64E6,     0x74C7,     0x44A4,     0x5485,     0xA56A,     0xB54B,
    0x8528,     0x9509,     0xE5EE,     0xF5CF,     0xC5AC,     0xD58D,
    0x3653,     0x2672,     0x1611,     0x0630,     0x76D7,     0x66F6,
    0x5695,     0x46B4,     0xB75B,     0xA77A,     0x9719,     0x8738,
    0xF7DF,     0xE7FE,     0xD79D,     0xC7BC,     0x48C4,     0x58E5,
    0x6886,     0x78A7,     0x0840,     0x1861,     0x2802,     0x3823,
    0xC9CC,     0xD9ED,     0xE98E,     0xF9AF,     0x8948,     0x9969,
    0xA90A,     0xB92B,     0x5AF5,     0x4AD4,     0x7AB7,     0x6A96,
    0x1A71,     0x0A50,     0x3A33,     0x2A12,     0xDBFD,     0xCBDC,
    0xFBBF,     0xEB9E,     0x9B79,     0x8B58,     0xBB3B,     0xAB1A,
    0x6CA6,     0x7C87,     0x4CE4,     0x5CC5,     0x2C22,     0x3C03,
    0x0C60,     0x1C41,     0xEDAE,     0xFD8F,     0xCDEC,     0xDDCD,
    0xAD2A,     0xBD0B,     0x8D68,     0x9D49,     0x7E97,     0x6EB6,
    0x5ED5,     0x4EF4,     0x3E13,     0x2E32,     0x1E51,     0x0E70,
    0xFF9F,     0xEFBE,     0xDFDD,     0xCFFC,     0xBF1B,     0xAF3A,
    0x9F59,     0x8F78,     0x9188,     0x81A9,     0xB1CA,     0xA1EB,
    0xD10C,     0xC12D,     0xF14E,     0xE16F,     0x1080,     0x00A1,
    0x30C2,     0x20E3,     0x5004,     0x4025,     0x7046,     0x6067,
    0x83B9,     0x9398,     0xA3FB,     0xB3DA,     0xC33D,     0xD31C,
    0xE37F,     0xF35E,     0x02B1,     0x1290,     0x22F3,     0x32D2,
    0x4235,     0x5214,     0x6277,     0x7256,     0xB5EA,     0xA5CB,
    0x95A8,     0x8589,     0xF56E,     0xE54F,     0xD52C,     0xC50D,
    0x34E2,     0x24C3,     0x14A0,     0x0481,     0x7466,     0x6447,
    0x5424,     0x4405,     0xA7DB,     0xB7FA,     0x8799,     0x97B8,
    0xE75F,     0xF77E,     0xC71D,     0xD73C,     0x26D3,     0x36F2,
    0x0691,     0x16B0,     0x6657,     0x7676,     0x4615,     0x5634,
    0xD94C,     0xC96D,     0xF90E,     0xE92F,     0x99C8,     0x89E9,
    0xB98A,     0xA9AB,     0x5844,     0x4865,     0x7806,     0x6827,
    0x18C0,     0x08E1,     0x3882,     0x28A3,     0xCB7D,     0xDB5C,
    0xEB3F,     0xFB1E,     0x8BF9,     0x9BD8,     0xABBB,     0xBB9A,
    0x4A75,     0x5A54,     0x6A37,     0x7A16,     0x0AF1,     0x1AD0,
    0x2AB3,     0x3A92,     0xFD2E,     0xED0F,     0xDD6C,     0xCD4D,
    0xBDAA,     0xAD8B,     0x9DE8,     0x8DC9,     0x7C26,     0x6C07,
    0x5C64,     0x4C45,     0x3CA2,     0x2C83,     0x1CE0,     0x0CC1,
    0xEF1F,     0xFF3E,     0xCF5D,     0xDF7C,     0xAF9B,     0xBFBA,
    0x8FD9,     0x9FF8,     0x6E17,     0x7E36,     0x4E55,     0x5E74,
    0x2E93,     0x3EB2,     0x0ED1,     0x1EF0
};

/* Non - Reflected */
uint16_t util_crc16(uint16_t current, const char *k, size_t len) {
    register uint16_t h = current;
    for (; len; --len, ++k)
        h = util_crc16_table[(h>>8)^((unsigned char)*k)]^(h<<8);
    return h;
}
/* Reflective Varation (for reference) */
#if 0
uint16_t util_crc16(const char *k, int len, const short clamp) {
    register uint16_t h= (uint16_t)0xFFFFFFFF;
    for (; len; --len, ++k)
        h = util_crc16_table[(h^((unsigned char)*k))&0xFF]^(h>>8);
    return (~h)%clamp;
}
#endif

size_t util_strtocmd(const char *in, char *out, size_t outsz) {
    size_t sz = 1;
    for (; *in && sz < outsz; ++in, ++out, ++sz)
        *out = (*in == '-') ? '_' : (isalpha(*in) && !isupper(*in)) ? *in + 'A' - 'a': *in;
    *out = 0;
    return sz-1;
}

size_t util_strtononcmd(const char *in, char *out, size_t outsz) {
    size_t sz = 1;
    for (; *in && sz < outsz; ++in, ++out, ++sz)
        *out = (*in == '_') ? '-' : (isalpha(*in) && isupper(*in)) ? *in + 'a' - 'A' : *in;
    *out = 0;
    return sz-1;
}

/*
 * Portable implementation of vasprintf/asprintf. Assumes vsnprintf
 * exists, otherwise compiler error.
 *
 * TODO: fix for MSVC ....
 */
int util_vasprintf(char **dat, const char *fmt, va_list args) {
    int   ret;
    int   len;
    char *tmp = NULL;

    /*
     * For visuals tido _vsnprintf doesn't tell you the length of a
     * formatted string if it overflows. However there is a MSVC
     * intrinsic (which is documented wrong) called _vcsprintf which
     * will return the required amount to allocate.
     */
    #ifdef _MSC_VER
        if ((len = _vscprintf(fmt, args)) < 0) {
            *dat = NULL;
            return -1;
        }

        tmp = (char*)mem_a(len + 1);
        if ((ret = _vsnprintf_s(tmp, len+1, len+1, fmt, args)) != len) {
            mem_d(tmp);
            *dat = NULL;
            return -1;
        }
        *dat = tmp;
        return len;
    #else
        /*
         * For everything else we have a decent conformint vsnprintf that
         * returns the number of bytes needed.  We give it a try though on
         * a short buffer, since efficently speaking, it could be nice to
         * above a second vsnprintf call.
         */
        char    buf[128];
        va_list cpy;
        va_copy(cpy, args);
        len = vsnprintf(buf, sizeof(buf), fmt, cpy);
        va_end (cpy);

        if (len < (int)sizeof(buf)) {
            *dat = util_strdup(buf);
            return len;
        }

        /* not large enough ... */
        tmp = (char*)mem_a(len + 1);
        if ((ret = vsnprintf(tmp, len + 1, fmt, args)) != len) {
            mem_d(tmp);
            *dat = NULL;
            return -1;
        }

        *dat = tmp;
        return len;
    #endif
}
int util_asprintf(char **ret, const char *fmt, ...) {
    va_list  args;
    int      read;
    va_start(args, fmt);
    read = util_vasprintf(ret, fmt, args);
    va_end  (args);

    return read;
}

/*
 * These are various re-implementations (wrapping the real ones) of
 * string functions that MSVC consideres unsafe. We wrap these up and
 * use the safe varations on MSVC.
 */
#ifdef _MSC_VER
    static char **util_strerror_allocated() {
        static char **data = NULL;
        return data;
    }

    static void util_strerror_cleanup(void) {
        size_t i;
        char  **data = util_strerror_allocated();
        for (i = 0; i < vec_size(data); i++)
            mem_d(data[i]);
        vec_free(data);
    }

    const char *util_strerror(int num) {
        char         *allocated = NULL;
        static bool   install   = false;
        static size_t tries     = 0;
        char        **vector    = util_strerror_allocated();

        /* try installing cleanup handler */
        while (!install) {
            if (tries == 32)
                return "(unknown)";

            install = !atexit(&util_strerror_cleanup);
            tries ++;
        }

        allocated = (char*)mem_a(4096); /* A page must be enough */
        strerror_s(allocated, 4096, num);
    
        vec_push(vector, allocated);
        return (const char *)allocated;
    }

    int util_snprintf(char *src, size_t bytes, const char *format, ...) {
        int      rt;
        va_list  va;
        va_start(va, format);

        rt = vsprintf_s(src, bytes, format, va);
        va_end  (va);

        return rt;
    }

    char *util_strcat(char *dest, const char *src) {
        strcat_s(dest, strlen(src), src);
        return dest;
    }

    char *util_strncpy(char *dest, const char *src, size_t num) {
        strncpy_s(dest, num, src, num);
        return dest;
    }
#else
    const char *util_strerror(int num) {
        return strerror(num);
    }

    int util_snprintf(char *src, size_t bytes, const char *format, ...) {
        int      rt;
        va_list  va;
        va_start(va, format);
        rt = vsnprintf(src, bytes, format, va);
        va_end  (va);

        return rt;
    }

    char *util_strcat(char *dest, const char *src) {
        return strcat(dest, src);
    }

    char *util_strncpy(char *dest, const char *src, size_t num) {
        return strncpy(dest, src, num);
    }

#endif /*! _MSC_VER */

/*
 * Implementation of the Mersenne twister PRNG (pseudo random numer
 * generator).  Implementation of MT19937.  Has a period of 2^19937-1
 * which is a Mersenne Prime (hence the name).
 *
 * Implemented from specification and original paper:
 * http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/ARTICLES/mt.pdf
 *
 * This code is placed in the public domain by me personally
 * (Dale Weiler, a.k.a graphitemaster).
 */

#define MT_SIZE    624
#define MT_PERIOD  397
#define MT_SPACE   (MT_SIZE - MT_PERIOD)

static uint32_t mt_state[MT_SIZE];
static size_t   mt_index = 0;

static GMQCC_INLINE void mt_generate(void) {
    /*
     * The loop has been unrolled here: the original paper and implemenation
     * Called for the following code:
     * for (register unsigned i = 0; i < MT_SIZE; ++i) {
     *     register uint32_t load;
     *     load  = (0x80000000 & mt_state[i])                 // most  significant 32nd bit
     *     load |= (0x7FFFFFFF & mt_state[(i + 1) % MT_SIZE]) // least significant 31nd bit
     *
     *     mt_state[i] = mt_state[(i + MT_PERIOD) % MT_SIZE] ^ (load >> 1);
     *
     *     if (load & 1) mt_state[i] ^= 0x9908B0DF;
     * }
     *
     * This essentially is a waste: we have two modulus operations, and
     * a branch that is executed every iteration from [0, MT_SIZE).
     *
     * Please see: http://www.quadibloc.com/crypto/co4814.htm for more
     * information on how this clever trick works.
     */
    static const uint32_t matrix[2] = {
        0x00000000,
        0x9908B0Df
    };
    /*
     * This register gives up a little more speed by instructing the compiler
     * to force these into CPU registers (they're counters for indexing mt_state
     * which we can force the compiler to generate prefetch instructions for)
     */
    register uint32_t y;
    register uint32_t i;

    /*
     * Said loop has been unrolled for MT_SPACE (226 iterations), opposed
     * to [0, MT_SIZE)  (634 iterations).
     */
    for (i = 0; i < MT_SPACE; ++i) {
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i + MT_PERIOD] ^ (y >> 1) ^ matrix[y & 1];

        i ++; /* loop unroll */

        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i + MT_PERIOD] ^ (y >> 1) ^ matrix[y & 1];
    }

    /*
     * collapsing the walls unrolled (evenly dividing 396 [632-227 = 396
     * = 2*2*3*3*11])
     */
    i = MT_SPACE;
    while (i < MT_SIZE - 1) {
        /*
         * We expand this 11 times .. manually, no macros are required
         * here. This all fits in the CPU cache.
         */
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i - MT_SPACE] ^ (y >> 1) ^ matrix[y & 1];
        ++i;
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i - MT_SPACE] ^ (y >> 1) ^ matrix[y & 1];
        ++i;
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i - MT_SPACE] ^ (y >> 1) ^ matrix[y & 1];
        ++i;
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i - MT_SPACE] ^ (y >> 1) ^ matrix[y & 1];
        ++i;
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i - MT_SPACE] ^ (y >> 1) ^ matrix[y & 1];
        ++i;
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i - MT_SPACE] ^ (y >> 1) ^ matrix[y & 1];
        ++i;
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i - MT_SPACE] ^ (y >> 1) ^ matrix[y & 1];
        ++i;
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i - MT_SPACE] ^ (y >> 1) ^ matrix[y & 1];
        ++i;
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i - MT_SPACE] ^ (y >> 1) ^ matrix[y & 1];
        ++i;
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i - MT_SPACE] ^ (y >> 1) ^ matrix[y & 1];
        ++i;
        y           = (0x80000000 & mt_state[i]) | (0x7FFFFFFF & mt_state[i + 1]);
        mt_state[i] = mt_state[i - MT_SPACE] ^ (y >> 1) ^ matrix[y & 1];
        ++i;
    }

    /* i = mt_state[623] */
    y                     = (0x80000000 & mt_state[MT_SIZE - 1]) | (0x7FFFFFFF & mt_state[MT_SIZE - 1]);
    mt_state[MT_SIZE - 1] = mt_state[MT_PERIOD - 1] ^ (y >> 1) ^ matrix[y & 1];
}

void util_seed(uint32_t value) {
    /*
     * We seed the mt_state with a LCG (linear congruential generator)
     * We're operating exactly on exactly m=32, so there is no need to
     * use modulus.
     *
     * The multipler of choice is 0x6C07865, also knows as the Borosh-
     * Niederreiter multipler used for modulus 2^32.  More can be read
     * about this in Knuth's TAOCP Volume 2, page 106.
     *
     * If you don't own TAOCP something is wrong with you :-) .. so I
     * also provided a link to the original paper by Borosh and
     * Niederreiter.  It's called "Optional Multipliers for PRNG by The
     * Linear Congruential Method" (1983).
     * http://en.wikipedia.org/wiki/Linear_congruential_generator
     *
     * From said page, it says the following:
     * "A common Mersenne twister implementation, interestingly enough
     *  used an LCG to generate seed data."
     *
     * Remarks:
     * The data we're operating on is 32-bits for the mt_state array, so
     * there is no masking required with 0xFFFFFFFF
     */
    register size_t i;

    mt_state[0] = value;
    for (i = 1; i < MT_SIZE; ++i)
        mt_state[i] = 0x6C078965 * (mt_state[i - 1] ^ mt_state[i - 1] >> 30) + i;
}

uint32_t util_rand() {
    register uint32_t y;

    /*
     * This is inlined with any sane compiler (I checked)
     * for some reason though, SubC seems to be generating invalid
     * code when it inlines this.
     */
    if (!mt_index)
        mt_generate();

    y = mt_state[mt_index];

    /* Standard tempering */
    y ^= y >> 11;              /* +7 */
    y ^= y << 7  & 0x9D2C5680; /* +4 */
    y ^= y << 15 & 0xEFC60000; /* -4 */
    y ^= y >> 18;              /* -7 */

    if(++mt_index == MT_SIZE)
         mt_index = 0;

    return y;
}
