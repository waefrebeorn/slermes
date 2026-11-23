/*
 * testenv.c — C port of upstream Hermes pm/testenv.py.
 *
 * PoP: w5testenv_developer_tools @ pm/testenv.py:developer_tools
 * PoP: w5testenv_parse_extras   @ pm/testenv.py:parse_extras
 *
 * The path/subprocess-bound helpers (testenv_root / ensure_testenv /
 * testenv_python) are intentionally not reimplemented here: the gate covers
 * the pure, deterministic decision logic and the module constants.
 */
#include "noc_w5_testenv_ca5da8.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *w5testenv_name(void) { return W5TESTENV_NAME; }

const char *const *w5testenv_default_extras(size_t *outn) {
    static const char *const v[] = {"all"};
    if (outn) *outn = 1;
    return v;
}
const char *const *w5testenv_groups(size_t *outn) {
    static const char *const v[] = {"dev", "test"};
    if (outn) *outn = 2;
    return v;
}
const char *const *w5testenv_developer_const(size_t *outn) {
    static const char *const v[] = {"just"};
    if (outn) *outn = 1;
    return v;
}

static int in_list(const char **list, size_t n, const char *name) {
    for (size_t i = 0; i < n; i++)
        if (list[i] && !strcmp(list[i], name)) return 1;
    return 0;
}

int w5testenv_developer_tools(const char **pinned, size_t npinned,
                              char ***out, size_t *outn) {
    size_t nd = 0;
    const char *const *dev = w5testenv_developer_const(&nd);
    char **res = (char **)calloc(nd + 1, sizeof(char *));
    size_t k = 0;
    if (res) {
        for (size_t i = 0; i < nd; i++)
            if (in_list(pinned, npinned, dev[i]))
                res[k++] = strdup(dev[i]);
    }
    if (out) *out = res;
    if (outn) *outn = k;
    return (int)k;
}

int w5testenv_parse_extras(const char *value, char ***out, size_t *outn) {
    if (!value) value = "";
    size_t n = strlen(value);
    char *buf = (char *)malloc(n + 1);
    if (!buf) { if (out) *out = NULL; if (outn) *outn = 0; return 0; }
    memcpy(buf, value, n + 1);
    for (size_t i = 0; i < n; i++)
        if (buf[i] == ',') buf[i] = ' ';

    size_t count = 0;
    for (size_t i = 0; i < n;) {
        while (i < n && isspace((unsigned char)buf[i])) i++;
        if (i >= n) break;
        while (i < n && !isspace((unsigned char)buf[i])) i++;
        count++;
    }
    char **res = NULL;
    if (count) {
        res = (char **)calloc(count, sizeof(char *));
        size_t k = 0;
        for (size_t i = 0; i < n;) {
            while (i < n && isspace((unsigned char)buf[i])) i++;
            if (i >= n) break;
            size_t start = i;
            while (i < n && !isspace((unsigned char)buf[i])) i++;
            if (res) res[k] = (char *)malloc(i - start + 1);
            if (res && res[k]) {
                memcpy(res[k], buf + start, i - start);
                res[k][i - start] = '\0';
            }
            k++;
        }
    }
    free(buf);
    if (out) *out = res;      /* NULL when Python would return None */
    if (outn) *outn = count;
    return (int)count;
}

void w5testenv_free_list(char **arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; i++) free(arr[i]);
    free(arr);
}
