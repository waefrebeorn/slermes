// shim: account_usage_title_case_slug — mirrors Python agent/account_usage.py::_title_case_slug
// Python source: s=str(value or "").strip(); if not s: return None
//   return s.replace("_", " ").replace("-", " ").title()
// Key: Python STRIPS leading/trailing whitespace BEFORE title-casing.
// Returns NULL (watercup traces as "") only for None/empty/whitespace-only inputs.
// NOTE: C harness passes "NULL" string to indicate None, so we treat it as None.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

const char *account_usage_title_case_slug(const char *value) {
    if (!value) return NULL;
    // C harness passes "NULL" string to indicate None
    if (strcmp(value, "NULL") == 0) return NULL;

    size_t len = strlen(value);
    if (len == 0) return NULL;

    // Python: s = str(value).strip() — strip leading/trailing whitespace
    size_t start = 0;
    while (start < len && (value[start] == ' ' || value[start] == '\t' ||
                            value[start] == '\n' || value[start] == '\r'))
        start++;
    size_t end = len;
    while (end > start && (value[end-1] == ' ' || value[end-1] == '\t' ||
                            value[end-1] == '\n' || value[end-1] == '\r'))
        end--;

    size_t stripped_len = end - start;
    if (stripped_len == 0) return NULL;  // whitespace-only → None

    // Python: s.replace("_", " ").replace("-", " ").title()
    // .title() capitalizes first letter of each word, lowercases rest.
    char *buf = malloc(stripped_len + 1);
    if (!buf) return NULL;
    int prev_alpha = 0;
    for (size_t i = 0; i < stripped_len; i++) {
        char c = value[start + i];
        if (c == '_') c = ' ';
        else if (c == '-') c = ' ';
        if (isalpha((unsigned char)c)) {
            if (!prev_alpha) {
                c = toupper((unsigned char)c);
            } else {
                c = tolower((unsigned char)c);
            }
            prev_alpha = 1;
        } else {
            prev_alpha = 0;
        }
        buf[i] = c;
    }
    buf[stripped_len] = '\0';
    return buf;
}
