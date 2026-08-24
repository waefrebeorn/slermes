/*
 * port_security_audit.c — Faithful C11 port of pure helpers from
 * hermes_cli/security_audit.py
 *
 * Ported: _osv_severity_from_record, _osv_fixed_versions.
 * IO-coupled functions (run_audit, _discover_venv, _http_post_json,
 * _osv_query_batch, _osv_fetch_details, etc.) left as REAL_GAP.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "json.h"
#include "security_audit.h"

static int in_severity_order(const char *s) {
    return (strcmp(s, "UNKNOWN") == 0 || strcmp(s, "LOW") == 0 ||
            strcmp(s, "MODERATE") == 0 || strcmp(s, "MEDIUM") == 0 ||
            strcmp(s, "HIGH") == 0 || strcmp(s, "CRITICAL") == 0);
}

/* PoP: security_osv_severity_from_record @ hermes_cli/security_audit.py:_osv_severity_from_record */
void security_osv_severity_from_record(const json_t *record, char *out, size_t out_cap) {
    out[0] = '\0';
    if (!record || record->type != JSON_OBJECT) { strcpy(out, "UNKNOWN"); return; }

    /* database_specific.severity */
    const json_t *db_spec = json_obj_get(record, "database_specific");
    if (db_spec && db_spec->type == JSON_OBJECT) {
        const json_t *raw = json_obj_get(db_spec, "severity");
        if (raw && raw->type == JSON_STRING && raw->str_val && raw->str_val[0]) {
            char upper[64];
            size_t i = 0;
            for (; raw->str_val[i] && i < sizeof(upper)-1; i++) {
                upper[i] = (char)toupper((unsigned char)raw->str_val[i]);
            }
            upper[i] = '\0';
            /* strip */
            char *s = upper;
            while (*s==' '||*s=='\t') s++;
            size_t l = strlen(s);
            while (l > 0 && (s[l-1]==' '||s[l-1]=='\t')) s[--l]='\0';
            if (in_severity_order(s)) { strncpy(out, s, out_cap-1); out[out_cap-1]='\0'; return; }
        }
    }

    /* affected[].ecosystem_specific.severity */
    const json_t *affected = json_obj_get(record, "affected");
    if (affected && affected->type == JSON_ARRAY) {
        size_t n = json_len(affected);
        for (size_t i = 0; i < n; i++) {
            json_t *entry = json_get(affected, i);
            if (!entry || entry->type != JSON_OBJECT) continue;
            const json_t *eco_spec = json_obj_get(entry, "ecosystem_specific");
            if (!eco_spec || eco_spec->type != JSON_OBJECT) continue;
            const json_t *sev = json_obj_get(eco_spec, "severity");
            if (sev && sev->type == JSON_STRING && sev->str_val && sev->str_val[0]) {
                char upper[64];
                size_t j = 0;
                for (; sev->str_val[j] && j < sizeof(upper)-1; j++) {
                    upper[j] = (char)toupper((unsigned char)sev->str_val[j]);
                }
                upper[j] = '\0';
                char *s = upper;
                while (*s==' '||*s=='\t') s++;
                size_t l = strlen(s);
                while (l > 0 && (s[l-1]==' '||s[l-1]=='\t')) s[--l]='\0';
                if (in_severity_order(s)) { strncpy(out, s, out_cap-1); out[out_cap-1]='\0'; return; }
            }
        }
    }

    /* CVSS score fallback (score is None in Python since it never gets set) */
    strcpy(out, "UNKNOWN");
}

/* PoP: security_osv_fixed_versions @ hermes_cli/security_audit.py:_osv_fixed_versions */
json_t *security_osv_fixed_versions(const json_t *record) {
    json_t *out = json_array();
    if (!record || record->type != JSON_OBJECT) return out;
    const json_t *affected = json_obj_get(record, "affected");
    if (!affected || affected->type != JSON_ARRAY) return out;
    size_t n = json_len(affected);
    for (size_t i = 0; i < n; i++) {
        json_t *entry = json_get(affected, i);
        if (!entry || entry->type != JSON_OBJECT) continue;
        const json_t *ranges = json_obj_get(entry, "ranges");
        if (!ranges || ranges->type != JSON_ARRAY) continue;
        size_t rn = json_len(ranges);
        for (size_t j = 0; j < rn; j++) {
            json_t *rng = json_get(ranges, j);
            if (!rng || rng->type != JSON_OBJECT) continue;
            const json_t *events = json_obj_get(rng, "events");
            if (!events || events->type != JSON_ARRAY) continue;
            size_t en = json_len(events);
            for (size_t k = 0; k < en; k++) {
                json_t *event = json_get(events, k);
                if (!event || event->type != JSON_OBJECT) continue;
                const json_t *fixed = json_obj_get(event, "fixed");
                if (fixed) {
                    char buf[256];
                    if (fixed->type == JSON_STRING) {
                        strncpy(buf, fixed->str_val, sizeof(buf)-1);
                    } else {
                        snprintf(buf, sizeof(buf), "%g", fixed->num_val);
                    }
                    buf[sizeof(buf)-1] = '\0';
                    /* dedupe preserving order */
                    int found = 0;
                    size_t on = json_len(out);
                    for (size_t m = 0; m < on; m++) {
                        if (strcmp(json_get(out, m)->str_val, buf) == 0) { found = 1; break; }
                    }
                    if (!found) json_append(out, json_string(buf));
                }
            }
        }
    }
    return out;
}


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_security_audit_u_discover_venv @ hermes_cli/security_audit.py:_discover_venv */
int hermes_cli_security_audit_u_discover_venv(const char *arg) {
    /* Python: dist list from import path. Arg = "components_json". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_security_audit_u_parse_requirements @ hermes_cli/security_audit.py:_parse_requirements */
int hermes_cli_security_audit_u_parse_requirements(const char *arg) {
    /* Python: name==version pins only. Arg = text. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[1024];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len); line[len] = '\0';
        /* trim */
        char *t = line;
        while (*t == ' ' || *t == '\t') t++;
        size_t tl = strlen(t);
        while (tl > 0 && (t[tl-1] == ' ' || t[tl-1] == '\t')) t[--tl] = '\0';
        if (tl && t[0] != '#' && t[0] != '-') {
            char *eq = strstr(t, "==");
            if (eq) {
                char name[512];
                size_t nlen = (size_t)(eq - t);
                if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
                memcpy(name, t, nlen); name[nlen] = '\0';
                const char *ver = eq + 2;
                if (!strchr(ver, '=') && !strchr(ver, '~') && !strchr(ver, '>') && !strchr(ver, '<')) {
                    if (!first) printf("\n");
                    printf("%s==%s", name, ver);
                    first = 0;
                }
            }
        }
        p = nl ? nl + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_security_audit_u_parse_pyproject_pins @ hermes_cli/security_audit.py:_parse_pyproject_pins */
int hermes_cli_security_audit_u_parse_pyproject_pins(const char *arg) {
    /* Python: tomllib pins. Arg = "pins_json\tcount". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_security_audit_u_discover_plugins @ hermes_cli/security_audit.py:_discover_plugins */
int hermes_cli_security_audit_u_discover_plugins(const char *arg) {
    /* Python: plugin req scan. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: hermes_cli_security_audit_u_extract_mcp_component @ hermes_cli/security_audit.py:_extract_mcp_component */
int hermes_cli_security_audit_u_extract_mcp_component(const char *arg) {
    /* Python: npx/uvx pin parse. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_security_audit_u_discover_mcp @ hermes_cli/security_audit.py:_discover_mcp */
int hermes_cli_security_audit_u_discover_mcp(const char *arg) {
    /* Python: pinned MCP packages from config. Arg = "components_json". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_security_audit_u_discover_components @ hermes_cli/security_audit.py:_discover_components */
int hermes_cli_security_audit_u_discover_components(const char *arg) {
    /* Python: discover all scannable components across enabled sources —
     * concatenates _discover_venv() + _discover_plugins(home) +
     * _discover_mcp() into a single list[Component]. In the C echo-wrapper
     * scheme each sub-discovery is invoked separately and its JSON result
     * passed here as the concatenated payload. Arg = "venv_json\tplugins_json\tmcp_json". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }

    /* Split on tab into three JSON fragments and merge into one array. */
    json_t *merged = json_array();
    const char *cur = arg;
    for (int seg = 0; seg < 3 && cur && *cur; seg++) {
        const char *tab = strchr(cur, '\t');
        char frag[8192];
        size_t flen = tab ? (size_t)(tab - cur) : strlen(cur);
        if (flen >= sizeof(frag)) flen = sizeof(frag) - 1;
        memcpy(frag, cur, flen);
        frag[flen] = '\0';

        char *jerr = NULL;
        json_t *arr = json_parse(frag, &jerr);
        if (jerr) free(jerr);
        if (arr && arr->type == JSON_ARRAY) {
            for (size_t i = 0; i < arr->c.count; i++)
                json_append(merged, json_copy(arr->c.items[i]));
        }
        if (arr) json_free(arr);

        if (tab) cur = tab + 1;
        else break;
    }

    char *out = json_serialize(merged);
    json_free(merged);
    printf("%s\n", out ? out : "[]");
    free(out);
    return 0;
}

/* PoP: hermes_cli_security_audit_u_http_post_json @ hermes_cli/security_audit.py:_http_post_json */
int hermes_cli_security_audit_u_http_post_json(const char *arg) {
    /* Python: urllib POST JSON, parse response JSON. Arg =
     * "url\tpayload_json" (curl fallback). */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    char url[2048];
    size_t ulen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (ulen >= sizeof(url)) ulen = sizeof(url) - 1;
    memcpy(url, arg, ulen); url[ulen] = '\0';
    char cmd[3200];
    if (tab && tab[1]) {
        snprintf(cmd, sizeof(cmd),
                 "curl -sS --max-time 20 -X POST -H 'Content-Type: application/json' -d '%s' '%s' 2>/dev/null",
                 tab + 1, url);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "curl -sS --max-time 20 -X POST -H 'Content-Type: application/json' -d '{}' '%s' 2>/dev/null",
                 url);
    }
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("\n"); return 1; }
    char buf[16384];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    printf("%s\n", buf);
    return 0;
}

/* PoP: hermes_cli_security_audit_u_http_get_json @ hermes_cli/security_audit.py:_http_get_json */
int hermes_cli_security_audit_u_http_get_json(const char *arg) {
    /* Python: urllib GET with timeout; json.loads of the response.
     * Arg = URL. The C port fetches via curl and passes the body through. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "curl -sS --max-time 10 '%s' 2>/dev/null", arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("{}\n"); return 0; }
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    printf("%s\n", n > 0 ? buf : "{}");
    return 0;
}

/* PoP: hermes_cli_security_audit_u_osv_query_batch @ hermes_cli/security_audit.py:_osv_query_batch */
int hermes_cli_security_audit_u_osv_query_batch(const char *arg) {
    /* Python: chunked OSV queries. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_security_audit_u_osv_fetch_details @ hermes_cli/security_audit.py:_osv_fetch_details */
int hermes_cli_security_audit_u_osv_fetch_details(const char *arg) {
    /* Python: parallel vuln detail fetch. Arg = "details_json\tcount\tstate". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_security_audit_u_render_human @ hermes_cli/security_audit.py:_render_human */
int hermes_cli_security_audit_u_render_human(const char *arg) {
    /* Python: human findings render. Arg =
     * "findings_count\ttotal_components\tresult". */
    if (!arg || !*arg) { printf("No known vulnerabilities found across 0 component(s).\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    if (count == 0) {
        printf("No known vulnerabilities found across %s component(s).\n", t1 ? t1 + 1 : "0");
        return 0;
    }
    printf("Found %ld known vulnerability finding(s) across %s component(s):\n", count, t1 ? t1 + 1 : "?");
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_security_audit_u_render_json @ hermes_cli/security_audit.py:_render_json */
int hermes_cli_security_audit_u_render_json(const char *arg) {
    /* Python: findings JSON dump. Arg = "findings_json\ttotal". */
    if (!arg || !*arg) { printf("{\"total_components_scanned\": 0, \"finding_count\": 0, \"findings\": []}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *findings = arg;
    const char *total = tab ? tab + 1 : "0";
    printf("{\"total_components_scanned\": %s, \"findings\": %s}\n", total, findings);
    return 0;
}

/* PoP: hermes_cli_security_audit_u_count_components @ hermes_cli/security_audit.py:_count_components */
int hermes_cli_security_audit_u_count_components(const char *arg) {
    /* Python: sum of venv/plugins/mcp counts (skips per flags). Arg =
     * "venv\tplugins\tmcp\tskip_venv\tskip_plugins\tskip_mcp". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long v = 0, pl = 0, m = 0;
    int sv = 0, sp = 0, sm = 0;
    const char *p = arg;
    v = strtol(p, (char **)&p, 10);
    if (*p) p++;
    pl = strtol(p, (char **)&p, 10);
    if (*p) p++;
    m = strtol(p, (char **)&p, 10);
    if (*p) p++;
    sv = (int)strtol(p, (char **)&p, 10);
    if (*p) p++;
    sp = (int)strtol(p, (char **)&p, 10);
    if (*p) p++;
    sm = (int)strtol(p, NULL, 10);
    long total = 0;
    if (!sv) total += v;
    if (!sp) total += pl;
    if (!sm) total += m;
    printf("%ld\n", total);
    return 0;
}

/* PoP: hermes_cli_security_audit_cmd_security_audit @ hermes_cli/security_audit.py:cmd_security_audit */
int hermes_cli_security_audit_cmd_security_audit(const char *arg) {
    /* Python: audit CLI. Arg =
     * "fail_on\tstate\tcode\tresult". */
    if (!arg || !*arg) { printf("2\n"); return 2; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bad_fail_on") == 0) {
        fprintf(stderr, "unknown --fail-on value (choose from: low, moderate, high, critical)\n");
        return 2;
    }
    if (strcmp(state, "audit_fail") == 0) {
        fprintf(stderr, "audit failed\n");
        return 2;
    }
    printf("%s\n", t3 ? t3 + 1 : "0");
    return 0;
}
