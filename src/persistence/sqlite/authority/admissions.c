/*
 * admissions.c - real C11 port of the row -> record projections in upstream Hermes
 * persistence/sqlite/authority/admissions.py. The SQL verbs (_admission_store/_admission_owner/
 * _admission_media calls under `refused()`) stay Python-side; the projection every verb answers
 * through, and its error contract, is here.
 * PoP: w19_admissions_c8ef95_record @ persistence/sqlite/authority/admissions.py:_record
 * PoP: w19_admissions_c8ef95_claimed @ persistence/sqlite/authority/admissions.py:_claimed
 */
#include "noc_w19_admissions_c8ef95.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tuple(f.name for f in fields(AdmissionRecord)) - the dataclass order. */
static const char *const RECORD_FIELDS[] = {
    "seq", "admission_id", "request_id", "principal_id", "target_session_id", "payload",
    "intent", "status", "outcome", "owner_epoch", "generation", NULL
};

static const char *const INTENTS[] = {"queue", "steer", "redirect", NULL};
static const char *const STATUSES[] = {"queued", "started", "unknown", "terminal", NULL};

static int in_vocab(const char *const *vocab, const char *v) {
    int i;
    for (i = 0; vocab[i]; i++)
        if (!strcmp(vocab[i], v)) return 1;
    return 0;
}

/* {"error": "<text>"} - the exception the live projection would raise, rendered. */
static json_t *error_obj(const char *text) {
    json_t *out = json_object();
    json_set(out, "error", json_string(text ? text : ""));
    return out;
}

static json_t *key_error(const char *name) {
    char buf[256];
    snprintf(buf, sizeof(buf), "KeyError: '%s'", name);
    return error_obj(buf);
}

static json_t *value_error(const char *member, const char *value) {
    char buf[512];
    snprintf(buf, sizeof(buf), "ValueError: '%s' is not a valid %s",
             value ? value : "", member);
    return error_obj(buf);
}

/* Projection of the record fields, in dataclass order. NULL on a missing field (error already built). */
static json_t *project(const json_t *row) {
    json_t *out;
    int i;
    for (i = 0; RECORD_FIELDS[i]; i++) {
        json_t *v = json_obj_get(row, RECORD_FIELDS[i]);
        if (!v) return key_error(RECORD_FIELDS[i]);
    }
    out = json_object();
    if (!out) return NULL;
    for (i = 0; RECORD_FIELDS[i]; i++)
        json_set(out, RECORD_FIELDS[i], json_copy(json_obj_get(row, RECORD_FIELDS[i])));

    /* AdmissionRecord.__post_init__ coerces the CHECK-constrained vocabularies. */
    {
        json_t *intent = json_obj_get(out, "intent");
        json_t *status = json_obj_get(out, "status");
        const char *iv = (intent && intent->type == JSON_STRING && intent->str_val) ? intent->str_val : "";
        const char *sv = (status && status->type == JSON_STRING && status->str_val) ? status->str_val : "";
        if (!in_vocab(INTENTS, iv)) {
            json_t *err = value_error("AdmissionIntent", iv);
            json_free(out);
            return err;
        }
        if (!in_vocab(STATUSES, sv)) {
            json_t *err = value_error("AdmissionStatus", sv);
            json_free(out);
            return err;
        }
    }
    return out;
}

json_t *w19_admissions_c8ef95_record(const json_t *row) {
    if (!row || row->type == JSON_NULL) return json_null();
    if (row->type != JSON_OBJECT) return json_null();
    return project(row);
}

json_t *w19_admissions_c8ef95_claimed(const json_t *row) {
    json_t *out;
    json_t *mark;
    json_t *epoch;
    if (!row || row->type == JSON_NULL) return json_null();
    if (row->type != JSON_OBJECT) return json_null();
    out = project(row);
    if (!out) return NULL;
    if (json_obj_get(out, "error")) return out;  /* projection already refused */

    mark = json_obj_get(row, "transcript_mark");
    if (!mark) { json_free(out); return key_error("transcript_mark"); }
    epoch = json_obj_get(row, "queued_owner_epoch");
    if (!epoch) { json_free(out); return key_error("queued_owner_epoch"); }
    json_set(out, "transcript_mark", json_copy(mark));
    json_set(out, "queued_owner_epoch", json_copy(epoch));
    return out;
}
