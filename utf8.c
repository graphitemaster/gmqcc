/*
 * Copyright (C) 2012
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
#include "gmqcc.h"

static unsigned char utf8_lengths[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* ascii characters */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x80 - 0xBF are within multibyte sequences        */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* they could be interpreted as 2-byte starts but    */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* the codepoint would be < 127                      */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*                                                   */
    0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* C0 and C1 would also result in overlong encodings */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /*                                                   */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    /* with F5 the codepoint is above 0x10FFFF,
     * F8-FB would start 5-byte sequences
     * FC-FD would start 6-byte sequences
     * ...
     */
};

static uchar_t utf8_range[5] = {
    1,       /* invalid - let's not allow the creation of 0-bytes :P */
    1,       /* ascii minimum                                        */
    0x80,    /* 2-byte minimum                                       */
    0x800,   /* 3-byte minimum                                       */
    0x10000, /* 4-byte minimum                                       */
};

/** Analyze the next character and return various information if requested.
 * @param _s      An utf-8 string.
 * @param _start  Filled with the start byte-offset of the next valid character
 * @param _len    Filled with the length of the next valid character
 * @param _ch     Filled with the unicode value of the next character
 * @param _maxlen Maximum number of bytes to read from _s
 * @return        Whether or not another valid character is in the string
 */
bool u8_analyze(const char *_s, size_t *_start, size_t *_len, uchar_t *_ch, size_t _maxlen)
{
    const unsigned char *s = (const unsigned char*)_s;
    size_t i, j;
    size_t bits = 0;
    uchar_t ch;

    i = 0;
/* findchar: */
    while (i < _maxlen && s[i] && (bits = utf8_lengths[s[i]]) == 0)
        ++i;

    if (i >= _maxlen || !s[i]) {
        if (_start) *_start = i;
        if (_len) *_len = 0;
        return false;
    }

    if (bits == 1) { /* ascii */
        if (_start) *_start = i;
        if (_len) *_len = 1;
        if (_ch) *_ch = (uchar_t)s[i];
        return true;
    }

    ch = (s[i] & (0xFF >> bits));
    for (j = 1; j < bits; ++j)
    {
        if ( (s[i+j] & 0xC0) != 0x80 )
        {
            i += j;
            /* in gmqcc, invalid / overlong encodings are considered an error
             * goto findchar;
             */
            if (!s[i]) goto done;
            return false;
        }
        ch = (ch << 6) | (s[i+j] & 0x3F);
    }
    if (ch < utf8_range[bits] || ch >= 0x10FFFF)
    {
        /* same: error
         * i += bits;
         * goto findchar;
         */
        return false;
    }

done:
    if (_start)
        *_start = i;
    if (_len)
        *_len = bits;
    if (_ch)
        *_ch = ch;
    return true;
}

/* might come in handy */
size_t u8_strlen(const char *_s)
{
    size_t st, ln;
    size_t len = 0;
    const unsigned char *s = (const unsigned char*)_s;

    while (*s)
    {
        /* ascii char, skip u8_analyze */
        if (*s < 0x80)
        {
            ++len;
            ++s;
            continue;
        }

        /* invalid, skip u8_analyze */
        if (*s < 0xC2)
        {
            ++s;
            continue;
        }

        if (!u8_analyze((const char*)s, &st, &ln, NULL, 0x10))
            break;
        /* valid character, skip after it */
        s += st + ln;
        ++len;
    }
    return len;
}

size_t u8_strnlen(const char *_s, size_t n)
{
    size_t st, ln;
    size_t len = 0;
    const unsigned char *s = (const unsigned char*)_s;

    while (*s && n)
    {
        /* ascii char, skip u8_analyze */
        if (*s < 0x80)
        {
            ++len;
            ++s;
            --n;
            continue;
        }

        /* invalid, skip u8_analyze */
        if (*s < 0xC2)
        {
            ++s;
            --n;
            continue;
        }

        if (!u8_analyze((const char*)s, &st, &ln, NULL, n))
            break;
        /* valid character, see if it's still inside the range specified by n: */
        if (n < st + ln)
            return len;
        ++len;
        n -= st + ln;
        s += st + ln;
    }
    return len;
}

/* Required for character constants */
uchar_t u8_getchar(const char *_s, const char **_end)
{
    size_t st, ln;
    uchar_t ch;

    if (!u8_analyze(_s, &st, &ln, &ch, 0x10))
        ch = 0;
    if (_end)
        *_end = _s + st + ln;
    return ch;
}

uchar_t u8_getnchar(const char *_s, const char **_end, size_t _maxlen)
{
    size_t st, ln;
    uchar_t ch;

    if (!u8_analyze(_s, &st, &ln, &ch, _maxlen))
        ch = 0;
    if (_end)
        *_end = _s + st + ln;
    return ch;
}

/* required for \x{asdf}-like string escape sequences */
int u8_fromchar(uchar_t w, char *to, size_t maxlen)
{
    if (maxlen < 1)
        return 0;

    if (!w)
        return 0;

/* We may want an -f flag for this behaviour...
    if (w >= 0xE000)
        w -= 0xE000;
*/

    if (w < 0x80)
    {
        to[0] = (char)w;
        if (maxlen < 2)
            return -1;
        to[1] = 0;
        return 1;
    }
    /* for a little speedup */
    if (w < 0x800)
    {
        if (maxlen < 3)
        {
            to[0] = 0;
            return -1;
        }
        to[2] = 0;
        to[1] = 0x80 | (w & 0x3F); w >>= 6;
        to[0] = 0xC0 | w;
        return 2;
    }
    if (w < 0x10000)
    {
        if (maxlen < 4)
        {
            to[0] = 0;
            return -1;
        }
        to[3] = 0;
        to[2] = 0x80 | (w & 0x3F); w >>= 6;
        to[1] = 0x80 | (w & 0x3F); w >>= 6;
        to[0] = 0xE0 | w;
        return 3;
    }

    /* RFC 3629 */
    if (w <= 0x10FFFF)
    {
        if (maxlen < 5)
        {
            to[0] = 0;
            return -1;
        }
        to[4] = 0;
        to[3] = 0x80 | (w & 0x3F); w >>= 6;
        to[2] = 0x80 | (w & 0x3F); w >>= 6;
        to[1] = 0x80 | (w & 0x3F); w >>= 6;
        to[0] = 0xF0 | w;
        return 4;
    }
    return 0;
}
