/*
 * memory_tool_store.c — C home for upstream Hermes tools/memory_tool_store.py.
 * PoP: w10_memorytool_0052c2_normalize_for_match @ tools/memory_tool_store.py:_normalize_for_match
 * PoP: w10_memorytool_0052c2_span_occurrences @ tools/memory_tool_store.py:_span_occurrences
 * PoP: w10_memorytool_0052c2_repeated_span_message @ tools/memory_tool_store.py:_repeated_span_message
 * PoP: w10_memorytool_0052c2_over_limit @ tools/memory_tool_store.py:_over_limit
 * PoP: w10_memorytool_0052c2_stale_entry_message @ tools/memory_tool_store.py:_stale_entry_message
 * PoP: w10_memorytool_0052c2_find_unique_match @ tools/memory_tool_store.py:_find_unique_match
 *
 * Module-level literals mirrored from the Python source (provenance):
 *   ENTRY_DELIMITER = "\n§\n"
 *   PATCH_DROP_HINT = "To drop text inside an entry, patch a longer span that contains it;
 *                      'remove' deletes the whole entry."
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_memorytool_0052c2.h"

static const char *const tools_memory_tool_store_CONST_ENTRY_DELIMITER = "\n\xC2\xA7\n";
static const char *const tools_memory_tool_store_CONST_PATCH_DROP_HINT =
    "To drop text inside an entry, patch a longer span that contains it; 'remove' deletes the whole entry.";

/* str.maketrans(_MATCH_FOLD): quotes/backticks -> ', dashes -> -, then split/join. */
static int is_ws(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
char *w10_memorytool_0052c2_normalize_for_match(const char *text) {
    if (!text) return strdup("");
    size_t cap = strlen(text) * 4 + 8;
    char *folded = (char *)malloc(cap);
    char *w = folded;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        /* literal backslash-n -> space */
        if (p[0]=='\\' && p[1]=='n') { *w++=' '; p+=2; continue; }
        if (*p=='"' || *p=='`') { *w++='\''; p++; continue; }
        /* U+2018..U+201F -> ' ; U+2010..U+2015, U+2212 -> - */
        if (p[0]==0xE2 && p[1]==0x80 && p[2]>=0x90 && p[2]<=0x9F) {
            *w++ = (p[2]<=0x95) ? '-' : '\''; p+=3; continue;
        }
        if (p[0]==0xE2 && p[1]==0x88 && p[2]==0x92) { *w++='-'; p+=3; continue; }
        if (p[0]==0xC2 && p[1]==0xA0) { *w++=' '; p+=2; continue; } /* NBSP */
        *w++ = (char)*p++;
    }
    *w='\0';
    /* " ".join(folded.split()) */
    char *out = (char *)malloc(cap);
    size_t n = 0;
    char *r = folded;
    while (*r) {
        while (*r && is_ws((unsigned char)*r)) r++;
        if (!*r) break;
        if (n) out[n++]=' ';
        while (*r && !is_ws((unsigned char)*r)) out[n++]=*r++;
    }
    out[n]='\0';
    free(folded);
    return out;
}

int w10_memorytool_0052c2_span_occurrences(const char *text, const char *span) {
    if (!text) return 0;
    if (!span || !*span) return (int)strlen(text) + 1;
    size_t sl = strlen(span), tl = strlen(text);
    if (sl > tl) return 0;
    int count = 0;
    for (size_t i = 0; i + sl <= tl; i++)
        if (!memcmp(text + i, span, sl)) count++;
    return count;
}

char *w10_memorytool_0052c2_repeated_span_message(const char *old_text, int count) {
    char *out = (char *)malloc(4096);
    snprintf(out, 4096,
        "'%s' appears %d times in the matched entry, and patch edits one span. "
        "Include more surrounding text so old_text occurs once.",
        old_text ? old_text : "", count);
    return out;
}

static void comma_group(long long v, char *out, size_t cap) {
    char neg = v < 0 ? 1 : 0;
    unsigned long long u = neg ? (unsigned long long)(-v) : (unsigned long long)v;
    char buf[64]; int n = 0;
    do { buf[n++] = (char)('0' + (u % 10)); u /= 10; } while (u);
    size_t k = 0;
    if (neg) out[k++] = '-';
    for (int i = n - 1; i >= 0; i--) {
        out[k++] = buf[i];
        if (i && (i % 3) == 0) out[k++] = ',';
    }
    out[k] = '\0';
}

char *w10_memorytool_0052c2_over_limit(const char *new_total, const char *limit) {
    long long nt = strtoll(new_total ? new_total : "0", NULL, 10);
    long long li = strtoll(limit ? limit : "0", NULL, 10);
    char a[64], b[64], d[64];
    comma_group(nt, a, sizeof(a)); comma_group(li, b, sizeof(b)); comma_group(nt - li, d, sizeof(d));
    char *out = (char *)malloc(512);
    snprintf(out, 512, "would put memory at %s/%s chars: free at least %s chars", a, b, d);
    return out;
}

char *w10_memorytool_0052c2_stale_entry_message(const char *entry) {
    char *out = (char *)malloc(8192);
    snprintf(out, 8192,
        "Entry changed since it was staged, so this write was not applied: '%s' is no longer "
        "in memory as reviewed. Recreate the change against the current entry or reject it; "
        "the pending record has been preserved.", entry ? entry : "");
    return out;
}

static int needle_has_alnum(const char *s) {
    for (const char *p = s; *p; p++) if (isalnum((unsigned char)*p)) return 1;
    return 0;
}

json_t *w10_memorytool_0052c2_find_unique_match(json_t *entries, const char *old_text) {
    json_t *result = json_array();
    const char *needle = old_text ? old_text : "";
    size_t n = (entries && entries->type == JSON_ARRAY) ? json_array_size(entries) : 0;
    long exact_first = -1;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_array_get(entries, i);
        if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, needle)) { exact_first=(long)i; break; }
    }
    /* matches[] uses the SAME selection rule Python uses in _find_unique_match. */
    long matches[64];
    size_t nm = 0;
    if (exact_first >= 0) {
        for (size_t i = 0; i < n && nm < 64; i++) {
            json_t *e = json_array_get(entries, i);
            if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, needle))
                matches[nm++] = (long)i;
        }
    } else {
        for (size_t i = 0; i < n && nm < 64; i++) {
            json_t *e = json_array_get(entries, i);
            if (e && e->type == JSON_STRING && e->str_val && strstr(e->str_val, needle))
                matches[nm++] = (long)i;
        }
        if (nm == 0) {
            char *nd = w10_memorytool_0052c2_normalize_for_match(needle);
            if (needle_has_alnum(nd)) {
                for (size_t i = 0; i < n && nm < 64; i++) {
                    json_t *e = json_array_get(entries, i);
                    if (!e || e->type != JSON_STRING || !e->str_val) continue;
                    char *nf = w10_memorytool_0052c2_normalize_for_match(e->str_val);
                    if (strstr(nf, nd)) matches[nm++] = (long)i;
                    free(nf);
                }
            }
            free(nd);
        }
    }
    /* distinct matched entry texts -> ambiguous (first wins for duplicates) */
    int ambiguous = 0;
    const char *seen = NULL;
    for (size_t j = 0; j < nm; j++) {
        json_t *e = json_array_get(entries, (size_t)matches[j]);
        const char *t = (e && e->str_val) ? e->str_val : "";
        if (!seen) seen = t;
        else if (strcmp(seen, t)) { ambiguous = 1; break; }
    }
    long first = nm ? matches[0] : -1;
    json_append(result, first >= 0 ? json_number((double)first) : json_null());
    json_append(result, json_bool(ambiguous));
    return result;
}
