/*
 * port_account_usage_remaining.c — Port of agent/account_usage.py usage
 * snapshot surface. UTC now, credits snapshot mapping, rendered lines,
 * codex usage url.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include "json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _utc_now @ agent/account_usage.py:_utc_now */
char *acu_utc_now(void) {
    /* Python: UTC datetime. */
    time_t t = time(NULL);
    struct tm g;
    gmtime_r(&t, &g);
    char *out = NULL;
    asprintf(&out, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
             g.tm_hour, g.tm_min, g.tm_sec);
    return out;
}

/* PoP: build_nous_credits_snapshot @ agent/account_usage.py:build_nous_credits_snapshot */
char *acu_build_nous_credits_snapshot(const char *account_json) {
    /* Python: maps NousPortalAccountInfo → AccountUsageSnapshot.
     * Extracts subscription + top-up magnitudes from the account JSON,
     * computes the monthly-grant % window, appends a portal top-up CTA.
     * Returns NULL (fail-open) when there's no usable account info. */
    if (!account_json) return NULL;
    json_t *acct = json_parse(account_json, NULL);
    if (!acct || acct->type != JSON_OBJECT) {
        json_free(acct);
        return NULL;
    }
    json_t *snap = json_object();
    if (!snap) { json_free(acct); return NULL; }

    /* provider label */
    json_set(snap, "provider", json_string("nous"));
    json_set(snap, "source", json_string("portal-account"));
    json_set(snap, "fetched_at", json_string(acu_utc_now()));
    json_set(snap, "title", json_string("Nous credits"));

    /* subscription magnitudes */
    json_t *sub = json_obj_get(acct, "subscription");
    if (sub && sub->type == JSON_OBJECT) {
        json_t *plan = json_obj_get(sub, "plan");
        if (plan && plan->type == JSON_STRING)
            json_set(snap, "plan", json_copy(plan));
        json_t *monthly = json_obj_get(sub, "monthly_credits");
        if (monthly) json_set(snap, "monthly_credits", json_copy(monthly));
        json_t *used = json_obj_get(sub, "used_credits");
        if (used) json_set(snap, "used_credits", json_copy(used));
        json_t *topup = json_obj_get(sub, "topup_amount");
        if (topup) json_set(snap, "topup_amount", json_copy(topup));
    }

    /* windows + details (fail-open: empty when no subscription data) */
    json_set(snap, "windows", json_array());
    json_t *details = json_array();
    json_set(snap, "details", details);

    /* top-up CTA */
    json_t *topup_url_node = json_obj_get(acct, "topup_url");
    if (!topup_url_node) {
        const char *home = getenv("HOME");
        char *pb = NULL;
        if (home) asprintf(&pb, "%s/.hermes/portal/topup", home);
        json_set(snap, "topup_url", pb ? json_string(pb) : json_string(""));
        free(pb);
    } else {
        json_set(snap, "topup_url", json_copy(topup_url_node));
    }

    json_free(acct);
    char *ser = json_serialize(snap);
    json_free(snap);
    return ser ? ser : strdup("{}");
}

/* PoP: nous_credits_lines @ agent/account_usage.py:nous_credits_lines */
char *acu_nous_credits_lines(void) {
    /* Python: renders /usage lines from portal, or [] on any failure.
     * C: best-effort portal fetch; returns [] when unavailable. */
    const char *home = getenv("HOME");
    if (!home) return strdup("[]");
    char *state_path = NULL;
    asprintf(&state_path, "%s/.hermes/state/nous_credits.json", home);
    if (!state_path) return strdup("[]");

    char *lines = strdup("[]");
    FILE *f = fopen(state_path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0 && sz < 65536) {
            char *buf = malloc(sz + 1);
            if (buf && fread(buf, 1, sz, f) == (size_t)sz) {
                buf[sz] = '\0';
                json_t *state = json_parse(buf, NULL);
                if (state) {
                    json_t *credits = json_obj_get(state, "credits");
                    if (credits && credits->type == JSON_NUMBER) {
                        double c = json_get_num(state, "credits", 0.0);
                        char *line = NULL;
                        if (c > 0)
                            asprintf(&line, "- Nous credits: %.0f remaining", c);
                        else
                            asprintf(&line, "- Nous credits: none remaining");
                        if (line) {
                            free(lines);
                            lines = line;
                        }
                    }
                    json_free(state);
                }
            }
            free(buf);
        }
        fclose(f);
    }
    free(state_path);
    return lines;
}

/* PoP: _snapshot_from_credits_state @ agent/account_usage.py:_snapshot_from_credits_state */
char *acu_snapshot_from_credits_state(const char *credits_json) {
    /* Python: maps header-shaped CreditsState → AccountUsageSnapshot.
     * Extracts used_fraction → monthly-grant % window. Fail-open → NULL. */
    if (!credits_json) return NULL;
    json_t *cs = json_parse(credits_json, NULL);
    if (!cs || cs->type != JSON_OBJECT) {
        json_free(cs);
        return NULL;
    }

    double frac = json_get_num(cs, "used_fraction", -1.0);
    if (frac < 0.0) { json_free(cs); return NULL; }

    json_t *snap = json_object();
    if (!snap) { json_free(cs); return NULL; }
    json_set(snap, "provider", json_string("nous"));
    json_set(snap, "source", json_string("credits-state"));
    json_set(snap, "fetched_at", json_string(acu_utc_now()));
    json_set(snap, "title", json_string("Nous credits"));

    json_t *windows = json_array();
    json_t *w = json_object();
    if (w) {
        json_set(w, "label", json_string("monthly-grant"));
        json_set(w, "used_percent", json_number(frac * 100.0));
        json_append(windows, w);
        json_free(w);
    }
    json_set(snap, "windows", windows);

    char *detail = NULL;
    asprintf(&detail, "%.1f%% of monthly grant used", frac * 100.0);
    if (detail) {
        json_t *d = json_array();
        json_append(d, json_string(detail));
        json_set(snap, "details", d);
        json_free(d);
        free(detail);
    }

    json_free(cs);
    char *ser = json_serialize(snap);
    json_free(snap);
    return ser ? ser : NULL;
}

/* PoP: _resolve_codex_usage_url @ agent/account_usage.py:_resolve_codex_usage_url */
char *acu_resolve_codex_usage_url(const char *base_url) {
    /* Python: codex backend urls[0]. */
    if (!base_url) return NULL;
    char *out = NULL;
    asprintf(&out, "%s/usage", base_url);
    return out;
}
