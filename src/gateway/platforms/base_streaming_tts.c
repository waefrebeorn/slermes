/*
 * base_streaming_tts.c -- real C11 port of the streaming-TTS format descriptor,
 * handle and turn-scoped suppression helpers of upstream Hermes
 *   gateway/platforms/base_streaming_tts.py.
 *
 * PoP: w21_basestream_dce4b8_audio_format @ gateway/platforms/base_streaming_tts.py:AudioFormat
 * PoP: w21_basestream_dce4b8_handle @ gateway/platforms/base_streaming_tts.py:StreamingTTSHandle
 * PoP: w21_basestream_dce4b8_turn_key @ gateway/platforms/base_streaming_tts.py:streaming_tts_turn_key
 * PoP: w21_basestream_dce4b8_should_skip @ gateway/platforms/base_streaming_tts.py:streaming_tts_should_skip_whole_file
 *
 * The module is pure stdlib: two dataclasses (AudioFormat, StreamingTTSHandle)
 * and two pure helpers. Nothing here reaches the network, the filesystem or a
 * clock, so the whole module is ported rather than a subset.
 */
#include "hermes_json.h"
#include "noc_w21_basestream_dce4b8.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *bst_dce4b8_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int bst_dce4b8_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static int bst_dce4b8_is_py_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* Python str(scalar). Containers keep their JSON text (see header). */
static char *bst_dce4b8_str(const json_t *v) {
    char buf[64];
    char *s;
    if (!v) return bst_dce4b8_dup("None");
    switch (v->type) {
    case JSON_NULL: return bst_dce4b8_dup("None");
    case JSON_BOOL: return bst_dce4b8_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return bst_dce4b8_dup(v->str_val);
    case JSON_NUMBER:
        if (bst_dce4b8_is_py_int(v)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return bst_dce4b8_dup(buf);
    case JSON_ARRAY:
    case JSON_OBJECT:
        s = json_dumps(v, 0);
        return s ? s : bst_dce4b8_dup("");
    }
    return bst_dce4b8_dup("");
}

/* `<Class>.__init__() got an unexpected keyword argument '<key>'` */
static json_t *bst_dce4b8_unexpected(const char *cls, const char *key) {
    char msg[512];
    json_t *out = json_object();
    snprintf(msg, sizeof(msg), "%s.__init__() got an unexpected keyword argument '%s'",
             cls, key ? key : "");
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string("TypeError"));
    json_set(out, "message", json_string(msg));
    return out;
}

static json_t *bst_dce4b8_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value);
    return out;
}

/* --- AudioFormat ------------------------------------------------------------ */

static const char *const BSTMOD_AUDIO_FIELDS[] = {"sample_rate", "channels", "sample_width", NULL};

static int bst_dce4b8_audio_key_known(const char *key) {
    int j;
    for (j = 0; BSTMOD_AUDIO_FIELDS[j]; j++) {
        if (key && !strcmp(key, BSTMOD_AUDIO_FIELDS[j])) return 1;
    }
    return 0;
}

static json_t *bst_dce4b8_audio_value(const json_t *overrides) {
    static const double DEFAULTS[] = {24000.0, 1.0, 2.0};
    json_t *out = json_object();
    size_t i;
    for (i = 0; i < 3; i++) {
        json_t *v = json_obj_get(overrides, BSTMOD_AUDIO_FIELDS[i]);
        json_set(out, BSTMOD_AUDIO_FIELDS[i], v ? json_copy(v) : json_number(DEFAULTS[i]));
    }
    return out;
}

json_t *w21_basestream_dce4b8_audio_format(const json_t *overrides) {
    size_t i;
    if (overrides && overrides->type == JSON_OBJECT) {
        for (i = 0; i < overrides->c.count; i++) {
            const char *key = overrides->c.keys[i];
            if (!bst_dce4b8_audio_key_known(key)) return bst_dce4b8_unexpected("AudioFormat", key);
        }
    } else {
        overrides = NULL;
    }
    return bst_dce4b8_ok(bst_dce4b8_audio_value(overrides));
}

/* --- StreamingTTSHandle ----------------------------------------------------- */

json_t *w21_basestream_dce4b8_handle(const json_t *overrides) {
    static const char *const FIELDS[] = {"chat_id", "audio_format", "audible", "aborted", NULL};
    json_t *out;
    const json_t *fmt_in = NULL;
    json_t *cid;
    size_t i;
    if (overrides && overrides->type == JSON_OBJECT) {
        for (i = 0; i < overrides->c.count; i++) {
            const char *key = overrides->c.keys[i];
            int known = 0, j;
            for (j = 0; FIELDS[j]; j++) {
                if (key && !strcmp(key, FIELDS[j])) { known = 1; break; }
            }
            if (!known) return bst_dce4b8_unexpected("StreamingTTSHandle", key);
        }
        fmt_in = json_obj_get(overrides, "audio_format");
    }
    /* The fixture's nested object is the AudioFormat constructor's kwargs; any
     * other stored value rides through as `asdict` would copy it. */
    if (fmt_in && fmt_in->type == JSON_OBJECT) {
        for (i = 0; i < fmt_in->c.count; i++) {
            const char *key = fmt_in->c.keys[i];
            if (!bst_dce4b8_audio_key_known(key)) return bst_dce4b8_unexpected("AudioFormat", key);
        }
    }
    out = json_object();
    cid = json_obj_get(overrides, "chat_id");
    json_set(out, "chat_id", cid ? json_copy(cid) : json_string(""));
    if (fmt_in && fmt_in->type == JSON_OBJECT) json_set(out, "audio_format", bst_dce4b8_audio_value(fmt_in));
    else if (fmt_in) json_set(out, "audio_format", json_copy(fmt_in));
    else json_set(out, "audio_format", bst_dce4b8_audio_value(NULL));
    {
        json_t *audible = json_obj_get(overrides, "audible");
        json_t *aborted = json_obj_get(overrides, "aborted");
        json_set(out, "audible", audible ? json_copy(audible) : json_bool(0));
        json_set(out, "aborted", aborted ? json_copy(aborted) : json_bool(0));
    }
    return out;
}

/* --- turn key / suppression ------------------------------------------------ */

static const json_t *bst_dce4b8_event_marker(const json_t *event) {
    const json_t *mid, *pid;
    if (!event || event->type != JSON_OBJECT) return NULL;
    mid = json_obj_get(event, "message_id");
    if (bst_dce4b8_truthy(mid)) return mid;
    pid = json_obj_get(event, "platform_update_id");
    if (bst_dce4b8_truthy(pid)) return pid;
    return NULL;
}

char *w21_basestream_dce4b8_turn_key(const json_t *session_key, const json_t *turn_marker,
                                    const json_t *event) {
    const json_t *marker = turn_marker;
    char *skey, *mkey, *out;
    size_t n;
    if (!bst_dce4b8_truthy(session_key)) return NULL;
    if (!marker && event) marker = bst_dce4b8_event_marker(event);
    if (!marker || marker->type == JSON_NULL) return NULL;
    skey = bst_dce4b8_str(session_key);
    mkey = bst_dce4b8_str(marker);
    if (!skey || !mkey) { free(skey); free(mkey); return NULL; }
    n = strlen(skey) + strlen(mkey) + 2;
    out = (char *)malloc(n);
    if (out) snprintf(out, n, "%s:%s", skey, mkey);
    free(skey);
    free(mkey);
    return out;
}

int w21_basestream_dce4b8_should_skip(const json_t *completed_turns, const json_t *session_key,
                                     const json_t *turn_marker, const json_t *event) {
    char *key = w21_basestream_dce4b8_turn_key(session_key, turn_marker, event);
    size_t i;
    int found = 0;
    if (!key || !key[0]) { free(key); return 0; }
    if (completed_turns && completed_turns->type == JSON_ARRAY) {
        for (i = 0; i < completed_turns->c.count; i++) {
            json_t *v = completed_turns->c.items[i];
            if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, key)) {
                found = 1;
                break;
            }
        }
    }
    free(key);
    return found;
}
