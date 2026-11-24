/*
 * admissions.c - C home for persistence/authority/admissions.py.
 * Ports the module's pure helpers: the gate refusal, the refused-verdict envelope, the steering
 * replay predicate and the stored intent/status vocabulary check. Every repository verb is SQL
 * on a live connection and stays Python; the decision it embeds is ported here.
 * PoP: w19_admissions_c58422_raise_gate @ persistence/authority/admissions.py:raise_gate
 * PoP: w19_admissions_c58422_refusal @ persistence/authority/admissions.py:GateRefusal.refusal
 * PoP: w19_admissions_c58422_steer_replayed @ persistence/authority/admissions.py:steer_replayed
 * PoP: w19_admissions_c58422_coerce_record @ persistence/authority/admissions.py:AdmissionRecord.__post_init__
 */
#include "noc_w19_admissions_c58422.h"
#include <stdio.h>
#include <string.h>

/* AdmissionIntent members, in declaration order. */
static const char *const ADMISSION_INTENTS[] = { "queue", "steer", "redirect", NULL };
/* AdmissionStatus members, in declaration order. */
static const char *const ADMISSION_STATUSES[] = { "queued", "started", "unknown", "terminal", NULL };
/* AdmissionOutcome members: the steering pair is what steer_replayed accepts. */
#define STEERING "steering"

static int member_of(const char *const *members, const char *value) {
    if (!value) return 0;
    for (int i = 0; members[i]; i++)
        if (!strcmp(members[i], value)) return 1;
    return 0;
}

/* repr() of the offending value in the enum ValueError: 'x' for a str, None for None. */
static char *repr_text(const char *value) {
    char *out;
    if (!value) {
        out = (char *)malloc(5);
        if (out) memcpy(out, "None", 5);
        return out;
    }
    size_t n = strlen(value);
    out = (char *)malloc(n + 3);
    if (!out) return NULL;
    out[0] = '\'';
    memcpy(out + 1, value, n);
    out[n + 1] = '\'';
    out[n + 2] = '\0';
    return out;
}

static json_t *refused_envelope(void) {
    json_t *out = json_object();
    json_set(out, "raised", json_bool(0));
    json_set(out, "reason", json_null());
    json_set(out, "field", json_null());
    json_set(out, "detail", json_null());
    return out;
}

json_t *w19_admissions_c58422_refusal(const char *reason, const char *field, const char *detail) {
    json_t *out = json_object();
    json_set(out, "reason", json_string(reason ? reason : ""));
    json_set(out, "field", field ? json_string(field) : json_null());
    json_set(out, "detail", detail ? json_string(detail) : json_null());
    return out;
}

json_t *w19_admissions_c58422_raise_gate(int gate_kind, const char *reason,
                                        const char *field, const char *detail) {
    /* decided = gate() if callable(gate) else gate ; None stays open. */
    if (gate_kind == 0 || gate_kind == 2) return refused_envelope();
    json_t *out = w19_admissions_c58422_refusal(reason, field, detail);
    json_set(out, "raised", json_bool(1));
    return out;
}

int w19_admissions_c58422_steer_replayed(const char *status, const char *outcome) {
    if (!status || !outcome) return 0;
    if (strcmp(status, "terminal")) return 0;
    return (!strcmp(outcome, STEERING) || !strcmp(outcome, "steered")) ? 1 : 0;
}

json_t *w19_admissions_c58422_coerce_record(const char *intent, const char *status) {
    json_t *out = json_object();
    if (!member_of(ADMISSION_INTENTS, intent)) {
        char *shown = repr_text(intent);
        char buf[256];
        snprintf(buf, sizeof buf, "%s is not a valid AdmissionIntent", shown ? shown : "");
        free(shown);
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string(buf));
        return out;
    }
    if (!member_of(ADMISSION_STATUSES, status)) {
        char *shown = repr_text(status);
        char buf[256];
        snprintf(buf, sizeof buf, "%s is not a valid AdmissionStatus", shown ? shown : "");
        free(shown);
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string(buf));
        return out;
    }
    json_set(out, "ok", json_bool(1));
    json_set(out, "intent", json_string(intent));
    json_set(out, "status", json_string(status));
    return out;
}
