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
