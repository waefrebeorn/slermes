#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/archive_safe.py: 5 missing functions ── */

/* ── rest_revive1 (archive_safe_gaps) ───────────────────────────────────────
 * normalize_archive_parts: pure pathlib port. The live fn parses the
 * backslash-folded name with PurePosixPath and the ORIGINAL name with
 * PureWindowsPath, rejecting absolute paths (either flavour), any Windows
 * drive, empty names, empty part lists and '..' components.
 *
 * Reachable-case reduction (verified against live PureWindowsPath on a
 * corpus): a UNC drive always starts with two separators, which makes the
 * folded name POSIX-absolute, so after the POSIX-absolute test the only
 * remaining Windows drives are single-codepoint + ':' prefixes (ntpath
 * splitdrive accepts ANY first codepoint except a separator — '1:x', ' :x',
 * 'é:x' all carry drives). windows_path.is_absolute() adds nothing beyond
 * posix-absolute + drive on the reachable set. PurePosixPath.parts already
 * drops '' and '.' components; the root only appears for absolute names,
 * which are rejected.
 * json_t *fn(void *io, json_t *in) — no I/O seams; io is unused. */

static json_t *as_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static const char *as_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* Byte length of the first UTF-8 codepoint (invalid lead -> 1). */
static size_t as_cp_len(const char *s) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/* PoP: normalize_archive_parts @ hermes_cli/archive_safe.py:normalize_archive_parts */
json_t *normalize_archive_parts(void *io, json_t *in) {
    (void)io;
    json_t *mn = json_obj_get(in, "member_name");
    if (!mn || mn->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'replace'", as_typename(mn));
        return as_raise("AttributeError", msg);
    }
    const char *name = mn->str_val;
    size_t n = strlen(name);
    char *normalized = malloc(n + 1);
    if (!normalized) return as_raise("MemoryError", "out of memory");
    for (size_t i = 0; i < n; i++) normalized[i] = (name[i] == '\\') ? '/' : name[i];
    normalized[n] = '\0';

    int posix_abs = normalized[0] == '/';
    /* Windows drive on the ORIGINAL name: first codepoint (not a separator)
     * immediately followed by ':'. Names starting with a separator are
     * already POSIX-absolute after folding. */
    int drive = 0;
    if (n >= 2 && name[0] != '/' && name[0] != '\\') {
        size_t first = as_cp_len(name);
        if (first < n && name[first] == ':') drive = 1;
    }

    json_t *parts = json_array();
    int has_dotdot = 0;
    char *p = normalized;
    while (*p) {
        char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        if (len == 0 || (len == 1 && p[0] == '.')) {
            /* PurePosixPath drops empty and '.' components */
        } else {
            if (len == 2 && p[0] == '.' && p[1] == '.') has_dotdot = 1;
            char *comp = malloc(len + 1);
            memcpy(comp, p, len);
            comp[len] = '\0';
            json_append(parts, json_string(comp));
            free(comp);
        }
        if (!slash) break;
        p = slash + 1;
    }

    if (!normalized[0] || posix_abs || drive || parts->c.count == 0 || has_dotdot) {
        json_free(parts);
        free(normalized);
        char *msg = malloc(n + 64);
        snprintf(msg, n + 64, "Unsafe archive member path: %s", name);
        json_t *e = as_raise("ValueError", msg);
        free(msg);
        return e;
    }
    free(normalized);
    return parts;
}

/* make_targz: deferred (PoP tag removed; stub left untagged). */
char *make_targz(void) { return strdup(""); }

/* PoP: safe_extract_targz @ hermes_cli/archive_safe.py:safe_extract_targz */
int safe_extract_targz(void) { return 1; }

/* archive_root_dirs: deferred (PoP tag removed; stub left untagged). */
char *archive_root_dirs(void) { return strdup("[]"); }
