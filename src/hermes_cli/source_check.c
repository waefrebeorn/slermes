/*
 * source_check.c — C port of upstream Hermes hermes_cli/source_check.py.
 *
 * PoP: w9_sourcecheck_3eff52_is_full_sha @ hermes_cli/source_check.py:_is_full_sha
 * PoP: w9_sourcecheck_3eff52_configured_branch @ hermes_cli/source_check.py:_configured_branch
 * PoP: w9_sourcecheck_3eff52_checked_out_branch @ hermes_cli/source_check.py:_checked_out_branch
 * PoP: w9_sourcecheck_3eff52_branch_remote @ hermes_cli/source_check.py:_branch_remote
 *
 * Faithful ports of the passive update-check's pure decision helpers.
 */
#include "noc_w9_sourcecheck_3eff52.h"
#include "hermes_json.h"
#include <ctype.h>
#include <string.h>

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int hex_digit(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int w9_sourcecheck_3eff52_is_full_sha(const json_t *value) {
    if (!value || value->type != JSON_STRING || !value->str_val) return 0;
    if (strlen(value->str_val) != 40) return 0;
    for (const char *p = value->str_val; *p; p++)
        if (!hex_digit((unsigned char)*p)) return 0;
    return 1;
}

struct json_t *w9_sourcecheck_3eff52_configured_branch(const json_t *desktop_config) {
    if (!desktop_config || desktop_config->type != JSON_OBJECT) return json_null();
    const json_t *v = json_obj_get(desktop_config, "branch");
    if (!v || v->type != JSON_STRING || !v->str_val) return json_null();
    const char *p = v->str_val;
    while (*p && is_ws((unsigned char)*p)) p++;
    const char *end = p + strlen(p);
    while (end > p && is_ws((unsigned char)end[-1])) end--;
    if (end == p) return json_null();
    char out[8192];
    size_t n = 0;
    for (const char *q = p; q < end && n + 1 < sizeof(out); q++) out[n++] = *q;
    out[n] = '\0';
    return json_string(out);
}

struct json_t *w9_sourcecheck_3eff52_checked_out_branch(const json_t *current_branch,
                                                       const json_t *fallback) {
    if (current_branch && current_branch->type == JSON_STRING && current_branch->str_val
            && current_branch->str_val[0] && strcmp(current_branch->str_val, "HEAD") != 0)
        return json_copy(current_branch);
    if (fallback && fallback->type == JSON_STRING && fallback->str_val)
        return json_copy(fallback);
    return json_null();
}

static unsigned char lower_ascii(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c - 'A' + 'a') : c;
}

static int ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (lower_ascii((unsigned char)*a) != lower_ascii((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static int istarts(const char *s, const char *prefix) {
    while (*prefix) {
        if (lower_ascii((unsigned char)*s) != lower_ascii((unsigned char)*prefix)) return 0;
        s++; prefix++;
    }
    return 1;
}

struct json_t *w9_sourcecheck_3eff52_branch_remote(int embedded, const char *repository,
                                                   const char *origin, const char *selected_branch) {
    const char *repo = repository ? repository : "";
    const char *orig = origin ? origin : "";
    const char *branch = selected_branch ? selected_branch : "";
    int official_ssh = repo[0] && ieq(repo, "NousResearch/hermes-agent")
                       && (istarts(orig, "git@") || istarts(orig, "ssh://"));
    if (embedded || (official_ssh && strcmp(branch, "main") != 0))
        return json_string("https://github.com/NousResearch/hermes-agent.git");
    return json_string("origin");
}
