/*
 * files.c — C home for upstream Hermes utils/files.py (gate w5_files_19ae74).
 *
 * PoP: w5_files_19ae74_yaml11_ambiguous          @ utils/files.py:_rt_value
 * PoP: w5_files_19ae74_contended_replace_error   @ utils/files.py:_is_contended_windows_replace_error
 * PoP: w5_files_19ae74_mtime_is_racy              @ utils/files.py:mtime_is_racy
 * PoP: w5_files_19ae74_file_signature             @ utils/files.py:file_signature
 *
 * Pure predicates/keys from the file-cache layer. All 64-bit quantities cross
 * the JSON boundary as decimal strings (JSON numbers are doubles in libjson).
 */
#include "noc_w5_files_19ae74.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long long w5f_i64(const char *s) {
    if (!s) return 0;
    return strtoll(s, NULL, 10);
}

int w5_files_19ae74_yaml11_ambiguous(const char *word) {
    if (!word) return 0;
    char buf[128];
    size_t n = strlen(word);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)word[i]);
    buf[n] = '\0';
    static const char *const WORDS[] = {
        "y", "n", "yes", "no", "true", "false", "on", "off", "null", "~"
    };
    for (size_t i = 0; i < sizeof(WORDS) / sizeof(WORDS[0]); i++)
        if (strcmp(buf, WORDS[i]) == 0) return 1;
    return 0;
}

int w5_files_19ae74_contended_replace_error(int is_windows, long long winerror) {
    return is_windows && (winerror == 5 || winerror == 32 || winerror == 33);
}

int w5_files_19ae74_mtime_is_racy(const char *mtime_ns, const char *now_ns) {
    /* time.time_ns() - mtime_ns < RACY_MTIME_WINDOW_NS (2_000_000_000) */
    long long diff = w5f_i64(now_ns) - w5f_i64(mtime_ns);
    return diff < 2000000000LL;
}

json_t *w5_files_19ae74_file_signature(const char *mtime_ns, const char *size,
                                       const char *ino, const char *ctime_ns) {
    const char *in[4] = {mtime_ns, size, ino, ctime_ns};
    json_t *out = json_array();
    for (int i = 0; i < 4; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%lld", w5f_i64(in[i]));
        json_append(out, json_string(buf));
    }
    return out;
}

