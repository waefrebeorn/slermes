/*
 * hosted_room_authority_history.c -- real C11 port of the pure half of upstream
 * Hermes gateway/hosted_room_authority_history.py.
 *
 * PoP: w21_hostedroom_3970cb_validate_history @ gateway/hosted_room_authority_history.py:validate_history
 * PoP: w21_hostedroom_3970cb_at_sequence @ gateway/hosted_room_authority_history.py:at_sequence
 * PoP: w21_hostedroom_3970cb_origin_gateway_id @ gateway/hosted_room_authority_history.py:origin_gateway_id
 * PoP: w21_hostedroom_3970cb_identifier @ gateway/hosted_rooms_common.py:identifier
 *
 * `read_history_locked` (SQL inside the caller's room-state transaction), the
 * AuthoritySpan dataclass and the AuthorityHistoryError class stay upstream;
 * the error a span violation raises is reproduced here as the exact
 * {"status":"error","type":...,"message":...} record.
 */
#include "noc_w21_hostedroom_3970cb.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HR_LABEL_GATEWAY "authority gateway"

static json_t *hr_ok(json_t *value)
{
    json_t *out = json_object();
    if (!out) { json_free(value); return NULL; }
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value ? value : json_null());
    return out;
}

static json_t *hr_err(const char *type, const char *message)
{
    json_t *out = json_object();
    if (!out) return NULL;
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string(type));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

static json_t *hr_authority_error(const char *message)
{
    return hr_err("AuthorityHistoryError", message);
}

/* `type(v) is int`: an integral JSON number (a JSON bool is not an int). */
static int hr_node_int_strict(const json_t *v, long long *out)
{
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9007199254740992.0 || d > 9007199254740992.0) return 0;
    *out = (long long)d;
    return 1;
}

/* An int64 API argument: an integral number, or its decimal string form. */
static int hr_node_int(const json_t *v, long long *out)
{
    if (v && v->type == JSON_STRING && v->str_val && *v->str_val) {
        char *end = NULL;
        long long parsed = strtoll(v->str_val, &end, 10);
        if (end && *end == '\0') { *out = parsed; return 1; }
        return 0;
    }
    return hr_node_int_strict(v, out);
}

/* Python str.strip(): leading/trailing ASCII whitespace. */
static char *hr_strip(const char *s)
{
    const char *start = s ? s : "";
    const char *end;
    size_t n;
    char *out;

    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' ||
           *start == '\f' || *start == '\v')
        start++;
    end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                           end[-1] == '\r' || end[-1] == '\f' || end[-1] == '\v'))
        end--;
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

static int hr_ident_char(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '.' || c == '_' || c == ':' || c == '-';
}

/* IDENTIFIER_RE.fullmatch: ^[A-Za-z0-9][A-Za-z0-9._:-]*$ */
static int hr_shape_ok(const char *s)
{
    size_t i;
    if (!s || !*s) return 0;
    if (!((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z') ||
          (s[0] >= '0' && s[0] <= '9')))
        return 0;
    for (i = 1; s[i]; i++)
        if (!hr_ident_char((unsigned char)s[i])) return 0;
    return 1;
}

static long long hr_strlen_chars(const char *s)
{
    /* UTF-8 code points, as Python len() counts them. */
    long long n = 0;
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) {
        if ((*p & 0xC0) != 0x80) n++;
        p++;
    }
    return n;
}

json_t *w21_hostedroom_3970cb_identifier(const json_t *value, const char *label,
                                         long long max_chars, int check_shape)
{
    char *stripped;
    char message[160];

    if (!value || value->type != JSON_STRING) {
        snprintf(message, sizeof(message), "%s must be a string", label ? label : "");
        return hr_authority_error(message);
    }
    stripped = hr_strip(value->str_val);
    if (!stripped) return hr_authority_error("out of memory");
    if (!*stripped || hr_strlen_chars(stripped) > max_chars ||
        (check_shape && !hr_shape_ok(stripped))) {
        snprintf(message, sizeof(message), "invalid %s", label ? label : "");
        free(stripped);
        return hr_authority_error(message);
    }
    {
        json_t *out = hr_ok(json_string(stripped));
        free(stripped);
        return out;
    }
}

/* _gateway(): identifier() plus the "must be canonical" check. */
static const char *hr_gateway(const json_t *value, const char **error)
{
    static char scratch[256];
    char *stripped;

    *error = NULL;
    if (!value || value->type != JSON_STRING) {
        *error = HR_LABEL_GATEWAY " must be a string";
        return NULL;
    }
    stripped = hr_strip(value->str_val);
    if (!stripped) { *error = "out of memory"; return NULL; }
    if (!*stripped || hr_strlen_chars(stripped) > 128 || !hr_shape_ok(stripped)) {
        *error = "invalid " HR_LABEL_GATEWAY;
        free(stripped);
        return NULL;
    }
    if (strcmp(stripped, value->str_val) != 0) {
        *error = HR_LABEL_GATEWAY " must be canonical";
        free(stripped);
        return NULL;
    }
    snprintf(scratch, sizeof(scratch), "%s", stripped);
    free(stripped);
    return scratch;
}

/* The three span fields, in the order the dataclass declares them. */
static json_t *hr_span(const char *gateway_id, long long epoch, long long from_seq)
{
    json_t *span = json_object();
    if (!span) return NULL;
    json_set(span, "gateway_id", json_string(gateway_id ? gateway_id : ""));
    json_set(span, "epoch", json_number((double)epoch));
    json_set(span, "from_seq", json_number((double)from_seq));
    return span;
}

static int hr_exact_fields(const json_t *item)
{
    if (!item || item->type != JSON_OBJECT) return 0;
    if (json_object_size(item) != 3) return 0;
    return json_obj_get(item, "gateway_id") != NULL && json_obj_get(item, "epoch") != NULL &&
           json_obj_get(item, "from_seq") != NULL;
}

json_t *w21_hostedroom_3970cb_validate_history(const json_t *value, const json_t *gateway_id,
                                               const json_t *epoch)
{
    json_t *spans;
    long long prev_epoch = 0, prev_start = 0;
    size_t i, n;
    long long current_epoch = 0;

    if (!value || value->type == JSON_NULL) {
        json_t *span = json_object();
        if (!span) return hr_authority_error("out of memory");
        json_set(span, "gateway_id", gateway_id ? json_copy(gateway_id) : json_null());
        json_set(span, "epoch", epoch ? json_copy(epoch) : json_null());
        json_set(span, "from_seq", json_number(0));
        spans = json_array();
        if (!spans) { json_free(span); return hr_authority_error("out of memory"); }
        json_append(spans, span);
        return hr_ok(spans);
    }
    if (value->type != JSON_ARRAY || value->c.count < 1 ||
        value->c.count > W21_HOSTEDROOM_3970CB_MAX_SPANS)
        return hr_authority_error("authority history exceeds its bound or is absent");

    spans = json_array();
    if (!spans) return hr_authority_error("out of memory");
    n = value->c.count;
    for (i = 0; i < n; i++) {
        const json_t *item = value->c.items[i];
        const json_t *gw_node;
        const char *error = NULL;
        const char *gateway;
        long long generation, start;

        if (!hr_exact_fields(item)) {
            json_free(spans);
            return hr_authority_error("authority history fields are invalid");
        }
        gw_node = json_obj_get(item, "gateway_id");
        gateway = hr_gateway(gw_node, &error);
        if (!gateway) {
            json_free(spans);
            return hr_authority_error(error);
        }
        if (!hr_node_int_strict(json_obj_get(item, "epoch"), &generation) ||
            !hr_node_int_strict(json_obj_get(item, "from_seq"), &start) ||
            generation < 1 || generation >= 9223372036854775807LL || start < 0) {
            json_free(spans);
            return hr_authority_error("authority history coordinates are invalid");
        }
        if (i == 0) {
            if (generation != 1 || start != 0) {
                json_free(spans);
                return hr_authority_error("authority history has no verified starting point");
            }
        } else if (generation != prev_epoch + 1 || start <= prev_start) {
            json_free(spans);
            return hr_authority_error("authority history has a gap or unordered boundary");
        }
        json_append(spans, hr_span(gateway, generation, start));
        prev_epoch = generation;
        prev_start = start;
    }

    /* (spans[-1].gateway_id, spans[-1].epoch) != (gateway_id, epoch) */
    {
        const json_t *last = spans->c.items[spans->c.count - 1];
        const char *last_gateway = json_get_str(last, "gateway_id", "");
        int epoch_ok = hr_node_int(epoch, &current_epoch) &&
                       (double)current_epoch == json_get_num(last, "epoch", -1.0);
        if (!gateway_id || gateway_id->type != JSON_STRING ||
            strcmp(last_gateway, gateway_id->str_val ? gateway_id->str_val : "") != 0 ||
            !epoch_ok) {
            json_free(spans);
            return hr_authority_error("authority history does not reach the current owner");
        }
    }
    return hr_ok(spans);
}

json_t *w21_hostedroom_3970cb_at_sequence(const json_t *history, const json_t *seq)
{
    long long wanted;
    size_t i;

    if (!hr_node_int(seq, &wanted))
        return hr_authority_error("event precedes the known authority history");
    if (!history || history->type != JSON_ARRAY)
        return hr_authority_error("event precedes the known authority history");
    for (i = history->c.count; i > 0; i--) {
        const json_t *span = history->c.items[i - 1];
        long long from_seq;
        if (!span || span->type != JSON_OBJECT) continue;
        if (!hr_node_int_strict(json_obj_get(span, "from_seq"), &from_seq)) continue;
        if (wanted >= from_seq) return hr_ok(json_copy(span));
    }
    return hr_authority_error("event precedes the known authority history");
}

json_t *w21_hostedroom_3970cb_origin_gateway_id(const json_t *room)
{
    const json_t *gateway_id;
    const json_t *epoch;
    json_t *validated, *first, *value;

    if (!room || room->type != JSON_OBJECT)
        return hr_err("AttributeError", "'NoneType' object has no attribute 'get'");
    gateway_id = json_obj_get(room, "authority_gateway_id");
    if (!gateway_id) return hr_err("KeyError", "'authority_gateway_id'");
    epoch = json_obj_get(room, "authority_epoch");
    if (!epoch) return hr_err("KeyError", "'authority_epoch'");

    validated = w21_hostedroom_3970cb_validate_history(
        json_obj_get(room, "authority_history"), gateway_id, epoch);
    if (!validated) return hr_authority_error("out of memory");
    if (strcmp(json_get_str(validated, "status", ""), "ok") != 0) return validated;
    value = json_obj_get(validated, "value");
    first = (value && value->type == JSON_ARRAY && value->c.count > 0)
                ? value->c.items[0]
                : NULL;
    if (!first) { json_free(validated); return hr_authority_error("authority history exceeds its bound or is absent"); }
    {
        json_t *out = hr_ok(json_copy(json_obj_get(first, "gateway_id")));
        json_free(validated);
        return out;
    }
}

