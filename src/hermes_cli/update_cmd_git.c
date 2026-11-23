/*
 * update_cmd_git.c — C home for upstream Hermes hermes_cli/update_cmd_git.py.
 *
 * PoP: w12_updatecmd_c8ed08_is_fork @ hermes_cli/update_cmd_git.py:_is_fork
 * PoP: w12_updatecmd_c8ed08_has_http_code @ hermes_cli/update_cmd_git.py:_has_http_code
 * PoP: w12_updatecmd_c8ed08_classify_fetch_failure @ hermes_cli/update_cmd_git.py:_classify_fetch_failure
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w12_updatecmd_c8ed08.h"

static const char *const OFFICIAL_NORM[] = {
    "https://github.com/NousResearch/hermes-agent",
    "git@github.com:NousResearch/hermes-agent",
};
static const size_t OFFICIAL_NORM_N = sizeof(OFFICIAL_NORM) / sizeof(OFFICIAL_NORM[0]);

/* Python url.rstrip("/").removesuffix(".git"). */
static void norm_url(const char *url, char *buf, size_t cap) {
    size_t n = 0;
    if (url) { while (url[n] && n + 1 < cap) { buf[n] = url[n]; n++; } }
    buf[n] = '\0';
    while (n > 0 && buf[n - 1] == '/') { n--; buf[n] = '\0'; }  /* NUL at shortened length */
    if (n >= 4 && strcmp(buf + n - 4, ".git") == 0) { n -= 4; buf[n] = '\0'; }
}

static int has_http_one(const char *s, const char *code) {
    size_t need = strlen(code) + 32;
    char *a = (char *)malloc(need), *b = (char *)malloc(need);
    if (!a || !b) { free(a); free(b); return 0; }
    snprintf(a, need, "HTTP %s", code);
    snprintf(b, need, "returned error: %s", code);
    int hit = (strstr(s, a) != NULL) || (strstr(s, b) != NULL);
    free(a); free(b);
    return hit;
}

static int has_http_list(const char *s, const char *const *codes, size_t n) {
    for (size_t i = 0; i < n; i++) if (has_http_one(s, codes[i])) return 1;
    return 0;
}

json_t *w12_updatecmd_c8ed08_is_fork(const char *origin_url) {
    if (!origin_url || !*origin_url) return json_bool(false);
    char norm[4096];
    norm_url(origin_url, norm, sizeof(norm));
    for (size_t i = 0; i < OFFICIAL_NORM_N; i++)
        if (strcmp(norm, OFFICIAL_NORM[i]) == 0) return json_bool(false);
    return json_bool(true);
}

json_t *w12_updatecmd_c8ed08_has_http_code(const char *stderr_text, const json_t *codes) {
    const char *s = stderr_text ? stderr_text : "";
    if (!codes || codes->type != JSON_ARRAY) return json_bool(false);
    for (size_t i = 0; i < json_len(codes); i++) {
        json_t *v = json_get(codes, i);
        if (v && v->type == JSON_STRING && v->str_val && has_http_one(s, v->str_val))
            return json_bool(true);
    }
    return json_bool(false);
}

json_t *w12_updatecmd_c8ed08_classify_fetch_failure(const char *stderr_text) {
    const char *s = stderr_text ? stderr_text : "";
    /* Ordered exactly like _FETCH_FAILURE_RULES. */
    if (has_http_one(s, "429") || strstr(s, "rate limit") != NULL)
        return json_string("✗ GitHub is rate limiting requests or having an outage (HTTP 429) — try again in 5 minutes.");
    { const char *const c[] = {"500", "502", "503", "504"};
      if (has_http_list(s, c, 4))
          return json_string("✗ GitHub appears to be having an outage — try again in a few minutes (https://www.githubstatus.com)."); }
    if (strstr(s, "Could not resolve host") != NULL || strstr(s, "unable to access") != NULL)
        return json_string("✗ Network error — cannot reach the remote repository.");
    if (strstr(s, "could not read Username") != NULL || strstr(s, "terminal prompts disabled") != NULL)
        return json_string("✗ GitHub rejected the anonymous fetch (asked for a login) — this usually means a GitHub outage; try again in a few minutes (https://www.githubstatus.com). If it persists, check `git remote -v` points at a public repo.");
    if (strstr(s, "Authentication failed") != NULL)
        return json_string("✗ Authentication failed — check your git credentials or SSH key.");
    if (strstr(s, "Permission denied (publickey)") != NULL || strstr(s, "Host key verification failed") != NULL)
        return json_string("✗ SSH authentication failed — check your SSH key is added to GitHub, or switch `origin` to HTTPS: `git remote set-url origin https://github.com/NousResearch/hermes-agent.git`.");
    return json_string("✗ Failed to fetch updates from origin.");
}

void hermes_cli_update_cmd_git_port_pending(void) { (void)0; }
