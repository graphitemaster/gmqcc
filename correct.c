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

/*
 * This is a very clever method for correcting mistakes in QuakeC code
 * most notably when invalid identifiers are used or inproper assignments;
 * we can proprly lookup in multiple dictonaries (depening on the rules
 * of what the task is trying to acomplish) to find the best possible
 * match.
 *
 *
 * A little about how it works, and probability theory:
 *
 *  When given an identifier (which we will denote I), we're essentially
 *  just trying to choose the most likely correction for that identifier.
 *  (the actual "correction" can very well be the identifier itself).
 *  There is actually no way to know for sure that certian identifers
 *  such as "lates", need to be corrected to "late" or "latest" or any
 *  other permutations that look lexically the same.  This is why we
 *  must advocate the usage of probabilities.  This implies that we're
 *  trying to find the correction for C, out of all possible corrections
 *  that maximizes the probability of C for the original identifer I.
 *
 *  Bayes' Therom suggests something of the following:
 *      AC P(I|C) P(C) / P(I)
 *  Since P(I) is the same for every possibly I, we can ignore it giving
 *      AC P(I|C) P(C)
 *
 *  This greatly helps visualize how the parts of the expression are performed
 *  there is essentially three, from right to left we perform the following:
 *
 *  1: P(C), the probability that a proposed correction C will stand on its
 *     own.  This is called the language model.
 *
 *  2: P(I|C), the probability that I would be used, when the programmer
 *     really meant C.  This is the error model.
 *
 *  3: AC, the control mechanisim, which implies the enumeration of all
 *     feasible values of C, and then determine the one that gives the
 *     greatest probability score. Selecting it as the "correction"
 *   
 *
 * The requirement for complex expression involving two models:
 * 
 *  In reality the requirement for a more complex expression involving
 *  two seperate models is considerably a waste.  But one must recognize
 *  that P(C|I) is already conflating two factors.  It's just much simpler
 *  to seperate the two models and deal with them explicitaly.  To properly
 *  estimate P(C|I) you have to consider both the probability of C and
 *  probability of the transposition from C to I.  It's simply much more
 *  cleaner, and direct to seperate the two factors.
 */

/* some hashtable management for dictonaries */
static size_t *correct_find(ht table, const char *word) {
    return (size_t*)util_htget(table, word);
}

static int correct_update(ht *table, const char *word) {
    size_t *data = correct_find(*table, word);
    if (!data)
        return 0;

    (*data)++;
    return 1;
}


/*
 * _ is valid in identifiers. I've yet to implement numerics however
 * because they're only valid after the first character is of a _, or
 * alpha character.
 */
static const char correct_alpha[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";

static char *correct_strndup(const char *src, size_t n) {
    char   *ret;
    size_t  len = strlen(src);

    if (n < len)
        len = n;

    if (!(ret = (char*)mem_a(len + 1)))
        return NULL;

    ret[len] = '\0';
    return (char*)memcpy(ret, src, len);
}

static char *correct_concat(char *str1, char *str2, bool next) {
    char *ret = NULL;

#if 0
    if (!str1) {
         str1 = mem_a(1);
        *str1 = '\0';
    }
#endif

    str1 = mem_r (str1, strlen(str1) + strlen(str2) + 1);
    ret  = strcat(str1, str2);

    if (str2 && next)
        mem_d(str2);

    return ret;
}

/*
 * correcting logic for the following forms of transformations:
 *  1) deletion
 *  2) transposition
 *  3) alteration
 *  4) insertion
 */
static size_t correct_deletion(const char *ident, char **array, size_t index) {
    size_t itr;
    size_t len = strlen(ident);

    for (itr = 0; itr < len; itr++) {
        array[index + itr] = correct_concat (
            correct_strndup (ident,       itr),
            correct_strndup (ident+itr+1, len-(itr+1)),
            true
        );
    }

    return itr;
}

static size_t correct_transposition(const char *ident, char **array, size_t index) {
    size_t itr;
    size_t len = strlen(ident);

    for (itr = 0; itr < len - 1; itr++) {
        array[index + itr] = correct_concat (
            correct_concat (
                correct_strndup(ident,     itr),
                correct_strndup(ident+itr+1, 1),
                true
            ),
            correct_concat (
                correct_strndup(ident+itr,   1),
                correct_strndup(ident+itr+2, len-(itr+2)),
                true
            ),
            true
        );
    }

    return itr;
}

static size_t correct_alteration(const char *ident, char **array, size_t index) {
    size_t itr;
    size_t jtr;
    size_t ktr;
    size_t len    = strlen(ident);
    char   cct[2] = { 0, 0 }; /* char code table, for concatenation */

    for (itr = 0, ktr = 0; itr < len; itr++) {
        for (jtr = 0; jtr < sizeof(correct_alpha); jtr++, ktr++) {
            *cct = correct_alpha[jtr];
            array[index + ktr] = correct_concat (
                correct_concat (
                    correct_strndup(ident, itr),
                    (char *) &cct,
                    false
                ),
                correct_strndup (
                    ident + (itr+1),
                    len   - (itr+1)
                ),
                true
            );
        }
    }

    return ktr;
}

static size_t correct_insertion(const char *ident, char **array, size_t index) {
    size_t itr;
    size_t jtr;
    size_t ktr;
    size_t len    = strlen(ident);
    char   cct[2] = { 0, 0 }; /* char code table, for concatenation */

    for (itr = 0, ktr = 0; itr <= len; itr++) {
        for (jtr = 0; jtr < sizeof(correct_alpha); jtr++, ktr++) {
            *cct = correct_alpha[jtr];
            array[index + ktr] = correct_concat (
                correct_concat (
                    correct_strndup (ident, itr),
                    (char *) &cct,
                    false
                ),
                correct_strndup (
                    ident+itr,
                    len - itr
                ),
                true
            );
        }
    }

    return ktr;
}

static GMQCC_INLINE size_t correct_size(const char *ident) {
    /*
     * deletion      = len
     * transposition = len - 1
     * alteration    = len * sizeof(correct_alpha)
     * insertion     = (len + 1) * sizeof(correct_alpha)
     */   

    register size_t len = strlen(ident);
    return (len) + (len - 1) + (len * sizeof(correct_alpha)) + ((len + 1) * sizeof(correct_alpha));
}

static char **correct_edit(const char *ident) {
    size_t next;
    char **find = (char**)mem_a(correct_size(ident) * sizeof(char*));

    if (!find)
        return NULL;

    next  = correct_deletion     (ident, find, 0);
    next += correct_transposition(ident, find, next);
    next += correct_alteration   (ident, find, next);
    /*****/ correct_insertion    (ident, find, next);

    return find;
}

/*
 * We could use a hashtable but the space complexity isn't worth it
 * since we're only going to determine the "did you mean?" identifier
 * on error.
 */   
static int correct_exist(char **array, size_t rows, char *ident) {
    size_t itr;
    for (itr = 0; itr < rows; itr++)
        if (!strcmp(array[itr], ident))
            return 1;

    return 0;
}

static char **correct_known(ht table, char **array, size_t rows, size_t *next) {
    size_t itr;
    size_t jtr;
    size_t len;
    size_t row;
    char **res = NULL;
    char **end;

    for (itr = 0, len = 0; itr < rows; itr++) {
        end = correct_edit(array[itr]);
        row = correct_size(array[itr]);

        for (jtr = 0; jtr < row; jtr++) {
            if (correct_find(table, end[jtr]) && !correct_exist(res, len, end[jtr])) {
                res        = mem_r(res, sizeof(char*) * (len + 1));
                res[len++] = end[jtr];
            } else {
                mem_d(end[jtr]);
            }
        }

        mem_d(end);
    }

    *next = len;
    return res;
}

static char *correct_maximum(ht table, char **array, size_t rows) {
    char   *str  = NULL;
    size_t *itm  = NULL;
    size_t  itr;
    size_t  top;

    for (itr = 0, top = 0; itr < rows; itr++) {
        if ((itm = correct_find(table, array[itr])) && (*itm > top)) {
            top = *itm;
            str = array[itr];
        }
    }

    return str;
}

static void correct_cleanup(char **array, size_t rows) {
    size_t itr;
    for (itr = 0; itr < rows; itr++)
        mem_d(array[itr]);

    mem_d(array);
}

/*
 * This is the exposed interface:
 * takes a table for the dictonary a vector of sizes (used for internal
 * probability calculation, and an identifier to "correct"
 *
 * the add function works the same.  Except the identifier is used to
 * add to the dictonary.  
 */   
void correct_add(ht table, size_t ***size, const char *ident) {
    size_t     *data = NULL;
    const char *add  = ident;
    
    if (!correct_update(&table, add)) {
        data  = (size_t*)mem_a(sizeof(size_t));
        *data = 1;

        vec_push((*size), data);
        util_htset(table, add, data);
    }
}

char *correct_str(ht table, const char *ident) {
    char **e1;
    char **e2;
    char  *e1ident;
    char  *e2ident;
    char  *found = util_strdup(ident);

    size_t e1rows = 0;
    size_t e2rows = 0;

    /* needs to be allocated for free later */
    if (correct_find(table, ident))
        return found;

    /*mem_d(found);*/
    if ((e1rows = correct_size(ident))) {
        e1      = correct_edit(ident);

        if ((e1ident = correct_maximum(table, e1, e1rows))) {
            mem_d(found);
            found = util_strdup(e1ident);
            correct_cleanup(e1, e1rows);
            return found;
        }
    }

    e2 = correct_known(table, e1, e1rows, &e2rows);
    if (e2rows && ((e2ident = correct_maximum(table, e2, e2rows)))) {
        mem_d(found);
        found = util_strdup(e2ident);
    }
    
    correct_cleanup(e1, e1rows);
    correct_cleanup(e2, e2rows);
    
    return found;
}

void correct_del(ht dictonary, size_t **data) {
    size_t i;
    for (i = 0; i < vec_size(data); i++)
        mem_d(data[i]);

    vec_free(data);
    util_htdel(dictonary);
}
