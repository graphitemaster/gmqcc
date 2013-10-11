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
#define GMQCC_PLATFORM_HEADER
#include "gmqcc.h"
#include "platform.h"

/*
 * Initially this was handled with a table in the gmqcc.h header, but
 * much to my surprise the contents of the table was duplicated for
 * each translation unit, causing all these strings to be duplicated
 * for every .c file it was included into. This method culls back on
 * it. This is a 'utility' function because the executor also depends
 * on this for disassembled byte-code.
 */
const char *util_instr_str[VINSTR_END] = {
    "DONE",       "MUL_F",      "MUL_V",      "MUL_FV",
    "MUL_VF",     "DIV_F",      "ADD_F",      "ADD_V",
    "SUB_F",      "SUB_V",      "EQ_F",       "EQ_V",
    "EQ_S",       "EQ_E",       "EQ_FNC",     "NE_F",
    "NE_V",       "NE_S",       "NE_E",       "NE_FNC",
    "LE",         "GE",         "LT",         "GT",
    "LOAD_F",     "LOAD_V",     "LOAD_S",     "LOAD_ENT",
    "LOAD_FLD",   "LOAD_FNC",   "ADDRESS",    "STORE_F",
    "STORE_V",    "STORE_S",    "STORE_ENT",  "STORE_FLD",
    "STORE_FNC",  "STOREP_F",   "STOREP_V",   "STOREP_S",
    "STOREP_ENT", "STOREP_FLD", "STOREP_FNC", "RETURN",
    "NOT_F",      "NOT_V",      "NOT_S",      "NOT_ENT",
    "NOT_FNC",    "IF",         "IFNOT",      "CALL0",
    "CALL1",      "CALL2",      "CALL3",      "CALL4",
    "CALL5",      "CALL6",      "CALL7",      "CALL8",
    "STATE",      "GOTO",       "AND",        "OR",
    "BITAND",     "BITOR"
};

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
 * of course we stand to target Quake, which expects it's certain set of rules for proper
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
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108,
    0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF, 0x1231, 0x0210,
    0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B,
    0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462, 0x3443, 0x0420, 0x1401,
    0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE,
    0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6,
    0x5695, 0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D,
    0xC7BC, 0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B, 0x5AF5,
    0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC,
    0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87, 0x4CE4,
    0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD,
    0xAD2A, 0xBD0B, 0x8D68, 0x9D49, 0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13,
    0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A,
    0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E,
    0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1,
    0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB,
    0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0,
    0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xA7DB, 0xB7FA, 0x8799, 0x97B8,
    0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657,
    0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9,
    0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882,
    0x28A3, 0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92, 0xFD2E,
    0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07,
    0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E, 0xCF5D,
    0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74,
    0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

/* Non - Reflected */
uint16_t util_crc16(uint16_t current, const char *k, size_t len) {
    register uint16_t h = current;
    for (; len; --len, ++k)
        h = util_crc16_table[(h>>8)^((unsigned char)*k)]^(h<<8);
    return h;
}
/* Reflective Variation (for reference) */
#if 0
uint16_t util_crc16(const char *k, int len, const short clamp) {
    register uint16_t h= (uint16_t)0xFFFFFFFF;
    for (; len; --len, ++k)
        h = util_crc16_table[(h^((unsigned char)*k))&0xFF]^(h>>8);
    return (~h)%clamp;
}
#endif

/*
 * modifier is the match to make and the transposition from it, while add is the upper-value that determines the
 * transposition from uppercase to lower case.
 */
static GMQCC_INLINE size_t util_strtransform(const char *in, char *out, size_t outsz, const char *mod, int add) {
    size_t sz = 1;
    for (; *in && sz < outsz; ++in, ++out, ++sz) {
        *out = (*in == mod[0])
                    ? mod[1]
                    : (util_isalpha(*in) && ((add > 0) ? util_isupper(*in) : !util_isupper(*in)))
                        ? *in + add
                        : *in;
    }
    *out = 0;
    return sz-1;
}

size_t util_strtocmd(const char *in, char *out, size_t outsz) {
    return util_strtransform(in, out, outsz, "-_", 'A'-'a');
}
size_t util_strtononcmd(const char *in, char *out, size_t outsz) {
    return util_strtransform(in, out, outsz, "_-", 'a'-'A');
}
size_t util_optimizationtostr(const char *in, char *out, size_t outsz) {
    return util_strtransform(in, out, outsz, "_ ", 'a'-'A');
}

int util_snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list  arg;
    int      ret;

    va_start(arg, fmt);
    ret = platform_vsnprintf(str, size, fmt, arg);
    va_end(arg);

    return ret;
}

int util_asprintf(char **ret, const char *fmt, ...) {
    va_list  args;
    int      read;

    va_start(args, fmt);
    read = platform_vasprintf(ret, fmt, args);
    va_end  (args);

    return read;
}

int util_sscanf(const char *str, const char *format, ...) {
    va_list  args;
    int      read;

    va_start(args, format);
    read = platform_vsscanf(str, format, args);
    va_end(args);

    return read;
}

char *util_strncpy(char *dest, const char *src, size_t n) {
    return platform_strncpy(dest, src, n);
}
char *util_strncat(char *dest, const char *src, size_t n) {
    return platform_strncat(dest, src, n);
}
char *util_strcat(char *dest, const char *src) {
    return platform_strcat(dest, src);
}
const char *util_strerror(int err) {
    return platform_strerror(err);
}

const struct tm *util_localtime(const time_t *timer) {
    return platform_localtime(timer);
}
const char *util_ctime(const time_t *timer) {
    return platform_ctime(timer);
}

bool util_isatty(fs_file_t *file) {
    if (file == (fs_file_t*)stdout) return !!platform_isatty(STDOUT_FILENO);
    if (file == (fs_file_t*)stderr) return !!platform_isatty(STDERR_FILENO);
    return false;
}

/*
 * A small noncryptographic PRNG based on:
 * http://burtleburtle.net/bob/rand/smallprng.html
 */
static uint32_t util_rand_state[4] = {
    0xF1EA5EED, 0x00000000,
    0x00000000, 0x00000000
};

#define util_rand_rot(X, Y) (((X)<<(Y))|((X)>>(32-(Y))))

uint32_t util_rand() {
    uint32_t last;

    last               = util_rand_state[0] - util_rand_rot(util_rand_state[1], 27);
    util_rand_state[0] = util_rand_state[1] ^ util_rand_rot(util_rand_state[2], 17);
    util_rand_state[1] = util_rand_state[2] + util_rand_state[3];
    util_rand_state[2] = util_rand_state[3] + last;
    util_rand_state[3] = util_rand_state[0] + last;

    return util_rand_state[3];
}

#undef util_rand_rot

void util_seed(uint32_t value) {
    size_t i;

    util_rand_state[0] = 0xF1EA5EED;
    util_rand_state[1] = value;
    util_rand_state[2] = value;
    util_rand_state[3] = value;

    for (i = 0; i < 20; ++i)
        (void)util_rand();
}

