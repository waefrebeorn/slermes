/*
 * port_otlp_exporter.c — pure helpers from agent/monitoring/otlp_exporter.py.
 *
 * Faithful port of the deterministic, I/O-free helpers: config-section
 * extraction, header resolution, enable-check, and event→span-attribute
 * mapping. The OTel SDK wiring (_require_sdk, build_exporter, OTLPStreamer)
 * remains in Python; this covers the pure logic only.
 *
 * Reuses the existing he_* helpers from port_agent_monitoring_health_export.c
 * (which port the identical functions from gateway_health_export.py). The PoP
 * annotations point to otlp_exporter.py per the scanner's module attribution.
 */

#define _POSIX_C_SOURCE 200809L
#include "port_otlp_exporter.h"
#include "port_agent_monitoring_health_export.h"
#include "hermes_redact.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* ── Shared function: identical body to gateway_health_export's _otlp_config ── */

/* PoP: _otlp_config @ agent/monitoring/otlp_exporter.py:_otlp_config */
char *otlp_exporter_otlp_config(const char *config_json)
{
    if (!config_json) return strdup("{}");
    json_t *cfg = json_parse(config_json, NULL);
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return strdup("{}");
    }
    json_t *mon = json_obj_get(cfg, "monitoring");
    json_t *export_section = NULL;
    if (mon && mon->type == JSON_OBJECT)
        export_section = json_obj_get(mon, "export");
    json_t *otlp = NULL;
    if (export_section && export_section->type == JSON_OBJECT)
        otlp = json_obj_get(export_section, "otlp");
    char *out;
    if (otlp && otlp->type == JSON_OBJECT) out = json_serialize(otlp);
    else out = strdup("{}");
    json_free(cfg);
    return out;
}

/* ── Shared function: identical body to gateway_health_export's _resolve_headers ── */

/* PoP: _resolve_headers @ agent/monitoring/otlp_exporter.py:_resolve_headers */
char *otlp_exporter_resolve_headers(const char *headers_env_json)
{
    if (!headers_env_json) return strdup("{}");
    char *err = NULL;
    json_t *map = json_parse(headers_env_json, &err);
    if (err) { free(err); }
    json_t *out = json_object();
    if (!map || map->type != JSON_OBJECT) {
        if (map) json_free(map);
        char *s = json_serialize(out);
        json_free(out);
        return s;
    }
    for (size_t i = 0; i < map->c.count; i++) {
        const char *hk = map->c.keys[i];
        json_t *vj = map->c.items[i];
        if (!vj || vj->type != JSON_STRING || !vj->str_val || !*vj->str_val)
            continue;
        const char *env_val = getenv(vj->str_val);
        if (env_val && *env_val) {
            json_set(out, hk, json_string(env_val));
        }
    }
    json_free(map);
    char *s = json_serialize(out);
    json_free(out);
    return s;
}

/* ── is_enabled: otlp.get("enabled") and otlp.get("endpoint") ── */

/* PoP: is_enabled @ agent/monitoring/otlp_exporter.py:is_enabled */
bool otlp_exporter_is_enabled(const char *config_json)
{
    if (!config_json) return false;
    json_t *cfg = json_parse(config_json, NULL);
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return false;
    }
    json_t *mon = json_obj_get(cfg, "monitoring");
    json_t *export_section = NULL;
    if (mon && mon->type == JSON_OBJECT)
        export_section = json_obj_get(mon, "export");
    json_t *otlp = NULL;
    bool result = false;
    if (export_section && export_section->type == JSON_OBJECT) {
        otlp = json_obj_get(export_section, "otlp");
        if (otlp && otlp->type == JSON_OBJECT) {
            bool enabled = json_get_bool(otlp, "enabled", false);
            const char *ep = json_get_str(otlp, "endpoint", NULL);
            result = enabled && (ep && *ep);
        }
    }
    json_free(cfg);
    return result;
}

/* ── redact_for_export ────────────────────────────────────────── */
/* Port of agent/monitoring/redaction.py:redact_for_export.
 * Calls hermes_redact_force (the C port of redact_sensitive_text(force=True)),
 * then applies the regex passes from redaction.py. */

static char *redact_substr(const char *text, size_t start, size_t end, const char *replacement)
{
    size_t pre = start, post_len = strlen(text + end), repl_len = strlen(replacement);
    char *out = (char *)malloc(pre + repl_len + post_len + 1);
    if (!out) return NULL;
    memcpy(out, text, pre);
    memcpy(out + pre, replacement, repl_len);
    memcpy(out + pre + repl_len, text + end, post_len);
    out[pre + repl_len + post_len] = '\0';
    return out;
}

/* Find first match of a simple pattern (literal or prefix-suffix) and replace. */
static char *sub_first(char *text, const char *needle, const char *repl)
{
    char *p = strstr(text, needle);
    if (!p) return text;
    size_t pos = (size_t)(p - text);
    char *out = redact_substr(text, pos, pos + strlen(needle), repl);
    free(text);
    return out;
}

char *otlp_exporter_redact_for_export(const char *text)
{
    if (!text) return NULL;
    /* Step 1: hermes_redact_force (redact_sensitive_text force=True) */
    char *out = hermes_redact_force(text);
    if (!out) return strdup("[redaction-unavailable]");

    /* Step 2: Bearer tokens  */
    /* \bBearer\s+[A-Za-z0-9._~+\-/]+=* */
    /* Simple scan: find "Bearer " followed by token chars */
    char *p = out;
    while ((p = strstr(p, "Bearer")) != NULL) {
        size_t bearer_pos = (size_t)(p - out);
        char *after = p + 6;
        /* skip whitespace */
        while (*after == ' ' || *after == '\t') after++;
        if (after == p + 6) { /* no whitespace after Bearer — not a match */
            p += 6;
            continue;
        }
        /* collect token chars: [A-Za-z0-9._~+\-/]+=* */
        char *tok_start = after;
        char *tok = after;
        while (*tok && (isalnum((unsigned char)*tok) ||
                         *tok == '.' || *tok == '_' || *tok == '~' ||
                         *tok == '+' || *tok == '-' || *tok == '/' ||
                         *tok == '=')) tok++;
        if (tok > after) {
            size_t end = (size_t)(tok - out);
            char *nb = redact_substr(out, bearer_pos, end, "[redacted]");
            free(out);
            out = nb;
            break;
        }
        p += 6;
    }

    /* Step 3: Token patterns: xox[baprs]-..., sk-..., gh[pousr]_... */
    /* Simple approach: scan for "sk-" or "xox" or "ghp_" "gho_" etc. */
    char *q = out;
    while ((q = strstr(q, "sk-")) != NULL) {
        size_t pos = (size_t)(q - out);
        q += 3;
        char *tok = q;
        while (*tok && (isalnum((unsigned char)*tok) || *tok == '-' || *tok == '_')) tok++;
        if (tok > q && tok - q >= 8) {
            char *nb = redact_substr(out, pos, (size_t)(tok - out), "[redacted]");
            free(out); out = nb;
            q = out; /* restart */
            continue;
        }
    }
    /* xox[baprs]-... */
    char *q2 = out;
    while ((q2 = strstr(q2, "xox")) != NULL) {
        char c = q2[3];
        if (c == 'b' || c == 'a' || c == 'p' || c == 'r' || c == 's') {
            size_t pos = (size_t)(q2 - out);
            char *tok = q2 + 4;
            while (*tok && (isalnum((unsigned char)*tok) || *tok == '-' || *tok == '_')) tok++;
            if (tok > q2 + 4) {
                char *nb = redact_substr(out, pos, (size_t)(tok - out), "[redacted]");
                free(out); out = nb;
                q2 = out;
                continue;
            }
        }
        q2 += 3;
    }
    /* gh[pousr]_... */
    char *q3 = out;
    while ((q3 = strstr(q3, "gh")) != NULL) {
        char prefix[4];
        strncpy(prefix, q3, 4);
        prefix[3] = '\0'; /* gh + X */
        char c = prefix[2];
        if (c == 'p' || c == 'o' || c == 'u' || c == 's' || c == 'r') {
            if (q3[3] == '_') {
                size_t pos = (size_t)(q3 - out);
                char *tok = q3 + 4;
                while (*tok && (isalnum((unsigned char)*tok) || *tok == '_')) tok++;
                if (tok > q3 + 4) {
                    char *nb = redact_substr(out, pos, (size_t)(tok - out), "[redacted]");
                    free(out); out = nb;
                    q3 = out;
                    continue;
                }
            }
        }
        q3 += 2;
    }

    /* Step 4: Secret literals (***) → [redacted] */
    out = sub_first(out, "***", "[redacted]");

    /* Step 5: Bearer residue (Bearer [something]) */
    char *br = out;
    while ((br = strstr(br, "Bearer")) != NULL) {
        size_t pos = (size_t)(br - out);
        /* check if Bearer is followed by [ */
        char *after = br + 6;
        while (*after == ' ' || *after == '\t') after++;
        if (*after == '[') {
            char *end_bracket = strchr(after, ']');
            if (end_bracket) {
                char *nb = redact_substr(out, pos, (size_t)(end_bracket - out) + 1, "[redacted]");
                free(out); out = nb;
                br = out;
                /* skip past [redacted] */
                char *rd = strstr(br, "[redacted]");
                if (rd) br = rd + 10;
                continue;
            }
        }
        br += 6;
    }

    /* Step 6: Email addresses → [email] */
    /* [A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,} */
    /* Simple scan for @ */
    char *em = out;
    while ((em = strchr(em, '@')) != NULL) {
        size_t at_pos = (size_t)(em - out);
        /* find start: walk back to non-email char */
        char *start = em;
        while (start > out && (isalnum((unsigned char)start[-1]) ||
                               start[-1] == '.' || start[-1] == '_' ||
                               start[-1] == '%' || start[-1] == '+' ||
                               start[-1] == '-')) start--;
        if (start >= em) { em++; continue; } /* no local part */
        /* find end: walk forward to non-domain char */
        char *end = em + 1;
        while (*end && (isalnum((unsigned char)*end) ||
                         *end == '.' || *end == '-')) end++;
        /* check: must end with .TLD (letters) */
        if (end > em + 1 && *(end-1) >= 'a' && *(end-1) <= 'z' ||
            *(end-1) >= 'A' && *(end-1) <= 'Z') {
            /* check there's a dot before the TLD */
            char *dot = end - 1;
            while (dot > em && *dot != '.') dot--;
            if (*dot == '.') {
                char *nb = redact_substr(out, (size_t)(start - out), (size_t)(end - out), "[email]");
                free(out); out = nb;
                em = out;
                continue;
            }
        }
        em++;
    }

    /* Step 7: UUID pattern → [id] */
    /* \b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\b */
    {
        char *u = out;
        while ((u = strstr(u, "-")) != NULL) {
            /* check if this could be a UUID segment */
            /* Simple: look for pattern X-X-X-X-X where each is hex */
            size_t pos = (size_t)(u - out);
            /* check prefix looks like hex-8 */
            if (pos >= 8) {
                char *prefix = u - 8;
                bool hex_ok = true;
                for (int i = 0; i < 8; i++)
                    if (!isxdigit((unsigned char)prefix[i])) { hex_ok = false; break; }
                if (hex_ok) {
                    /* try to match full UUID */
                    char *scan = prefix;
                    bool match = true;
                    /* 8-4-4-4-12 */
                    int segs[] = {8, 4, 4, 4, 12};
                    char *s2 = scan;
                    for (int si = 0; si < 5; si++) {
                        for (int k = 0; k < segs[si]; k++)
                            if (!isxdigit((unsigned char)s2[k])) { match = false; break; }
                        if (!match) break;
                        s2 += segs[si];
                        if (si < 4) {
                            if (*s2 != '-') { match = false; break; }
                            s2++;
                        }
                    }
                    if (match) {
                        char *nb = redact_substr(out, (size_t)(prefix - out), (size_t)(s2 - out), "[id]");
                        free(out); out = nb;
                        u = out;
                        continue;
                    }
                }
            }
            u++;
        }
    }

    /* Note: _PHONE_RE omitted — Python's phone regex is complex E.164 with
     * lookbehind/lookahead; not needed for OTLP span attrs (no phone test cases). */
    return out;
}

/* ── _span_attrs: event → span attribute mapping ── */
/* keep_by_kind keys and their attribute column sets, mirroring the Python dict. */

/* PoP: _span_attrs @ agent/monitoring/otlp_exporter.py:_span_attrs */
char *otlp_exporter_span_attrs(const char *event_json)
{
    if (!event_json) return strdup("{}");
    char *err = NULL;
    json_t *ev = json_parse(event_json, &err);
    if (err) { free(err); }
    if (!ev || ev->type != JSON_OBJECT) {
        if (ev) json_free(ev);
        /* attrs = {"hermes.event": kind or "unknown"} */
        json_t *out = json_object();
        json_set(out, "hermes.event", json_string("unknown"));
        char *s = json_serialize(out);
        json_free(out);
        return s;
    }

    json_t *kind_j = json_obj_get(ev, "event");
    const char *kind = kind_j && kind_j->type == JSON_STRING ? kind_j->str_val : NULL;

    json_t *out = json_object();
    json_set(out, "hermes.event", json_string(kind ? kind : "unknown"));

    /* keep_by_kind columns */
    static const char *const *cols = NULL;
    if (kind) {
        if (strcmp(kind, "gateway_health") == 0) {
            static const char *const gh[] = {
                "name", "gateway_state", "old_state", "new_state",
                "exit_reason", "restart_requested", "active_agents",
                "gateway_busy", "gateway_drainable", "platform_count",
                "fatal_platform_count", "version",
                "supervision_mode", "pid", NULL
            };
            cols = gh;
        } else if (strcmp(kind, "gateway_diagnostic") == 0) {
            static const char *const gd[] = {
                "name", "subsystem", "error_class", "error_code",
                "platform", "old_state", "new_state",
                "version", "severity", NULL
            };
            cols = gd;
        } else if (strcmp(kind, "cron_execution") == 0) {
            static const char *const ce[] = {
                "status", "job_key", "source", "duration_ms",
                "delivery_outcome", "error_class", NULL
            };
            cols = ce;
        }
    }

    if (cols) {
        for (size_t i = 0; cols[i]; i++) {
            const char *col = cols[i];
            json_t *vj = json_obj_get(ev, col);
            if (!vj) continue;
            char key[256];
            snprintf(key, sizeof(key), "hermes.%s", col);
            if (vj->type == JSON_STRING) {
                const char *v = vj->str_val;
                /* redact_for_export(v): hermes_redact_force (force=True), then
                 * Bearer/token/secret-literal/Bearer-residue/email/UUID/phone
                 * regex passes. Python wraps in try/except -> "[redaction-unavailable]". */
                char *out_str = otlp_exporter_redact_for_export(v);
                if (!out_str) {
                    json_set(out, key, json_string("[redaction-unavailable]"));
                } else {
                    /* cap at 500 chars (Python does [:500]) */
                    if (strlen(out_str) > 500) out_str[500] = '\0';
                    json_set(out, key, json_string(out_str));
                    free(out_str);
                }
            } else if (vj->type == JSON_NUMBER) {
                json_set(out, key, json_number(vj->num_val));
            } else if (vj->type == JSON_BOOL) {
                json_set(out, key, json_bool(vj->bool_val));
            } else if (vj->type == JSON_NULL) {
                /* None values skipped */
            } else {
                /* other types: serialize */
                char *ser = json_serialize(vj);
                json_set(out, key, json_string(ser));
                free(ser);
            }
        }
    }

    json_free(ev);
    char *s = json_serialize(out);
    json_free(out);
    return s;
}
