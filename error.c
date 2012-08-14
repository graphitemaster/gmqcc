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
#include "gmqcc.h"

/*
 * Compiler error system, this handles the error printing, and managing
 * such as after so many errors just stop the compilation, and other
 * intereting like colors for the console.
 */

#ifndef WIN32
int levelcolor[] = {
    CON_WHITE,
    CON_CYAN,
    CON_RED
};
#endif

void vprintmsg(int level, const char *name, size_t line, const char *msgtype, const char *msg, va_list ap)
{
#ifndef WIN32
    fprintf (stderr, "\033[0;%dm%s:%d: \033[0;%dm%s: \033[0m", CON_CYAN, name, (int)line, levelcolor[level], errtype);
#else
    fprintf (stderr, "%s:%d: %s: ", name, line, errtype);
#endif
    vfprintf(stderr, msg, ap);
    fprintf (stderr, "\n");
}

void printmsg(int level, const char *name, size_t line, const char *msgtype, const char *msg, ...)
{
    va_list   va;
    va_start(va, msg);
    vprintmsg(level, name, line, errtype, msg, va);
    va_end  (va);
}

void cvprintmsg(lex_ctx ctx, int lvl, const char *msgtype, const char *msg, va_list ap)
{
    vprintmsg(lvl, ctx.name, ctx.line, msgtype, msg, ap);
}

void cprintmsg (lex_ctx ctx, int lvl, const char *msgtype, const char *msg, ...)
{
    va_list   va;
    va_start(va, msg);
    cvprintmsg(ctx, lvl, msgtype, msg, va);
    va_end  (va);
}
