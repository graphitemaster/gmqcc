/*
 * Copyright (C) 2012
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
#include <stdarg.h>
#include <errno.h>
#include "gmqcc.h"

uint64_t mem_ab = 0;
uint64_t mem_db = 0;
uint64_t mem_at = 0;
uint64_t mem_dt = 0;

struct memblock_t {
    const char  *file;
    unsigned int line;
    size_t       byte;
    struct memblock_t *next;
    struct memblock_t *prev;
};

static struct memblock_t *mem_start = NULL;

void *util_memory_a(size_t byte, unsigned int line, const char *file) {
    struct memblock_t *info = malloc(sizeof(struct memblock_t) + byte);
    void              *data = (void*)(info+1);
    if (!info) return NULL;
    info->line = line;
    info->byte = byte;
    info->file = file;
    info->prev = NULL;
    info->next = mem_start;
    if (mem_start)
        mem_start->prev = info;
    mem_start = info;

    util_debug("MEM", "allocation:   % 8u (bytes) address 0x%08X @ %s:%u\n", byte, data, file, line);
    mem_at++;
    mem_ab += info->byte;

    return data;
}

void util_memory_d(void *ptrn, unsigned int line, const char *file) {
    struct memblock_t *info = NULL;

    if (!ptrn) return;
    info = ((struct memblock_t*)ptrn - 1);

    util_debug("MEM", "released:     % 8u (bytes) address 0x%08X @ %s:%u\n", info->byte, ptrn, file, line);
    mem_db += info->byte;
    mem_dt++;

    if (info->prev)
        info->prev->next = info->next;
    if (info->next)
        info->next->prev = info->prev;
    if (info == mem_start)
        mem_start = info->next;

    free(info);
}

void *util_memory_r(void *ptrn, size_t byte, unsigned int line, const char *file) {
    struct memblock_t *oldinfo = NULL;

    struct memblock_t *newinfo;

    if (!ptrn)
        return util_memory_a(byte, line, file);
    if (!byte) {
        util_memory_d(ptrn, line, file);
        return NULL;
    }

    oldinfo = ((struct memblock_t*)ptrn - 1);
    newinfo = ((struct memblock_t*)malloc(sizeof(struct memblock_t) + byte));

    util_debug("MEM", "reallocation: % 8u -> %u (bytes) address 0x%08X -> 0x%08X @ %s:%u\n", oldinfo->byte, byte, ptrn, (void*)(newinfo+1), file, line);

    /* new data */
    if (!newinfo) {
        util_memory_d(oldinfo+1, line, file);
        return NULL;
    }

    /* copy old */
    memcpy(newinfo+1, oldinfo+1, oldinfo->byte);

    /* free old */
    if (oldinfo->prev)
        oldinfo->prev->next = oldinfo->next;
    if (oldinfo->next)
        oldinfo->next->prev = oldinfo->prev;
    if (oldinfo == mem_start)
        mem_start = oldinfo->next;

    /* fill info */
    newinfo->line = line;
    newinfo->byte = byte;
    newinfo->file = file;
    newinfo->prev = NULL;
    newinfo->next = mem_start;
    if (mem_start)
        mem_start->prev = newinfo;
    mem_start = newinfo;

    mem_ab -= oldinfo->byte;
    mem_ab += newinfo->byte;

    free(oldinfo);

    return newinfo+1;
}

void util_meminfo() {
    struct memblock_t *info;

    if (!opts.memchk)
        return;

    for (info = mem_start; info; info = info->next) {
        util_debug("MEM", "lost:       % 8u (bytes) at %s:%u\n",
            info->byte,
            info->file,
            info->line);
    }

    util_debug("MEM", "Memory information:\n\
        Total allocations:   %llu\n\
        Total deallocations: %llu\n\
        Total allocated:     %llu (bytes)\n\
        Total deallocated:   %llu (bytes)\n\
        Leaks found:         lost %llu (bytes) in %d allocations\n",
            mem_at,   mem_dt,
            mem_ab,   mem_db,
           (mem_ab -  mem_db),
           (mem_at -  mem_dt)
    );
}

/*
 * Some string utility functions, because strdup uses malloc, and we want
 * to track all memory (without replacing malloc).
 */
char *util_strdup(const char *s) {
    size_t  len = 0;
    char   *ptr = NULL;

    if (!s)
        return NULL;

    if ((len = strlen(s)) && (ptr = mem_a(len+1))) {
        memcpy(ptr, s, len);
        ptr[len] = '\0';
    }
    return ptr;
}

/*
 * Remove quotes from a string, escapes from \ in string
 * as well.  This function shouldn't be used to create a
 * char array that is later freed (it uses pointer arith)
 */
char *util_strrq(const char *s) {
    char *dst = (char*)s;
    char *src = (char*)s;
    char  chr;
    while ((chr = *src++) != '\0') {
        if (chr == '\\') {
            *dst++ = chr;
            if ((chr = *src++) == '\0')
                break;
            *dst++ = chr;
        } else if (chr != '"')
            *dst++ = chr;
    }
    *dst = '\0';
    return dst;
}

/*
 * Chops a substring from an existing string by creating a
 * copy of it and null terminating it at the required position.
 */
char *util_strchp(const char *s, const char *e) {
    const char *c = NULL;
    if (!s || !e)
        return NULL;

    c = s;
    while (c != e)
        c++;

    return util_strdup(s);
}

/*
 * Returns true if string is all uppercase, otherwise
 * it returns false.
 */
bool util_strupper(const char *str) {
    while (*str) {
        if(!isupper(*str))
            return false;
        str++;
    }
    return true;
}

/*
 * Returns true if string is all digits, otherwise
 * it returns false.
 */
bool util_strdigit(const char *str) {
    while (*str) {
        if(!isdigit(*str))
            return false;
        str++;
    }
    return true;
}

bool util_strncmpexact(const char *src, const char *ned, size_t len) {
    return (!strncmp(src, ned, len) && !src[len]);
}

void util_debug(const char *area, const char *ms, ...) {
    va_list  va;
    if (!opts.debug)
        return;

    if (!strcmp(area, "MEM") && !opts.memchk)
        return;

    va_start(va, ms);
    con_out ("[%s] ", area);
    con_vout(ms, va);
    va_end  (va);
}

/*
 * Endianess swapping, all data must be stored little-endian.  This
 * reorders by stride and length, much nicer than other functions for
 * certian-sized types like short or int.
 */
void util_endianswap(void *m, int s, int l) {
    size_t w = 0;
    size_t i = 0;

    /* ignore if we're already LE */
    if(*((char *)&s))
        return;

    for(; w < (size_t)l; w++) {
        for(;  i < (size_t)(s << 1); i++) {
            unsigned char *p = (unsigned char *)m+w*s;
            unsigned char  t = p[i];
            p[i]             = p[s-i-1];
            p[s-i-1]         = t;
        }
    }
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
 * reflection of 1100. Quakle however expects a NON-Reflected CRC on the output, but still
 * requires a final XOR on the values (0xFFFF and 0x0000) this is a standard CCITT CRC-16
 * which I respectfully as a programmer don't agree with.
 *
 * So now you know what we target, and why we target it, despite how unsettling it may seem
 * but those are what Quake seems to request.
 */

/*
 * This is an implementation of CRC32 & CRC16. The polynomials have been
 * offline computed for faster generation at the cost of larger code size.
 *
 * CRC32 Polynomial: 0xEDB88320
 * CRC16 Polynomial: 0x00001021
 */
static const uint32_t util_crc32_table[] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};
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

/*
 * Implements a CRC function for X worth bits using (uint[X]_t)
 * as type. and util_crc[X]_table.

 * Quake expects a non-reflective CRC.
 */
#define CRC(X) \
uint##X##_t util_crc##X(uint##X##_t current, const char *k, size_t len) {  \
    register uint##X##_t h= current;                                  \
    for (; len; --len, ++k)                                           \
        h = util_crc##X##_table[(h>>8)^((unsigned char)*k)]^(h<<8);   \
    return h;                                                         \
}
CRC(32)
CRC(16)
#undef CRC
/*
#define CRC(X) \
uint##X##_t util_crc##X(const char *k, int len, const short clamp) {  \
    register uint##X##_t h= (uint##X##_t)0xFFFFFFFF;                  \
    for (; len; --len, ++k)                                           \
        h = util_crc##X##_table[(h^((unsigned char)*k))&0xFF]^(h>>8); \
    return (~h)%clamp;                                                \
}
*/


/*
 * Implements libc getline for systems that don't have it, which is
 * assmed all.  This works the same as getline().
 */
int util_getline(char **lineptr, size_t *n, FILE *stream) {
    int   chr;
    int   ret;
    char *pos;

    if (!lineptr || !n || !stream)
        return -1;
    if (!*lineptr) {
        if (!(*lineptr = (char*)mem_a((*n=64))))
            return -1;
    }

    chr = *n;
    pos = *lineptr;

    for (;;) {
        int c = getc(stream);

        if (chr < 2) {
            *n += (*n > 16) ? *n : 64;
            chr = *n + *lineptr - pos;
            if (!(*lineptr = (char*)mem_r(*lineptr,*n)))
                return -1;
            pos = *n - chr + *lineptr;
        }

        if (ferror(stream))
            return -1;
        if (c == EOF) {
            if (pos == *lineptr)
                return -1;
            else
                break;
        }

        *pos++ = c;
        chr--;
        if (c == '\n')
            break;
    }
    *pos = '\0';
    return (ret = pos - *lineptr);
}

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


FILE *util_fopen(const char *filename, const char *mode)
{
#ifdef WIN32
    FILE *out;
    if (fopen_s(&out, filename, mode) != 0)
        return NULL;
    return out;
#else
    return fopen(filename, mode);
#endif
}

void _util_vec_grow(void **a, size_t i, size_t s) {
    size_t m = *a ? 2*_vec_beg(*a)+i : i+1;
    void  *p = mem_r((*a ? _vec_raw(*a) : NULL), s * m + sizeof(size_t)*2);
    if (!*a)
        ((size_t*)p)[1] = 0;
    *a = (void*)((size_t*)p + 2);
    _vec_beg(*a) = m;
}

/*
 * Hash table for generic data, based on dynamic memory allocations
 * all around.  This is the internal interface, please look for
 * EXPOSED INTERFACE comment below
 */
typedef struct hash_node_t {
    char               *key;   /* the key for this node in table */
    void               *value; /* pointer to the data as void*   */
    struct hash_node_t *next;  /* next node (linked list)        */
} hash_node_t;

/*
 * x86 and x86_64 optimized murmur hash functions for the hashtable
 * we have individual implementations for optimal performance.
 *
 * Forced inlined as we wrap these up in the actual utility function
 * below.  These should be autovectorized by gcc.
 */
#ifdef __x86_64__
GMQCC_INLINE uint32_t util_hthashfunc(hash_table_t *ht, const char *key, register size_t seed) {
    const uint64_t       mix   = 0xC6A4A7935BD1E995;
    const int            rot   = 47;
    size_t               size  = strlen(key);
    uint64_t             hash  = seed ^ (size - mix);
    uint64_t             alias = 0;
    const uint64_t      *beg   = (const uint64_t*)key;
    const uint64_t      *end   = beg + (size / 8);
    const unsigned char *final = NULL;

    while (beg != end) {
        alias = *beg++;

        alias *= mix;
        alias ^= alias >> rot;
        alias *= mix;

        hash  ^= alias;
        hash  *= mix;
    }

    final = (const unsigned char *)beg;

    switch (size & 7) {
        case 7: hash ^= (uint64_t)(final[6]) << 48;
        case 6: hash ^= (uint64_t)(final[5]) << 40;
        case 5: hash ^= (uint64_t)(final[4]) << 32;
        case 4: hash ^= (uint64_t)(final[3]) << 24;
        case 3: hash ^= (uint64_t)(final[2]) << 16;
        case 2: hash ^= (uint64_t)(final[1]) << 8;
        case 1: hash ^= (uint64_t)(final[0]);
                hash *= mix;
    }

    hash ^= hash >> rot;
    hash *= mix;
    hash ^= hash >> rot;

    return (uint32_t)(hash % ht->size);
}

#else
GMQCC_INLINE uint32_t util_hthashfunc(hash_table_t *ht, const char *key, register size_t seed) {
    const uint32_t       mix   = 0x5BD1E995;
    const uint32_t       rot   = 24;
    size_t               size  = strlen(key);
    uint32_t             hash  = seed ^ size;
    uint32_t             alias = 0;
    const unsigned char *data  = (const unsigned char*)key;

    while (size >= 4) {
        alias = *(uint32_t*)data;

        alias *= mix;
        alias ^= alias >> rot;
        alias *= mix;

        hash  *= mix;
        hash  ^= alias;

        data += 4;
        size -= 4;
    }

    switch (size) {
        case 3: hash ^= data[2] << 16;
        case 2: hash ^= data[1] << 8;
        case 1: hash ^= data[0];
                hash *= mix;
    }

    hash ^= hash >> 13;
    hash *= mix;
    hash ^= hash >> 15;

    return hash % ht->size;
}
#endif

/* we use the crc table as seeds for the murmur hash :P */
size_t util_hthash(hash_table_t *ht, const char *key) {
    static   size_t seed = 0;
    register size_t hash = util_hthashfunc(ht, key, util_crc32_table[seed]);

    /* reset seed */
    if (seed >= sizeof(util_crc32_table) / sizeof(*util_crc32_table))
        seed  = 0;

    return hash;
}

hash_node_t *_util_htnewpair(const char *key, void *value) {
    hash_node_t *node;
    if (!(node = mem_a(sizeof(hash_node_t))))
        return NULL;

    if (!(node->key = util_strdup(key))) {
        mem_d(node);
        return NULL;
    }

    node->value = value;
    node->next  = NULL;

    return node;
}

/*
 * EXPOSED INTERFACE for the hashtable implementation
 * util_htnew(size)                             -- to make a new hashtable
 * util_htset(table, key, value, sizeof(value)) -- to set something in the table
 * util_htget(table, key)                       -- to get something from the table
 * util_htdel(table)                            -- to delete the table
 */
hash_table_t *util_htnew(size_t size) {
    hash_table_t *hashtable = NULL;
    if (size < 1)
        return NULL;

    if (!(hashtable = mem_a(sizeof(hash_table_t))))
        return NULL;

    if (!(hashtable->table = mem_a(sizeof(hash_node_t*) * size))) {
        mem_d(hashtable);
        return NULL;
    }

    hashtable->size = size;
    memset(hashtable->table, 0, sizeof(hash_node_t*) * size);

    return hashtable;
}

void util_htseth(hash_table_t *ht, const char *key, size_t bin, void *value) {
    hash_node_t *newnode = NULL;
    hash_node_t *next    = NULL;
    hash_node_t *last    = NULL;

    next = ht->table[bin];

    while (next && next->key && strcmp(key, next->key) > 0)
        last = next, next = next->next;

    /* already in table, do a replace */
    if (next && next->key && strcmp(key, next->key) == 0) {
        next->value = value;
    } else {
        /* not found, grow a pair man :P */
        newnode = _util_htnewpair(key, value);
        if (next == ht->table[bin]) {
            newnode->next  = next;
            ht->table[bin] = newnode;
        } else if (!next) {
            last->next = newnode;
        } else {
            newnode->next = next;
            last->next = newnode;
        }
    }
}

void util_htset(hash_table_t *ht, const char *key, void *value) {
    util_htseth(ht, key, util_hthash(ht, key), value);
}

void *util_htgeth(hash_table_t *ht, const char *key, size_t bin) {
    hash_node_t *pair = ht->table[bin];

    while (pair && pair->key && strcmp(key, pair->key) > 0)
        pair = pair->next;

    if (!pair || !pair->key || strcmp(key, pair->key) != 0)
        return NULL;

    return pair->value;
}

void *util_htget(hash_table_t *ht, const char *key) {
    return util_htgeth(ht, key, util_hthash(ht, key));
}

/*
 * Free all allocated data in a hashtable, this is quite the amount
 * of work.
 */
void util_htdel(hash_table_t *ht) {
    size_t i = 0;
    for (; i < ht->size; i++) {
        hash_node_t *n = ht->table[i];
        hash_node_t *p;

        /* free in list */
        while (n) {
            if (n->key)
                mem_d(n->key);
            p = n;
            n = n->next;
            mem_d(p);
        }

    }
    /* free table */
    mem_d(ht->table);
    mem_d(ht);
}
