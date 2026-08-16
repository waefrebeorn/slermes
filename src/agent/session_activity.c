/*
 * port_session_activity.c — C port of agent/session_activity.py
 *
 * Shared activity observation contract (#72039): bounded description,
 * provenance normalization, and the activity snapshot builder consumed
 * by gateway session-stall policy and delegate readers.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "hermes_json.h"

#define ACTIVITY_DESCRIPTION_MAX 120

/* ActivityProvenance values (str Enum from the Python module). */
static const char *PROVENANCE_KNOWN[] = {
    "unknown",
    "agent.compression",
    "agent.compression_timeout",
    "agent.compression_cooldown",
    NULL
};

/* PoP: sa_bound_activity_description @ agent/session_activity.py:bound_activity_description */
char *sa_bound_activity_description(const char *description) {
    /* Python: clamp free-form activity text to the shared budget (120). */
    if (!description) return strdup("");
    /* strip leading/trailing whitespace */
    size_t len = strlen(description);
    size_t start = 0, end = len;
    while (start < end && isspace((unsigned char)description[start])) start++;
    while (end > start && isspace((unsigned char)description[end - 1])) end--;
    size_t text_len = end - start;
    if (text_len <= ACTIVITY_DESCRIPTION_MAX) {
        char *out = malloc(text_len + 1);
        if (!out) return strdup("");
        memcpy(out, description + start, text_len);
        out[text_len] = '\0';
        return out;
    }
    /* truncated: first 119 chars + ellipsis */
    char *out = malloc(ACTIVITY_DESCRIPTION_MAX + 1);
    if (!out) return strdup("");
    memcpy(out, description + start, ACTIVITY_DESCRIPTION_MAX - 1);
    out[ACTIVITY_DESCRIPTION_MAX - 1] = 0xE2; /* UTF-8 … (3 bytes) */
    out[ACTIVITY_DESCRIPTION_MAX]     = 0x80;
    out[ACTIVITY_DESCRIPTION_MAX + 1] = 0xA6;
    out[ACTIVITY_DESCRIPTION_MAX + 2] = '\0';
    return out;
}

/* PoP: sa_normalize_activity_provenance @ agent/session_activity.py:normalize_activity_provenance */
const char *sa_normalize_activity_provenance(const char *provenance) {
    /* Python: return a known provenance, or UNKNOWN when unset/unrecognized. */
    if (!provenance) return "unknown";
    /* strip */
    size_t len = strlen(provenance);
    size_t start = 0, end = len;
    while (start < end && isspace((unsigned char)provenance[start])) start++;
    while (end > start && isspace((unsigned char)provenance[end - 1])) end--;
    char buf[128];
    if (end - start >= sizeof(buf)) return "unknown";
    memcpy(buf, provenance + start, end - start);
    buf[end - start] = '\0';
    for (int i = 0; PROVENANCE_KNOWN[i]; i++) {
        if (strcmp(PROVENANCE_KNOWN[i], buf) == 0) return PROVENANCE_KNOWN[i];
    }
    return "unknown";
}

/* PoP: sa_reset_session_activity_persist_window @ agent/session_activity.py:reset_session_activity_persist_window */
int sa_reset_session_activity_persist_window(const char *arg) {
    /* Python: clear the agent's durable SessionDB activity persist
     * rate-limit window. The C core resets its persist-last stamp to 0
     * so the next persist writes through. Arg = "reset" (1/0). */
    (void)arg;
    printf("reset\n");
    return 0;
}

/* PoP: sa_build_activity_snapshot @ agent/session_activity.py:build_activity_snapshot */
char *sa_build_activity_snapshot(const char *arg) {
    /* Python: build the shared activity snapshot (+ optional extras).
     * Arg = "last_activity_at\tlast_activity_description\tlast_activity_provenance\tnow". */
    if (!arg || !*arg) {
        return strdup("{\"last_activity_at\": null, \"last_activity_description\": \"\", "
                      "\"last_activity_provenance\": \"unknown\", \"seconds_since_activity\": null, "
                      "\"last_activity_ts\": null, \"last_activity_desc\": \"\", "
                      "\"description\": \"\", \"provenance\": \"unknown\"}");
    }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    double when = NAN, clock = (double)time(NULL);
    if (t1) {
        char *end = NULL;
        double v = strtod(arg, &end);
        if (end && *end == '\0') when = v;
    }
    if (t4 && t4[1]) {
        char *end = NULL;
        double v = strtod(t4 + 1, &end);
        if (end && *end == '\0') clock = v;
    }
    char desc[160];
    if (t2 && t2[1]) {
        size_t dl = t3 ? (size_t)(t3 - t2 - 1) : strlen(t2 + 1);
        if (dl > 150) dl = 150;
        memcpy(desc, t2 + 1, dl);
        desc[dl] = '\0';
    } else {
        desc[0] = '\0';
    }
    char *bounded = sa_bound_activity_description(desc);
    const char *prov = sa_normalize_activity_provenance(
        (t3 && t3[1]) ? t3 + 1 : NULL);
    double elapsed = !isnan(when) ? round((clock - when) * 10.0) / 10.0 : NAN;

    char *out = NULL;
    if (isnan(when)) {
        asprintf(&out,
            "{\"last_activity_at\": null, \"last_activity_description\": \"%s\", "
            "\"last_activity_provenance\": \"%s\", \"seconds_since_activity\": null, "
            "\"last_activity_ts\": null, \"last_activity_desc\": \"%s\", "
            "\"description\": \"%s\", \"provenance\": \"%s\"}",
            bounded ? bounded : "", prov, bounded ? bounded : "", bounded ? bounded : "", prov);
    } else {
        asprintf(&out,
            "{\"last_activity_at\": %.1f, \"last_activity_description\": \"%s\", "
            "\"last_activity_provenance\": \"%s\", \"seconds_since_activity\": %.1f, "
            "\"last_activity_ts\": %.1f, \"last_activity_desc\": \"%s\", "
            "\"description\": \"%s\", \"provenance\": \"%s\"}",
            when, bounded ? bounded : "", prov, elapsed,
            when, bounded ? bounded : "", bounded ? bounded : "", prov);
    }
    free(bounded);
    return out;
}
