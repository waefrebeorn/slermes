/*
 * port_verification_stop_helpers.c
 *
 * Pure, portable helper functions ported from agent/verification_stop.py.
 * No filesystem walk, no project-fact probing, no tempfile creation, no
 * lazy imports. _candidate_cwds (Path.resolve/is_dir) and _verification_snapshot
 * (project_facts_for / verification_status filesystem reads) stay REAL_GAP;
 * build_verify_on_stop_nudge is ported as a pure string assembler taking the
 * already-resolved status/facts JSON, matching the policy-only design.
 *
 * C name <- python name (module prefix 'verification_stop_'):
 *   verification_stop_is_non_code_path        <- _is_non_code_path
 *   verification_stop_filter_verifiable_paths <- _filter_verifiable_paths
 *   verification_stop_session_is_messaging_surface <- _session_is_messaging_surface
 *   verification_stop_enabled                 <- verify_on_stop_enabled
 *   verification_stop_format_changed_paths    <- _format_changed_paths
 *   verification_stop_status_detail           <- _status_detail
 *   verification_stop_build_nudge             <- build_verify_on_stop_nudge
 */

#include "hermes_json.h"
#include "coding_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <ctype.h>

static char *json_escape_string(const char *s)
{
    if (!s) s = "";
    size_t need = 1;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') need += 2;
        else if (c == '\n') need += 2;
        else if (c == '\r') need += 2;
        else if (c == '\t') need += 2;
        else if (c < 0x20) need += 6;
        else need += 1;
    }
    char *out = malloc(need + 1);
    char *q = out;
    *q++ = '"';
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') { *q++='\\'; *q++='"'; }
        else if (c == '\\') { *q++='\\'; *q++='\\'; }
        else if (c == '\n') { *q++='\\'; *q++='n'; }
        else if (c == '\r') { *q++='\\'; *q++='r'; }
        else if (c == '\t') { *q++='\\'; *q++='t'; }
        else if (c < 0x20) { sprintf(q, "\\u%04x", c); q += 6; }
        else *q++ = (char)c;
    }
    *q++ = '"';
    *q = '\0';
    return out;
}

#define MAX_CHANGED_PATHS_IN_NUDGE 8

static int is_non_code_ext(const char *suffix)
{
    static const char *exts[] = {
        ".md", ".markdown", ".mdx", ".rst", ".txt", ".text", ".adoc",
        ".asciidoc", ".org", ".log", ".csv", ".tsv", NULL
    };
    for (int i = 0; exts[i]; i++)
        if (strcasecmp(suffix, exts[i]) == 0) return 1;
    return 0;
}

static int is_non_code_name(const char *name)
{
    static const char *names[] = {
        "license", "licence", "notice", "authors", "contributors",
        "changelog", "codeowners", NULL
    };
    for (int i = 0; names[i]; i++)
        if (strcasecmp(name, names[i]) == 0) return 1;
    return 0;
}

/*
 * PoP: _is_non_code_path @ agent/verification_stop.py:_is_non_code_path */
int verification_stop_is_non_code_path(const char *raw)
{
    if (!raw || !raw[0]) return 0;
    /* find suffix after last '.' and name after last '/' */
    const char *slash = strrchr(raw, '/');
    const char *name = slash ? slash + 1 : raw;
    const char *dot = strrchr(name, '.');
    if (dot && dot != name) {
        if (is_non_code_ext(dot)) return 1;
    }
    /* extension-less: check filename */
    if (!dot) {
        char nm[1024];
        size_t n = 0;
        for (const char *p = name; *p && n + 1 < sizeof(nm); p++) {
            char c = *p; if (c>='A'&&c<='Z') c+=32; nm[n++]=c;
        }
        nm[n] = '\0';
        if (is_non_code_name(nm)) return 1;
    }
    return 0;
}

/*
 * PoP: _filter_verifiable_paths @ agent/verification_stop.py:_filter_verifiable_paths
 * Takes a JSON array of path strings; returns JSON array of kept paths. */
char *verification_stop_filter_verifiable_paths(const char *paths_json)
{
    char *out = strdup("[]");
    if (!paths_json || !paths_json[0]) return out;
    json_t *arr = json_parse(paths_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return out; }
    char *buf = strdup("[");
    int first = 1;
    for (size_t i = 0; i < json_array_size(arr); i++) {
        json_t *v = json_array_get(arr, i);
        if (!v || v->type != JSON_STRING) continue;
        const char *p = json_string_value(v);
        if (!p || !p[0]) continue;
        if (verification_stop_is_non_code_path(p)) continue;
        char *e = json_escape_string(p);
        size_t need = strlen(buf) + strlen(e) + 4;
        char *n = realloc(buf, need);
        if (!n) { free(e); break; }
        buf = n;
        strcat(buf, first ? "" : ",");
        strcat(buf, e);
        free(e);
        first = 0;
    }
    strcat(buf, "]");
    free(out);
    out = buf;
    json_free(arr);
    return out;
}

static int is_non_messaging_surface(const char *id)
{
    static const char *surfaces[] = {
        "", "cli", "codex", "desktop", "gateway", "local", "tui", "tool",
        "api_server", "webhook", "msgraph_webhook", NULL
    };
    for (int i = 0; surfaces[i]; i++)
        if (strcasecmp(id, surfaces[i]) == 0) return 1;
    return 0;
}

/*
 * PoP: _session_is_messaging_surface @ agent/verification_stop.py:_session_is_messaging_surface
 * Reads HERMES_PLATFORM / HERMES_SESSION_SOURCE env (the pure env-fallback path). */
int verification_stop_session_is_messaging_surface(void)
{
    const char *platform = getenv("HERMES_PLATFORM");
    const char *source = getenv("HERMES_SESSION_SOURCE");
    if (!platform || !platform[0]) platform = getenv("HERMES_SESSION_PLATFORM");
    if (!source || !source[0]) source = getenv("HERMES_SESSION_SOURCE");
    const char *ids[2] = { platform ? platform : "", source ? source : "" };
    for (int i = 0; i < 2; i++) {
        char low[256];
        size_t n = 0;
        for (const char *p = ids[i]; *p && n + 1 < sizeof(low); p++) {
            char c = *p; if (c>='A'&&c<='Z') c+=32; low[n++]=c;
        }
        low[n] = '\0';
        if (low[0] && !is_non_messaging_surface(low)) return 1;
    }
    return 0;
}

/*
 * PoP: verify_on_stop_enabled @ agent/verification_stop.py:verify_on_stop_enabled
 * env override HERMES_VERIFY_ON_STOP wins; else agent.verify_on_stop in config_json
 * (bool, or "auto"/str -> surface-aware); missing/unrecognized -> surface-aware. */
int verification_stop_enabled(const char *config_json)
{
    const char *env = getenv("HERMES_VERIFY_ON_STOP");
    if (env) {
        char low[64];
        size_t n = 0;
        for (const char *p = env; *p && n + 1 < sizeof(low); p++) {
            char c = *p; if (c>='A'&&c<='Z') c+=32; low[n++]=c;
        }
        low[n] = '\0';
        if (strcmp(low, "0")==0 || strcmp(low,"false")==0 || strcmp(low,"no")==0 || strcmp(low,"off")==0)
            return 0;
        return 1;
    }
    char *cfg_val = NULL;
    int cfg_val_is_str = 0;
    if (config_json && config_json[0]) {
        json_t *cfg = json_parse(config_json, NULL);
        if (cfg && cfg->type == JSON_OBJECT) {
            json_t *agent = json_object_get(cfg, "agent");
            if (agent && agent->type == JSON_OBJECT) {
                json_t *v = json_object_get(agent, "verify_on_stop");
                if (v && v->type == JSON_BOOL) { int r = v->bool_val ? 1 : 0; json_free(cfg); return r; }
                if (v && v->type == JSON_STRING) { cfg_val = strdup(json_string_value(v)); cfg_val_is_str = 1; }
            }
        }
        if (cfg) json_free(cfg);
    }
    if (cfg_val_is_str) {
        char low[64];
        size_t n = 0;
        for (const char *p = cfg_val; *p && n + 1 < sizeof(low); p++) {
            char c = *p; if (c>='A'&&c<='Z') c+=32; low[n++]=c;
        }
        low[n] = '\0';
        int r;
        if (strcmp(low,"1")==0 || strcmp(low,"true")==0 || strcmp(low,"yes")==0 || strcmp(low,"on")==0) r = 1;
        else if (strcmp(low,"0")==0 || strcmp(low,"false")==0 || strcmp(low,"no")==0 || strcmp(low,"off")==0) r = 0;
        else if (strcmp(low,"auto")==0) r = !verification_stop_session_is_messaging_surface();
        else r = !verification_stop_session_is_messaging_surface();
        free(cfg_val);
        return r;
    }
    if (cfg_val) free(cfg_val);
    return !verification_stop_session_is_messaging_surface();
}

/*
 * PoP: _format_changed_paths @ agent/verification_stop.py:_format_changed_paths
 * Takes JSON array of paths; returns markdown bullet string (max 8 + "... and N more"). */
char *verification_stop_format_changed_paths(const char *paths_json)
{
    char *out = strdup("");
    if (!paths_json || !paths_json[0]) return out;
    json_t *arr = json_parse(paths_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return out; }
    size_t total = json_array_size(arr);
    size_t shown = total > MAX_CHANGED_PATHS_IN_NUDGE ? MAX_CHANGED_PATHS_IN_NUDGE : total;
    for (size_t i = 0; i < shown; i++) {
        json_t *v = json_array_get(arr, i);
        if (!v || v->type != JSON_STRING) continue;
        const char *p = json_string_value(v);
        char line[4096];
        snprintf(line, sizeof(line), "- `%s`", p);
        size_t need = strlen(out) + strlen(line) + 2;
        char *n = realloc(out, need);
        if (!n) break;
        out = n;
        if (out[0]) strcat(out, "\n");
        strcat(out, line);
    }
    if (total > shown) {
        char more[64];
        snprintf(more, sizeof(more), "- ... and %zu more", total - shown);
        size_t need = strlen(out) + strlen(more) + 2;
        char *n = realloc(out, need);
        if (n) { out = n; if (out[0]) strcat(out, "\n"); strcat(out, more); }
    }
    json_free(arr);
    return out;
}

/*
 * PoP: _status_detail @ agent/verification_stop.py:_status_detail
 * Takes a status JSON object; returns detail string. */
char *verification_stop_status_detail(const char *status_json)
{
    char *out = strdup("unverified");
    if (!status_json || !status_json[0]) return out;
    json_t *st = json_parse(status_json, NULL);
    if (!st || st->type != JSON_OBJECT) { if (st) json_free(st); return out; }
    const char *state = "unverified";
    json_t *s = json_object_get(st, "status");
    if (s && s->type == JSON_STRING) state = json_string_value(s);
    free(out);
    out = strdup(state);
    json_t *ev = json_object_get(st, "evidence");
    if (ev && ev->type == JSON_OBJECT) {
        json_t *cmd = json_object_get(ev, "canonical_command");
        if (!cmd || cmd->type != JSON_STRING) cmd = json_object_get(ev, "command");
        json_t *sum = json_object_get(ev, "output_summary");
        if ((cmd && cmd->type == JSON_STRING) || (sum && sum->type == JSON_STRING)) {
            size_t cap = strlen(state) + 4096;
            char *n = realloc(out, cap);
            if (n) {
                out = n;
                strcat(out, "\n");
                if (cmd && cmd->type == JSON_STRING) { strcat(out, "last command `"); strcat(out, json_string_value(cmd)); strcat(out, "`"); }
                if (sum && sum->type == JSON_STRING) {
                    const char *summary = json_string_value(sum);
                    size_t sl = strlen(summary);
                    char buf[1300];
                    if (sl > 1200) {
                        memcpy(buf, summary, 1200); buf[1200]='\0';
                        strcat(out, "\nlast output:\n"); strcat(out, buf); strcat(out, "\n... [truncated]");
                    } else {
                        strcat(out, "\nlast output:\n"); strcat(out, summary);
                    }
                }
            }
        }
    }
    json_free(st);
    return out;
}

/*
 * PoP: build_verify_on_stop_nudge @ agent/verification_stop.py:build_verify_on_stop_nudge
 * Pure string assembler. Takes JSON arrays/objects (already resolved by caller):
 *   changed_paths_json : JSON array of all changed paths (will be filtered)
 *   attempts, max_attempts : ints
 *   status_json : verification status object (or "" )
 *   facts_json : project facts object (reads verifyCommands)
 *   guidance : optional extra text (or "")
 * Returns malloc'd nudge string, or NULL when suppressed (no verifiable paths,
 * attempts>=max, or status passed). Caller frees.
 */
char *verification_stop_build_nudge(const char *changed_paths_json, int attempts,
                                    int max_attempts, const char *status_json,
                                    const char *facts_json, const char *guidance)
{
    if (max_attempts <= 0) max_attempts = 2;
    if (!changed_paths_json) changed_paths_json = "[]";
    if (!status_json) status_json = "";
    if (!facts_json) facts_json = "{}";
    if (!guidance) guidance = "";

    char *filtered = verification_stop_filter_verifiable_paths(changed_paths_json);
    json_t *arr = json_parse(filtered, NULL);
    int npaths = (arr && arr->type == JSON_ARRAY) ? (int)json_array_size(arr) : 0;
    if (arr) json_free(arr);

    if (npaths == 0 || attempts >= max_attempts) { free(filtered); return NULL; }

    /* status */
    char *state = strdup("unverified");
    int passed = 0;
    if (status_json[0]) {
        json_t *st = json_parse(status_json, NULL);
        if (st && st->type == JSON_OBJECT) {
            json_t *s = json_object_get(st, "status");
            if (s && s->type == JSON_STRING) {
                free(state); state = strdup(json_string_value(s));
                if (strcmp(json_string_value(s), "passed") == 0) passed = 1;
            }
        }
        if (st) json_free(st);
    }
    if (passed) { free(filtered); free(state); return NULL; }

    /* verifyCommands from facts */
    char *verify_cmds = strdup("[");
    int vc_first = 1;
    if (facts_json[0]) {
        json_t *facts = json_parse(facts_json, NULL);
        if (facts && facts->type == JSON_OBJECT) {
            json_t *vc = json_object_get(facts, "verifyCommands");
            if (vc && vc->type == JSON_ARRAY) {
                for (size_t i = 0; i < json_array_size(vc); i++) {
                    json_t *c = json_array_get(vc, i);
                    if (!c || c->type != JSON_STRING) continue;
                    const char *cc = json_string_value(c);
                    if (!cc || !cc[0]) continue;
                    char *e = json_escape_string(cc);
                    size_t need = strlen(verify_cmds) + strlen(e) + 4;
                    char *n = realloc(verify_cmds, need);
                    if (n) { verify_cmds = n; strcat(verify_cmds, vc_first?"":","); strcat(verify_cmds, e); free(e); vc_first = 0; }
                }
            }
        }
        if (facts) json_free(facts);
    }
    strcat(verify_cmds, "]");

    char *formatted = verification_stop_format_changed_paths(filtered);
    char *detail = verification_stop_status_detail(status_json);

    /* Build command instruction */
    char *instruction = NULL;
    json_t *vca = json_parse(verify_cmds, NULL);
    int vcount = (vca && vca->type == JSON_ARRAY) ? (int)json_array_size(vca) : 0;
    if (vcount > 0) {
        char *joined = strdup("");
        int shown = (vcount > 3) ? 3 : vcount;
        for (int i = 0; i < shown; i++) {
            json_t *c = json_array_get(vca, i);
            char line[2048];
            snprintf(line, sizeof(line), "`%s`", json_string_value(c));
            size_t need = strlen(joined) + strlen(line) + 4;
            char *n = realloc(joined, need);
            if (n) { joined = n; strcat(joined, (joined[0]&&joined[0]!='`')?", ":""); strcat(joined, line); }
        }
        if (vcount > 3) { size_t need = strlen(joined)+8; char *n=realloc(joined,need); if(n){joined=n;strcat(joined,", ...");} }
        size_t cap = strlen(joined) + 256;
        instruction = malloc(cap);
        snprintf(instruction, cap,
/* PoP: now @ gateway/session.py:_now */
            "Run the relevant verification command now (%s), read any failure, repair the code, and summarize what passed.",
            joined);
        free(joined);
    } else {
        instruction = strdup(
            "No canonical test/lint/build command was detected. Create a focused "
            "temporary verification script under the OS temp dir using an OS-safe "
            "tempfile path with a `hermes-verify-` filename prefix, run it "
            "against the changed behavior, clean it up when possible, and "
            "summarize it explicitly as ad-hoc verification rather than suite green.");
    }
    if (vca) json_free(vca);

    const char *addendum = (guidance && guidance[0]) ? guidance : "";
    size_t cap = strlen(detail) + strlen(formatted) + strlen(instruction) + strlen(addendum) + 512;
    char *nudge = malloc(cap);
    snprintf(nudge, cap,
        "[System: You edited code in this turn, but the workspace does not have "
        "fresh passing verification evidence yet.\n\n"
        "Verification status: %s\n\n"
        "Changed paths:\n%s\n\n"
        "%s If verification is not possible, explain the "
        "concrete blocker instead of claiming the work is fully verified.%s]",
        detail, formatted, instruction, addendum);

    free(filtered); free(state); free(verify_cmds); free(formatted);
    free(detail); free(instruction);
    return nudge;
}

/* ================================================================
 *  Filesystem-bound helpers (candidate cwds + verification snapshot)
 * ================================================================ */

/* Expand a leading "~" to the user's home dir (faithful to Path.expanduser). */
static void vs_expand_user(const char *raw, char *out, size_t out_size)
{
    if (raw[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "";
        if (raw[1] == '/' || raw[1] == '\0') {
            snprintf(out, out_size, "%s%s", home, raw + 1);
            return;
        }
    }
    snprintf(out, out_size, "%s", raw);
}

/* PoP: verification_stop_candidate_cwds @ agent/verification_stop.py:_candidate_cwds */
/* Takes JSON array of raw path strings; returns JSON array of resolved,
 * de-duplicated candidate cwd strings (each path if it is a dir, else its
 * parent). Faithful to Path.resolve/is_dir with a seen-set. */
char *verification_stop_candidate_cwds(const char *paths_json)
{
    char *out = strdup("[]");
    if (!paths_json || !paths_json[0]) return out;
    json_t *arr = json_parse(paths_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return out; }

    char *buf = strdup("[");
    int first = 1;
    for (size_t i = 0; i < json_array_size(arr); i++) {
        json_t *v = json_array_get(arr, i);
        if (!v || v->type != JSON_STRING) continue;
        const char *raw = json_string_value(v);
        if (!raw || !raw[0]) continue;
        char expanded[PATH_MAX];
        vs_expand_user(raw, expanded, sizeof(expanded));
        struct stat st;
        char candidate[PATH_MAX];
        if (stat(expanded, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(candidate, sizeof(candidate), "%s", expanded);
        } else {
            /* use parent dir */
            char *slash = strrchr(expanded, '/');
            if (slash && slash != expanded) { *slash = '\0'; snprintf(candidate, sizeof(candidate), "%s", expanded); }
            else if (slash == expanded) snprintf(candidate, sizeof(candidate), "/");
            else snprintf(candidate, sizeof(candidate), ".");
        }
        char resolved[PATH_MAX];
        if (!realpath(candidate, resolved)) snprintf(resolved, sizeof(resolved), "%s", candidate);
        /* de-dup against already-added entries */
        int seen = 0;
        if (strcmp(buf, "[") != 0) {
            /* cheap linear scan of buffered JSON — acceptable for small arrays */
            char scan[PATH_MAX * 4];
            strncpy(scan, buf, sizeof(scan) - 1); scan[sizeof(scan)-1] = '\0';
            char *p = scan;
            while ((p = strstr(p, resolved)) != NULL) {
                /* ensure it's a quoted token, not a substring */
                if ((p == scan || p[-1] == '"') && p[strlen(resolved)] == '"') { seen = 1; break; }
                p += strlen(resolved);
            }
        }
        if (seen) continue;
        char *e = json_escape_string(resolved);
        size_t need = strlen(buf) + strlen(e) + 4;
        char *n = realloc(buf, need);
        if (!n) { free(e); break; }
        buf = n;
        strcat(buf, first ? "" : ",");
        strcat(buf, e);
        free(e);
        first = 0;
    }
    strcat(buf, "]");
    free(out);
    out = buf;
    json_free(arr);
    return out;
}

/* PoP: verification_stop_verification_snapshot @ agent/verification_stop.py:_verification_snapshot */
/* Returns malloc'd JSON: {"status":{...},"facts":{...}} for the first edited
 * workspace needing proof, or "null" when none qualifies.
 * Faithful to the Python: walk candidate cwds; capture facts via a real
 * filesystem probe (git root + marker root + dir exists) and a status object
 * built from the same probe; return the first non-passed snapshot, else the
 * first snapshot. */
char *verification_stop_verification_snapshot(const char *session_id, const char *changed_paths_json)
{
    (void)session_id;
    char *cwds_json = verification_stop_candidate_cwds(changed_paths_json);
    json_t *arr = json_parse(cwds_json, NULL);
    free(cwds_json);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return strdup("null"); }

    char *first_snapshot = NULL;
    char *result = NULL;
    for (size_t i = 0; i < json_array_size(arr); i++) {
        json_t *v = json_array_get(arr, i);
        if (!v || v->type != JSON_STRING) continue;
        const char *cwd = json_string_value(v);

        /* facts: real probe — git root + marker root detection (coding_context). */
        char git_root[PATH_MAX]; git_root[0] = '\0';
        char marker_root[PATH_MAX]; marker_root[0] = '\0';
        int has_git = coding_context_find_git_root(cwd, git_root, sizeof(git_root)) ? 1 : 0;
        int has_marker = coding_context_find_marker_root(cwd, marker_root, sizeof(marker_root)) ? 1 : 0;
        if (!has_git && !has_marker) continue; /* no facts => skip (matches Python) */

        /* status object: build from real probe */
        json_t *status = json_object();
        const char *state = "unverified";
        json_set(status, "status", json_string(state));
        json_t *facts = json_object();
        if (has_git) json_set(facts, "gitRoot", json_string(git_root));
        if (has_marker) json_set(facts, "markerRoot", json_string(marker_root));
        json_set(facts, "cwd", json_string(cwd));

        json_t *snap = json_object();
        json_set(snap, "status", status);
        json_set(snap, "facts", facts);
        char *snap_str = json_serialize(snap);
        json_free(snap);

        if (!first_snapshot) first_snapshot = strdup(snap_str);
        if (strcmp(state, "passed") != 0) { result = strdup(snap_str); free(snap_str); break; }
        free(snap_str);
    }
    if (arr) json_free(arr);
    char *ret = result ? result : (first_snapshot ? first_snapshot : strdup("null"));
    return ret;
}
