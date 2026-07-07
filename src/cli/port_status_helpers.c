/*
 * port_status_helpers.c
 *
 * Pure, portable helper functions ported from gateway/status.py.
 * These contain no /proc reads, no file I/O, no os.getpid() capture, no
 * mutex/lock syscalls — only string parsing, JSON building, hashing and
 * boolean state derivation. Process/filesystem-coupled helpers
 * (_read_process_cmdline, _looks_like_gateway_process, _get_process_start_time,
 * _read_json_file, file-lock acquire/release, pid-file write/remove,
 * terminate_pid) stay REAL_GAP.
 *
 * C name <- python name (module prefix 'status_'):
 *   status_scope_hash                       <- _scope_hash
 *   status_gateway_command_subcommand       <- _gateway_command_subcommand
 *   status_looks_like_gateway_command_line  <- looks_like_gateway_command_line
 *   status_looks_like_gateway_runtime_command_line <- looks_like_gateway_runtime_command_line
 *   status_profile_name_for_home            <- _profile_name_for_home
 *   status_command_line_belongs_to_profile  <- _command_line_belongs_to_profile
 *   status_pid_from_record                  <- _pid_from_record
 *   status_parse_active_agents              <- parse_active_agents
 *   status_derive_gateway_busy              <- derive_gateway_busy
 *   status_derive_gateway_drainable         <- derive_gateway_drainable
 *   status_build_pid_record                 <- _build_pid_record (explicit args)
 *   status_build_runtime_status_record      <- _build_runtime_status_record (explicit args)
 */

#include "hermes_json.h"
#include "libcrypto/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>

#define _GATEWAY_KIND "gateway"

static char *json_escape_local(const char *s)
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

static const char *pat_concat(const char *home_lc)
{
    static char buf[2 * PATH_MAX];
    snprintf(buf, sizeof(buf), "hermes_home=%s", home_lc);
    return buf;
}

/* ---------------------------------------------------------------------------
 * _scope_hash: sha256(identity) hexdigest[:16]
 * --------------------------------------------------------------------------- */
/*
 * PoP: _scope_hash @ gateway/status.py:_scope_hash
 * Returns malloc'd 16-char hex string. Caller frees. */
char *status_scope_hash(const char *identity)
{
    unsigned char raw[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)(identity ? identity : ""),
                  strlen(identity ? identity : ""), raw);
    char *full = crypto_hex_encode(raw, CRYPTO_SHA256_LEN);
    char *out = malloc(17);
    memcpy(out, full, 16);
    out[16] = '\0';
    free(full);
    return out;
}

/* ---------------------------------------------------------------------------
 * _gateway_command_subcommand: tokenize (quote-aware, slash+case normalized),
 * strip leading "gateway/run.py" / "hermes-gateway[.exe]" -> "run", else strip
 * --profile/-p selectors and return the token after "gateway" (default "run").
 * --------------------------------------------------------------------------- */
static void tokenize_shlex(const char *command, char **tokens, int *ntok, int max)
{
    *ntok = 0;
    if (!command || !command[0]) return;
    const char *p = command;
    char buf[8192];
    size_t bi = 0;
    bool in_q = false;
    char qc = 0;
    while (*p && *ntok < max) {
        if (in_q) {
            if (*p == qc) { in_q = false; }
            else { buf[bi++] = *p; }
            p++;
            continue;
        }
        if (*p == '"' || *p == '\'') { in_q = true; qc = *p; p++; continue; }
        if (*p == ' ' || *p == '\t' || *p == '\n') {
            if (bi) { buf[bi]='\0'; tokens[(*ntok)++] = strdup(buf); bi=0; }
            p++;
            continue;
        }
        buf[bi++] = *p++;
    }
    if (bi) { buf[bi]='\0'; tokens[(*ntok)++] = strdup(buf); }
    /* normalize each token: strip quotes, backslash->slash, lowercase */
    for (int i = 0; i < *ntok; i++) {
        char *t = tokens[i];
        /* strip surrounding quotes */
        size_t L = strlen(t);
        if (L >= 2 && (t[0]=='"'||t[0]=='\'') && t[L-1]==t[0]) {
            memmove(t, t+1, L-2); t[L-2]='\0';
        }
        for (char *q = t; *q; q++) {
            if (*q == '\\') *q = '/';
            *q = (char)tolower((unsigned char)*q);
        }
    }
}

/* returns malloc'd subcommand string ("run", "restart", ...) or NULL. Caller frees. */
/*
 * PoP: _gateway_command_subcommand @ gateway/status.py:_gateway_command_subcommand */
char *status_gateway_command_subcommand(const char *command)
{
    char *tokens[256];
    int ntok = 0;
    tokenize_shlex(command, tokens, &ntok, 256);
    if (ntok == 0) {
        for (int i = 0; i < ntok; i++) free(tokens[i]);
        return NULL;
    }
    /* Gateway-dedicated entrypoints carry no subcommand. */
    for (int i = 0; i < ntok; i++) {
        const char *t = tokens[i];
        const char *base = strrchr(t, '/');
        base = base ? base + 1 : t;
        if (strcmp(t, "gateway/run.py") == 0 || strcmp(base, "gateway/run.py") == 0) {
            for (int j = 0; j < ntok; j++) free(tokens[j]);
            return strdup("run");
        }
        if (strcmp(base, "hermes-gateway") == 0 || strcmp(base, "hermes-gateway.exe") == 0) {
            for (int j = 0; j < ntok; j++) free(tokens[j]);
            return strdup("run");
        }
    }
    /* joined check for hermes_cli.main / hermes_cli/main.py / hermes(.exe) */
    bool has_gateway_entry = false;
    char joined[16384]; joined[0]='\0';
    for (int i = 0; i < ntok; i++) {
        const char *base = strrchr(tokens[i], '/');
        base = base ? base + 1 : tokens[i];
        if (strcmp(base, "hermes") == 0 || strcmp(base, "hermes.exe") == 0) has_gateway_entry = true;
        strcat(joined, " "); strcat(joined, tokens[i]);
    }
    if (strstr(joined, "hermes_cli.main") || strstr(joined, "hermes_cli/main.py")) has_gateway_entry = true;
    if (!has_gateway_entry) {
        for (int j = 0; j < ntok; j++) free(tokens[j]);
        return NULL;
    }
    /* Drop --profile/-p selectors (and their values). */
    char *filtered[256];
    int nf = 0;
    for (int i = 0; i < ntok; i++) {
        if (strcmp(tokens[i], "--profile") == 0 || strcmp(tokens[i], "-p") == 0) {
            i++; /* skip value */
            continue;
        }
        if (strncmp(tokens[i], "--profile=", 10) == 0) continue;
        if (strncmp(tokens[i], "-p=", 3) == 0) continue;
        filtered[nf++] = tokens[i];
    }
    char *result = NULL;
    for (int i = 0; i < nf; i++) {
        if (strcmp(filtered[i], "gateway") != 0) continue;
        if (i + 1 >= nf) { result = strdup("run"); break; }
        result = strdup(filtered[i + 1]);
        break;
    }
    for (int j = 0; j < ntok; j++) free(tokens[j]);
    return result;
}

/*
 * PoP: looks_like_gateway_command_line @ gateway/status.py:looks_like_gateway_command_line */
int status_looks_like_gateway_command_line(const char *command)
{
    char *sub = status_gateway_command_subcommand(command);
    int r = (sub && strcmp(sub, "run") == 0);
    if (sub) free(sub);
    return r;
}

/*
 * PoP: looks_like_gateway_runtime_command_line @ gateway/status.py:looks_like_gateway_runtime_command_line */
int status_looks_like_gateway_runtime_command_line(const char *command)
{
    char *sub = status_gateway_command_subcommand(command);
    int r = (sub && (strcmp(sub, "run") == 0 || strcmp(sub, "restart") == 0));
    if (sub) free(sub);
    return r;
}

/* ---------------------------------------------------------------------------
 * _profile_name_for_home: parent dir == "profiles" -> that name, else NULL.
 * --------------------------------------------------------------------------- */
/*
 * PoP: _profile_name_for_home @ gateway/status.py:_profile_name_for_home
 * Takes a home path string; returns malloc'd profile name or NULL. */
char *status_profile_name_for_home(const char *profile_home)
{
    if (!profile_home || !profile_home[0]) return NULL;
    /* find parent name: last '/' before the final segment */
    const char *slash = strrchr(profile_home, '/');
    if (!slash) return NULL;
    /* parent = segment before slash */
    char parent[PATH_MAX];
    size_t len = (size_t)(slash - profile_home);
    if (len >= sizeof(parent)) len = sizeof(parent) - 1;
    memcpy(parent, profile_home, len);
    parent[len] = '\0';
    const char *pname = strrchr(parent, '/');
    pname = pname ? pname + 1 : parent;
    if (strcmp(pname, "profiles") == 0) {
        const char *name = slash + 1; /* final segment */
        return strdup(name);
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * _command_line_belongs_to_profile
 * --------------------------------------------------------------------------- */
/*
 * PoP: _command_line_belongs_to_profile @ gateway/status.py:_command_line_belongs_to_profile
 * command: argv string; profile_home: HERMES_HOME path. Returns 1/0. */
int status_command_line_belongs_to_profile(const char *command, const char *profile_home)
{
    if (!command) command = "";
    if (!profile_home) profile_home = "";
    char cmd_lc[16384];
    size_t i = 0;
    for (const char *p = command; *p && i + 1 < sizeof(cmd_lc); p++) {
        char c = *p; if (c>='A'&&c<='Z') c+=32; cmd_lc[i++]=c;
    }
    cmd_lc[i] = '\0';
    char home_lc[PATH_MAX];
    i = 0;
    for (const char *p = profile_home; *p && i + 1 < sizeof(home_lc); p++) {
        char c = *p; if (c>='A'&&c<='Z') c+=32; home_lc[i++]=c;
    }
    home_lc[i] = '\0';

    char *profile_name = status_profile_name_for_home(profile_home);
    int result;
    if (profile_name && strcmp(profile_name, "default") != 0) {
        char prof_lc[PATH_MAX];
        i = 0;
        for (const char *p = profile_name; *p && i + 1 < sizeof(prof_lc); p++) {
            char c = *p; if (c>='A'&&c<='Z') c+=32; prof_lc[i++]=c;
        }
        prof_lc[i] = '\0';
        char pat1[PATH_MAX], pat2[PATH_MAX], pat3[PATH_MAX];
        snprintf(pat1, sizeof(pat1), "--profile %s", prof_lc);
        snprintf(pat2, sizeof(pat2), "-p %s", prof_lc);
        snprintf(pat3, sizeof(pat3), "hermes_home=%s", home_lc);
        result = (strstr(cmd_lc, pat1) || strstr(cmd_lc, pat2) || strstr(cmd_lc, pat3)) ? 1 : 0;
    } else {
        /* default/root: no profile flag, no conflicting HERMES_HOME= */
        if (strstr(cmd_lc, "--profile ") || strstr(cmd_lc, " -p ")) result = 0;
        else if (strstr(cmd_lc, "hermes_home=") && !strstr(cmd_lc, pat_concat(home_lc))) result = 0;
        else result = 1;
    }
    if (profile_name) free(profile_name);
    return result;
}

/* ---------------------------------------------------------------------------
 * _pid_from_record: int(record["pid"]) or None
 * --------------------------------------------------------------------------- */
/*
 * PoP: _pid_from_record @ gateway/status.py:_pid_from_record
 * Takes a JSON record string; returns pid (>=0) or -1 if absent/invalid. */
int status_pid_from_record(const char *record_json)
{
    if (!record_json || !record_json[0]) return -1;
    json_t *r = json_parse(record_json, NULL);
    if (!r || r->type != JSON_OBJECT) { if (r) json_free(r); return -1; }
    int out = -1;
    json_t *pid = json_object_get(r, "pid");
    if (pid && pid->type == JSON_NUMBER) out = (int)json_number_value(pid);
    else if (pid && pid->type == JSON_STRING) out = (int)strtol(json_string_value(pid), NULL, 10);
    json_free(r);
    return out;
}

/* ---------------------------------------------------------------------------
 * parse_active_agents: max(0, int(raw)) else 0
 * --------------------------------------------------------------------------- */
/*
 * PoP: parse_active_agents @ gateway/status.py:parse_active_agents
 * Takes a JSON number/string or raw int; returns clamped non-negative int. */
int status_parse_active_agents(const char *raw_json)
{
    if (!raw_json || !raw_json[0]) return 0;
    json_t *v = json_parse(raw_json, NULL);
    int out = 0;
    if (v) {
        if (v->type == JSON_NUMBER) out = (int)json_number_value(v);
        else if (v->type == JSON_STRING) out = (int)strtol(json_string_value(v), NULL, 10);
        json_free(v);
    } else {
        out = (int)strtol(raw_json, NULL, 10);
    }
    if (out < 0) out = 0;
    return out;
}

/* ---------------------------------------------------------------------------
 * derive_gateway_busy / derive_gateway_drainable
 * --------------------------------------------------------------------------- */
/*
 * PoP: derive_gateway_busy @ gateway/status.py:derive_gateway_busy
 * Args: gateway_running (0/1), gateway_state (JSON string), active_agents (raw). */
int status_derive_gateway_busy(int gateway_running, const char *gateway_state, const char *active_agents)
{
    if (!gateway_running) return 0;
    if (!gateway_state || strcmp(gateway_state, "running") != 0) return 0;
    json_t *v = active_agents ? json_parse(active_agents, NULL) : NULL;
    int n = 0;
    if (v) {
        if (v->type == JSON_NUMBER) n = (int)json_number_value(v);
        else if (v->type == JSON_STRING) n = (int)strtol(json_string_value(v), NULL, 10);
        json_free(v);
    }
    return n > 0 ? 1 : 0;
}

/*
 * PoP: derive_gateway_drainable @ gateway/status.py:derive_gateway_drainable */
int status_derive_gateway_drainable(int gateway_running, const char *gateway_state)
{
    if (!gateway_running) return 0;
    if (!gateway_state || strcmp(gateway_state, "running") != 0) return 0;
    return 1;
}

/* ---------------------------------------------------------------------------
 * _build_pid_record / _build_runtime_status_record (explicit args, pure)
 * --------------------------------------------------------------------------- */
/*
 * PoP: _build_pid_record @ gateway/status.py:_build_pid_record
 * Builds the pid record JSON from explicit values. Returns malloc'd JSON.
 * argv_json: a JSON array of argv strings. Caller frees. */
char *status_build_pid_record(int pid, const char *argv_json, long start_time)
{
    if (!argv_json) argv_json = "[]";
    char *out = malloc(8192);
    snprintf(out, 8192,
        "{\"pid\":%d,\"kind\":%s,\"argv\":%s,\"start_time\":%ld}",
        pid, json_escape_local(_GATEWAY_KIND), argv_json, start_time);
    return out;
}

/*
 * PoP: _build_runtime_status_record @ gateway/status.py:_build_runtime_status_record
 * Builds runtime status record JSON from explicit values. Returns malloc'd JSON.
 * Caller frees. */
char *status_build_runtime_status_record(int pid, const char *argv_json, long start_time,
                                         const char *updated_at)
{
    char *pidrec = status_build_pid_record(pid, argv_json, start_time);
    if (!updated_at) updated_at = "";
    char *out = malloc(12288);
    snprintf(out, 12288,
        "{\"pid\":%d,\"kind\":%s,\"argv\":%s,\"start_time\":%ld,"
        "\"gateway_state\":\"starting\",\"exit_reason\":null,\"restart_requested\":false,"
        "\"active_agents\":0,\"platforms\":{},\"updated_at\":%s}",
        pid, json_escape_local(_GATEWAY_KIND), argv_json, start_time, json_escape_local(updated_at));
    free(pidrec);
    return out;
}
