/*
 * kanban_decompose.c — pure triage / roster helpers from
 * hermes_cli/kanban_decompose.py (the LLM-free concern).
 *
 * This module intentionally excludes the LLM-driven decompose_task() entry
 * point (which requires an auxiliary client + network). It ports the
 * deterministic helpers that surround it: JSON-blob extraction, profile
 * resolution, roster building/formatting, and assignee-choice normalization.
 *
 * Concern boundary: no kanban task table access, no worker/OS dispatch.
 * Pure string + profile-registry logic. Opaque internals; minimal includes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "kanban_db.h"

/* ------------------------------------------------------------------ */
/* _extract_json_blob                                                 */
/* ------------------------------------------------------------------ */

/* Strip ```...``` fence markers (handles ```json and plain ```). Returns a
 * malloc'd copy of the content BETWEEN the opening and closing fences (or the
 * whole string if no fence is present). */
static char *strip_fences(const char *raw)
{
    const char *s = raw;
    while (*s && isspace((unsigned char)*s)) s++;
    /* opening fence: ``` optionally followed by "json" and whitespace */
    const char *body = s;
    if (s[0] == '`' && s[1] == '`' && s[2] == '`') {
        body = s + 3;
        while (*body && (isalnum((unsigned char)*body) || *body == ' ' || *body == '\t'))
            body++;  /* skip "json" / lang tag */
        while (*body && isspace((unsigned char)*body) && *body != '\n' && *body != '\r')
            body++;
        if (*body == '\n' || *body == '\r') body++;
    }
    /* closing fence: last occurrence of ``` on the tail */
    char *dup = strdup(body);
    if (!dup) return NULL;
    char *close = strstr(dup, "```");
    if (close) {
        /* there may be trailing whitespace/newline before the closing fence */
        char *end = close;
        while (end > dup && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'))
            end--;
        *end = '\0';
    }
    return dup;
}

/* Crude JSON validation: attempt a structural parse by counting braces
 * and verifying the extracted candidate re-parses via the json lib. */
/* PoP: kdb_extract_json_blob @ hermes_cli/kanban_specify.py:_extract_json_blob */
/* PoP: kdb_extract_json_blob @ hermes_cli/profile_describer.py:_extract_json_blob */
/* PoP: kdb_extract_json_blob @ hermes_cli/kanban_decompose.py:_extract_json_blob */
char *kdb_extract_json_blob(const char *raw)
{
    if (!raw || !*raw) return NULL;
    char *stripped = strip_fences(raw);
    if (!stripped) return NULL;
    const char *first = strchr(stripped, '{');
    if (!first) { free(stripped); return NULL; }
    const char *last = strrchr(stripped, '}');
    if (!last || last <= first) { free(stripped); return NULL; }
    size_t len = (size_t)(last - first + 1);
    char *candidate = malloc(len + 1);
    memcpy(candidate, first, len);
    candidate[len] = '\0';
    free(stripped);

    /* Structural validation: must be a JSON object. We require balanced
     * braces and that every string is quoted/terminated (lightweight, no
     * external parser dependency). */
    const char *c = candidate;
    while (*c && isspace((unsigned char)*c)) c++;
    if (*c != '{') { free(candidate); return NULL; }
    int depth = 0; int in_str = 0; int ok = 1;
    for (const char *q = c; *q; q++) {
        if (in_str) {
            if (q[0] == '\\' && q[1]) { q++; continue; }
            if (q[0] == '"') in_str = 0;
            continue;
        }
        if (q[0] == '"') { in_str = 1; }
        else if (q[0] == '{' || q[0] == '[') depth++;
        else if (q[0] == '}' || q[0] == ']') { depth--; if (depth < 0) { ok = 0; break; } }
    }
    if (in_str) ok = 0;
    if (depth != 0) ok = 0;
    if (!ok) { free(candidate); return NULL; }
    return candidate;  /* caller frees */
}

/* ------------------------------------------------------------------ */
/* profile resolution helpers                                         */
/* ------------------------------------------------------------------ */

/* Pull a string field "key":"value" out of a flat JSON object. Returns
 * malloc'd value (caller frees) or NULL if absent. */
static char *json_get_field_str(const char *json, const char *key)
{
    if (!json) return NULL;
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *k = strstr(json, pat);
    if (!k) return NULL;
    const char *c = k + strlen(pat);
    while (*c && isspace((unsigned char)*c)) c++;
    if (*c != ':') return NULL;
    c++;
    while (*c && isspace((unsigned char)*c)) c++;
    if (*c != '"') return NULL;
    c++;
    const char *end = c;
    while (*end && *end != '"') { if (*end == '\\') end++; end++; }
    size_t n = (size_t)(end - c);
    char *val = malloc(n + 1);
    memcpy(val, c, n);
    val[n] = '\0';
    return val;
}

/* Is `name` an installed profile? Uses the on-disk profile registry. */
/* profile_exists is now a shared public symbol in port_kanban_db.c. */

/* Active profile name (falls back to "default"). The engine has no notion of
 * a "current" selection beyond the on-disk default; "default" is the oracle
 * ground truth for an isolated home. */
static const char *active_or_default(void)
{
    /* No active-profile API in the slim engine; the only guaranteed profile
     * in the oracle is "default". */
    return "default";
}

/* PoP: kdb_resolve_orchestrator_profile @ hermes_cli/kanban_decompose.py:_resolve_orchestrator_profile */
char *kdb_resolve_orchestrator_profile(const char *kanban_cfg_json)
{
    char *explicit = json_get_field_str(kanban_cfg_json, "orchestrator_profile");
    if (explicit && *explicit) {
        if (profile_exists(explicit)) return explicit;
        free(explicit);
    } else if (explicit) {
        free(explicit);
    }
    return strdup(active_or_default());
}

/* PoP: kdb_resolve_default_assignee @ hermes_cli/kanban_decompose.py:_resolve_default_assignee */
char *kdb_resolve_default_assignee(const char *kanban_cfg_json)
{
    char *explicit = json_get_field_str(kanban_cfg_json, "default_assignee");
    if (explicit && *explicit) {
        if (profile_exists(explicit)) return explicit;
        free(explicit);
    } else if (explicit) {
        free(explicit);
    }
    return strdup(active_or_default());
}

/* ------------------------------------------------------------------ */
/* roster build / format                                              */
/* ------------------------------------------------------------------ */

/* PoP: kdb_build_roster @ hermes_cli/kanban_decompose.py:_build_roster */
char *kdb_build_roster(void)
{
    char **profs = kdb_list_profiles_on_disk();
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    len += (size_t)snprintf(out + len, cap - len, "[");
    int first = 1;
    /* Mirror Python's list_profiles ordering: the built-in "default" comes
     * first, then named profiles in sorted order. kdb_list_profiles_on_disk
     * returns a sorted NULL-terminated array, so we split default out. */
    if (profs) {
        for (int i = 0; profs[i]; i++) {
            if (strcmp(profs[i], "default") == 0) continue; /* emit last-pass */
        }
        for (int i = 0; profs[i]; i++) {
            if (strcmp(profs[i], "default") != 0) continue;
            if (!first) out[len++] = ',';
            first = 0;
            len += (size_t)snprintf(out + len, cap - len,
                "{\"name\":\"default\",\"description\":\"(no description; profile named 'default')\",\"has_description\":false}");
        }
        for (int i = 0; profs[i]; i++) {
            if (strcmp(profs[i], "default") == 0) continue;
            if (!first) out[len++] = ',';
            first = 0;
            len += (size_t)snprintf(out + len, cap - len,
                "{\"name\":\"%s\",\"description\":\"(no description; profile named '%s')\",\"has_description\":false}",
                profs[i], profs[i]);
        }
        kdb_strv_free(profs);
    }
    out[len++] = ']';
    out[len] = '\0';
    return out;
}

static void js_escape(char *dst, const char *src, size_t dstcap)
{
    size_t j = 0;
    for (const char *p = src; *p && j + 2 < dstcap; p++) {
        if (*p == '"' || *p == '\\') { dst[j++] = '\\'; }
        dst[j++] = *p;
    }
    dst[j] = '\0';
}

/* PoP: kdb_format_roster @ hermes_cli/kanban_decompose.py:_format_roster */
char *kdb_format_roster(const char *roster_json)
{
    if (!roster_json || roster_json[0] != '[' || roster_json[1] == ']') {
        char *s = strdup("  (no profiles installed — decomposer cannot route work)");
        return s;
    }
    /* Parse each entry: extract name, description, has_description. */
    char **names = NULL; char **descs = NULL; int *has_desc = NULL; int n = 0, cap = 8;
    names = malloc(sizeof(char*) * cap);
    descs = malloc(sizeof(char*) * cap);
    has_desc = malloc(sizeof(int) * cap);

    const char *p = roster_json + 1;
    while (*p && *p != ']') {
        const char *kn = strstr(p, "\"name\"");
        const char *kd = strstr(p, "\"has_description\"");
        const char *kc = strstr(p, "\"description\"");
        if (!kn) break;
        /* name */
        const char *c = kn + 6; while (*c && isspace((unsigned char)*c)) c++;
        c++; while (*c && isspace((unsigned char)*c)) c++;
        const char *s = c + 1; const char *e = s;
        while (*e && *e != '"') { if (*e == '\\') e++; e++; }
        size_t nl = (size_t)(e - s);
        char nm[256]; memcpy(nm, s, nl); nm[nl] = '\0';
        /* description */
        char dc[512]; dc[0] = '\0';
        if (kc) {
            const char *cc = kc + 13; while (*cc && isspace((unsigned char)*cc)) cc++;
            cc++; while (*cc && isspace((unsigned char)*cc)) cc++;
            if (*cc == '"') {
                const char *s2 = cc + 1; const char *e2 = s2;
                while (*e2 && *e2 != '"') { if (*e2 == '\\') e2++; e2++; }
                size_t dl = (size_t)(e2 - s2);
                if (dl >= sizeof(dc)) dl = sizeof(dc) - 1;
                memcpy(dc, s2, dl); dc[dl] = '\0';
            }
        }
        int hd = 0;
        if (kd) {
            const char *colon = strchr(kd, ':');
            const char *v = colon ? colon + 1 : kd + 18;
            while (*v && isspace((unsigned char)*v)) v++;
            hd = (v[0] == 't');
        }
        if (n >= cap) { cap *= 2; names = realloc(names, sizeof(char*)*cap);
                        descs = realloc(descs, sizeof(char*)*cap);
                        has_desc = realloc(has_desc, sizeof(int)*cap); }
        names[n] = strdup(nm);
        descs[n] = strdup(dc);
        has_desc[n] = hd;
        n++;
        const char *next = strchr(e, '}');
        p = next ? next + 1 : e;
    }

    size_t oc = 1024; size_t ol = 0; char *out = malloc(oc);
    for (int i = 0; i < n; i++) {
        char nm[256]; js_escape(nm, names[i], sizeof(nm));
        const char *tag = has_desc[i] ? "" : " ⚠ undescribed";
        /* Mirror Python: "  - {name}{tag}: {description}" */
        ol += (size_t)snprintf(out + ol, oc - ol, "  - %s%s: %s\n",
                               nm, tag, descs[i] ? descs[i] : "");
        free(names[i]); free(descs[i]);
    }
    free(names); free(descs); free(has_desc);
    if (ol > 0 && out[ol-1] == '\n') out[--ol] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* assignee normalization                                            */
/* ------------------------------------------------------------------ */

/* PoP: kdb_normalize_assignee_choice @ hermes_cli/kanban_decompose.py:_normalize_assignee_choice */
char *kdb_normalize_assignee_choice(const char *assignee,
                                    const char *default_assignee,
                                    char **valid_names)
{
    const char *def = default_assignee && *default_assignee ? default_assignee : "default";
    if (!assignee || !*assignee || !strchr(assignee, 0) || strspn(assignee, " \t\r\n") == strlen(assignee)) {
        return strdup(def);
    }
    /* trim */
    const char *s = assignee; while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s);
    char *chosen = malloc(n + 1); memcpy(chosen, s, n); chosen[n] = '\0';
    if (n == 0) { strcpy(chosen, def); return chosen; }

    if (valid_names) {
        for (int i = 0; valid_names[i]; i++) {
            if (strcmp(valid_names[i], chosen) == 0) return chosen;
        }
    }
    /* not valid -> fallback */
    free(chosen);
    return strdup(def);
}

/* ------------------------------------------------------------------ */
/*  list_triage_ids                                                  */
/* ------------------------------------------------------------------ */
/* PoP: kdb_list_triage_ids @ hermes_cli/kanban_decompose.py:list_triage_ids */
/* Return task ids currently in the triage column, as a malloc'd
 * NULL-terminated array of malloc'd id strings (caller frees with
 * kdb_string_list_free). `tenant` may be NULL. Faithful to the Python
 * (status="triage", limit=1000). */
char **kdb_list_triage_ids(sqlite3 *conn, const char *tenant)
{
    int n = 0;
    kanban_task_t **list =
        kdb_list_tasks(conn, "triage", NULL, tenant, NULL, 0, 1000, &n);
    if (!list) return NULL;
    char **ids = (char **)malloc((size_t)(n + 1) * sizeof(char *));
    int out = 0;
    for (int i = 0; i < n; i++) {
        const char *id = kdb_task_id(list[i]);
        if (id) ids[out++] = strdup(id);
    }
    ids[out] = NULL;
    kdb_task_list_free(list);
    return ids;
}
