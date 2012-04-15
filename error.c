/*
 * Copyright (C) 2012 
 * 	Dale Weiler
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
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "gmqcc.h"

/*
 * Compiler error system, this handles the error printing, and managing
 * such as after so many errors just stop the compilation, and other
 * intereting like colors for the console.
 */
#ifndef WIN32
#	define CON_BLACK   30
#	define CON_RED     31
#	define CON_GREEN   32
#	define CON_BROWN   33
#	define CON_BLUE    34
#	define CON_MAGENTA 35
#	define CON_CYAN    36
#	define CON_WHITE   37
static const int error_color[] = {
	CON_RED,
	CON_CYAN,
	CON_MAGENTA,
	CON_BLUE,
	CON_BROWN,
	CON_WHITE
};
#endif
int error_total = 0;
int error_max   = 10;

static const char *const error_list[] = {
	"Parsing Error:",
	"Lexing Error:",
	"Internal Error:",
	"Compilation Error:",
	"Preprocessor Error:"
};

int error(struct lex_file *file, int status, const char *msg, ...) {
	char      bu[1024*4]; /* enough? */
	char      fu[1024*4]; /* enough? */
	va_list   va;
	
	if (error_total + 1 > error_max) {
		fprintf(stderr, "%d errors and more following, bailing\n", error_total);
		exit (-1);
	}
	error_total ++;
/* color */
#	ifndef WIN32
	sprintf  (bu, "\033[0;%dm%s \033[0;%dm %s:%d ", error_color[status-SHRT_MAX], error_list[status-SHRT_MAX], error_color[(status-1)-SHRT_MAX], file->name, file->line);
#else
	sprintf  (bu, "%s ", error_list[status-SHRT_MAX]);
#endif
	va_start (va, msg);
	vsprintf (fu, msg, va);
	va_end   (va);
	fputs    (bu, stderr);
	fputs    (fu, stderr);

/* color */
#	ifndef WIN32
	fputs    ("\033[0m", stderr);
#	endif
	
	fflush   (stderr);
	
	return status;
}
