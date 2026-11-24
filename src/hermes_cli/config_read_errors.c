/*
 * config_read_errors.c - C home for upstream Hermes hermes_cli/config_read_errors.py.
 *
 * PoP: w17_configread_1d9212_yaml_error_location @ hermes_cli/config_read_errors.py:_yaml_error_location
 * PoP: w17_configread_1d9212_yaml_error_details @ hermes_cli/config_read_errors.py:_yaml_error_details
 * PoP: w17_configread_1d9212_format_config_parse_failure @ hermes_cli/config_read_errors.py:format_config_parse_failure
 *
 * The live module's exception is represented by a JSON object carrying
 * problem_mark / context_mark / problem / str / oserror / permission. Path
 * spelling is injected verbatim (Windows Path str() emits backslashes).
 */
#include "noc_w17_configread_1d9212.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n;
    char *d;
    if (!s) return NULL;
    n = strlen(s) + 1;
    d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

json_t *w17_configread_1d9212_yaml_error_location(const json_t *exc) {
    json_t *mark = json_obj_get(exc, "problem_mark");
    json_t *line;
    char buf[64];
    if (!mark || mark->type == JSON_NULL) mark = json_obj_get(exc, "context_mark");
    if (!mark || mark->type != JSON_OBJECT) return json_string("");
    line = json_obj_get(mark, "line");
    if (!line || line->type != JSON_NUMBER) return json_string("");
    snprintf(buf, sizeof(buf), "line %lld", (long long)line->num_val + 1);
    return json_string(buf);
}

json_t *w17_configread_1d9212_yaml_error_details(const json_t *exc) {
    json_t *pb = json_obj_get(exc, "problem");
    const char *src;
    char *own = NULL, *out;
    size_t i, n, p = 0;
    int gap = 0;
    if (pb && pb->type == JSON_STRING && pb->str_val && pb->str_val[0]) {
        own = dupstr(pb->str_val);
    } else {
        json_t *sv = json_obj_get(exc, "str");
        own = dupstr(sv && sv->type == JSON_STRING ? sv->str_val : "");
    }
    src = own ? own : "";
    n = strlen(src);
    out = (char *)malloc(n + 2);
    if (!out) { free(own); return json_string(""); }
    for (i = 0; i < n; ++i) {
        if (isspace((unsigned char)src[i])) { gap = 1; continue; }
        if (gap && p > 0) out[p++] = ' ';
        gap = 0;
        out[p++] = src[i];
    }
    out[p] = '\0';
    free(own);
    return json_string(out);
}

static const char *fallback_msg(const char *fb) {
    if (!fb) fb = "";
    if (!strcmp(fb, "last-known-good")) return "Hermes is running on the settings it loaded before the edit until it is fixed, so recent changes are not applied.";
    if (!strcmp(fb, "last-known-good-backup")) return "Hermes is running on your last good settings until it is fixed, so recent changes are not applied.";
    if (!strcmp(fb, "refuse-write")) return "Nothing was written, so the existing file is preserved.";
    return "Hermes is running on default settings until it is fixed, so none of your saved settings are applied.";
}

json_t *w17_configread_1d9212_format_config_parse_failure(const char *path,
                                                          const json_t *exc,
                                                          const char *fallback) {
    char msg[8192], where[64], at[80], repair[512];
    char *p_copy, *fb_copy;
    json_t *locv;
    const char *loc;
    int is_os, perm;
    json_t *fv;
    (void)path;
    p_copy = dupstr(path ? path : "");
    fb_copy = dupstr(fallback ? fallback : "defaults");
    locv = w17_configread_1d9212_yaml_error_location(exc);
    loc = locv && locv->type == JSON_STRING ? locv->str_val : "";
    snprintf(where, sizeof(where), "%s", loc);
    fv = json_obj_get(exc, "oserror");
    is_os = fv && fv->type == JSON_BOOL && fv->bool_val;
    fv = json_obj_get(exc, "permission");
    perm = fv && fv->type == JSON_BOOL && fv->bool_val;
    if (is_os) {
        const char *fix = perm ? "Fix the file permissions or move it aside first."
                               : "Try again; run `hermes config check` if it keeps failing.";
        snprintf(msg, sizeof(msg), "Your settings file (%s) could not be read. %s %s",
                 p_copy ? p_copy : "", fallback_msg(fb_copy), fix);
    } else {
        const char *w = where[0] ? where : "the problem";
        snprintf(at, sizeof(at), "%s", where[0] ? where : "");
        char atbuf[96];
        snprintf(atbuf, sizeof(atbuf), "%s", at[0] ? " at " : "");
        strncat(atbuf, at, sizeof(atbuf) - strlen(atbuf) - 1);
        snprintf(repair, sizeof(repair),
                 "Open it with `hermes config edit`, fix %s, then run `hermes config check`.", w);
        snprintf(msg, sizeof(msg), "Your settings file (%s) has a formatting error%s. %s %s",
                 p_copy ? p_copy : "", atbuf, fallback_msg(fb_copy), repair);
    }
    json_free(locv);
    free(fb_copy);
    free(p_copy);
    return json_string(msg);
}
