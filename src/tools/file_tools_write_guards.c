/*
 * file_tools_write_guards.c — faithful C11 port of the pure display-text
 * detectors and refusal builder in tools/file_tools_write_guards.py.
 *
 * PoP: w18_filetools_b76f5d_is_internal_file_status_text @ tools/file_tools_write_guards.py:_is_internal_file_status_text
 * PoP: w18_filetools_b76f5d_looks_like_read_file_line_numbered_content @ tools/file_tools_write_guards.py:_looks_like_read_file_line_numbered_content
 * PoP: w18_filetools_b76f5d_is_internal_file_tool_content @ tools/file_tools_write_guards.py:_is_internal_file_tool_content
 * PoP: w18_filetools_b76f5d_stale_write_refusal @ tools/file_tools_write_guards.py:_stale_write_refusal
 */
#include "noc_w18_filetools_b76f5d.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* _READ_DEDUP_STATUS_MESSAGE, verbatim UTF-8 (154 code points, 156 bytes). */
static const char FTWG_MSG[] =
    "File unchanged since last read. The content from "
    "the earlier read_file result in this conversation is "
    "still current \xe2\x80\x94 refer to that instead of re-reading.";

static char *ftwg_dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

/* Python len(str): count Unicode code points (UTF-8 lead bytes), not bytes. */
static size_t ftwg_utf8_len(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    size_t n = 0;
    while (*p) { if ((*p & 0xC0) != 0x80) ++n; ++p; }
    return n;
}

/* Python str.strip()/lstrip() default whitespace set (ASCII subset used here). */
static int ftwg_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' ||
           c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
/* Allocate the stripped copy; NUL written at the NEW length. */
static char *ftwg_strip(const char *s, int left_only) {
    if (!s) return NULL;
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && ftwg_is_ws(p[a])) ++a;
    if (!left_only) while (b > a && ftwg_is_ws(p[b-1])) --b;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}

/* _is_internal_file_status_text */
int w18_filetools_b76f5d_is_internal_file_status_text(const char *content) {
    if (!content) return 0;
    char *stripped = ftwg_strip(content, 0);
    if (!stripped) return 0;
    int ok = 0;
    if (*stripped && strstr(stripped, FTWG_MSG) != NULL &&
        ftwg_utf8_len(stripped) <= 2 * ftwg_utf8_len(FTWG_MSG))
        ok = 1;
    free(stripped);
    return ok;
}

/* _looks_like_read_file_line_numbered_content */
int w18_filetools_b76f5d_looks_like_read_file_line_numbered_content(const char *content) {
    if (!content) return 0;
    int result = 0;
    char *work = ftwg_dupstr(content);
    if (!work) return 0;
    /* Python str.splitlines(): split on \n, \r, \r\n, \v, \f, 0x1c-0x1e. */
    size_t nlines = 0;
    int *nums = NULL;
    size_t nnums = 0, cap = 0;
    char *cur = work;
    while (1) {
        char *end = cur;
        while (*end && *end != '\n' && *end != '\r' && *end != '\v' && *end != '\f' &&
               *end != '\x1c' && *end != '\x1d' && *end != '\x1e') ++end;
        int had_sep = (*end != '\0');
        char sep = *end;
        *end = '\0';
        /* line = [line for line in content.splitlines() if line.strip()] */
        char *ls = ftwg_strip(cur, 0);
        if (ls && *ls) {
            ++nlines;
            /* prefix, sep, _rest = line.lstrip().partition('|') */
            char *lstr = ftwg_strip(cur, 1);  /* lstrip only */
            if (lstr) {
                char *bar = strchr(lstr, '|');
                if (bar) {
                    *bar = '\0';
                    /* prefix.isdigit(): non-empty, all ASCII digits */
                    size_t plen = strlen(lstr);
                    int digits = plen > 0;
                    for (size_t i = 0; i < plen; ++i)
                        if (lstr[i] < '0' || lstr[i] > '9') { digits = 0; break; }
                    if (digits) {
                        long long v = strtoll(lstr, NULL, 10);
                        if (nnums == cap) {
                            cap = cap ? cap * 2 : 16;
                            int *nn = (int *)realloc(nums, cap * sizeof(int));
                            if (!nn) { free(nums); nums = NULL; goto done; }
                            nums = nn;
                        }
                        nums[nnums++] = (int)v;
                    }
                }
                free(lstr);
            }
        }
        free(ls);
        if (!had_sep) break;
        cur = end + 1;
        if (sep == '\r' && *cur == '\n') ++cur;  /* CRLF is one separator */
    }

    if (nlines >= 2 && nnums >= 2 && ((double)nnums / (double)nlines) >= 0.6) {
        size_t consecutive = 0;
        for (size_t i = 1; i < nnums; ++i)
            if (nums[i] == nums[i-1] + 1) ++consecutive;
        result = consecutive >= nnums - 1;
    }
done:
    free(nums);
    free(work);
    return result;
}

/* _is_internal_file_tool_content */
int w18_filetools_b76f5d_is_internal_file_tool_content(const char *content) {
    return w18_filetools_b76f5d_is_internal_file_status_text(content) ||
           w18_filetools_b76f5d_looks_like_read_file_line_numbered_content(content);
}

/* _stale_write_refusal */
json_t *w18_filetools_b76f5d_stale_write_refusal(const char *filepath, const char *reason,
                                                 const char *resolved_or_null) {
    const char *fp = filepath ? filepath : "";
    const char *rz = reason ? reason : "";
    size_t cap = strlen(fp) + strlen(rz) + 512;
    char *buf = (char *)malloc(cap);
    json_t *obj = json_object();
    if (buf) {
        snprintf(buf, cap,
            "Refusing to overwrite %s: %s The file was NOT modified. Reload the current "
            "contents with read_file (every page, for a file that needs offset/limit), merge "
            "the requested change, then call write_file again. For small edits, prefer patch "
            "so existing unrelated changes are preserved.", fp, rz);
        json_set(obj, "error", json_string(buf));
        free(buf);
    } else {
        json_set(obj, "error", json_string(""));
    }
    json_set(obj, "stale_write_blocked", json_bool(1));
    json_set(obj, "path", json_string(fp));
    if (resolved_or_null && *resolved_or_null)
        json_set(obj, "resolved_path", json_string(resolved_or_null));
    return obj;
}
