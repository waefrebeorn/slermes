/*
 * telegram_ingress.c — C11 port of upstream Hermes
 *                      plugins/platforms/telegram/telegram_ingress.py
 *                      (gate w25_telegramin_d02e02).
 *
 * PoP: w25_telegramin_d02e02_contact_text @ plugins/platforms/telegram/telegram_ingress.py:contact_text
 * PoP: w25_telegramin_d02e02_drop_matching @ plugins/platforms/telegram/telegram_ingress.py:_drop_matching
 * PoP: w25_telegramin_d02e02_discard_media_buffers @ plugins/platforms/telegram/telegram_ingress.py:discard_session_media_buffers
 * PoP: w25_telegramin_d02e02_constants @ plugins/platforms/telegram/telegram_ingress.py:contact_text literals
 *
 * The PTB update, the adapter's buffers, the flush tasks and ``handle_message``
 * are seams: the buffers arrive as value lists, each entry carrying its session
 * key and whether its flush task had already finished.  What is ported is the
 * rendered contact block and which buffered events a reset boundary drops.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_telegramin_d02e02.h"

#define TG_HEADER "[The user shared a contact card.]"

/* Python truthiness over a JSON value; NULL json_t* is None. */
static int tg_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static json_t *tg_error(const char *name) {
    json_t *o = json_object();
    json_set(o, "__error__", json_string(name));
    return o;
}

/* An object's attribute with a None default, as getattr(x, name, None) gives. */
static const json_t *tg_attr(const json_t *obj, const char *name) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, name);
}

/* str(value) for the scalar shapes a contact carries. */
static char *tg_str(const json_t *v) {
    char buf[64];
    if (!v) return strdup("None");
    switch (v->type) {
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    default: return strdup("None");
    }
}

/* A small growable string buffer. */
typedef struct {
    char *buf;
    size_t len, cap;
    int oom;
} tg_sb;

static void tg_sb_add(tg_sb *sb, const char *s, size_t n) {
    if (sb->oom) return;
    if (sb->len + n + 1 > sb->cap) {
        size_t cap = sb->cap ? sb->cap : 64;
        char *p;
        while (cap < sb->len + n + 1) cap *= 2;
        p = (char *)realloc(sb->buf, cap);
        if (!p) { sb->oom = 1; return; }
        sb->buf = p;
        sb->cap = cap;
    }
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}

static void tg_sb_puts(tg_sb *sb, const char *s) { tg_sb_add(sb, s ? s : "", strlen(s ? s : "")); }

/* The join() of two optional name parts: only reached when both are truthy. */
static char *tg_join_names(const json_t *first, const json_t *last, const char **err) {
    tg_sb sb = {NULL, 0, 0, 0};
    int have = 0;
    if (tg_truthy(first)) {
        if (first->type != JSON_STRING) { *err = "TypeError"; return NULL; }
        tg_sb_puts(&sb, first->str_val);
        have = 1;
    }
    if (tg_truthy(last)) {
        if (last->type != JSON_STRING) { free(sb.buf); *err = "TypeError"; return NULL; }
        if (have) tg_sb_puts(&sb, " ");
        tg_sb_puts(&sb, last->str_val);
        have = 1;
    }
    if (sb.oom) { free(sb.buf); *err = "MemoryError"; return NULL; }
    if (!sb.buf) return strdup("");
    return sb.buf;
}

json_t *w25_telegramin_d02e02_contact_text(const json_t *contact) {
    tg_sb sb = {NULL, 0, 0, 0};
    const char *err = NULL;
    const json_t *first = tg_attr(contact, "first_name");
    const json_t *last = tg_attr(contact, "last_name");
    const json_t *phone = tg_attr(contact, "phone_number");
    const json_t *user_id = tg_attr(contact, "user_id");
    const json_t *vcard = tg_attr(contact, "vcard");
    char *name = tg_join_names(first, last, &err);

    if (!name) return tg_error(err ? err : "MemoryError");
    tg_sb_puts(&sb, TG_HEADER);
    if (name[0] != '\0') {
        tg_sb_puts(&sb, "\nName: ");
        tg_sb_puts(&sb, name);
    }
    free(name);
    if (tg_truthy(phone)) {
        char *text = tg_str(phone);
        tg_sb_puts(&sb, "\nPhone: ");
        tg_sb_puts(&sb, text);
        free(text);
    }
    if (tg_truthy(user_id)) {
        char *text = tg_str(user_id);
        tg_sb_puts(&sb, "\nTelegram user id: ");
        tg_sb_puts(&sb, text);
        free(text);
    }
    if (tg_truthy(vcard)) {
        if (vcard->type != JSON_STRING) { free(sb.buf); return tg_error("TypeError"); }
        tg_sb_puts(&sb, "\nvCard:\n");
        tg_sb_puts(&sb, vcard->str_val);
    }
    if (sb.oom) { free(sb.buf); return tg_error("MemoryError"); }
    {
        json_t *out = json_string(sb.buf ? sb.buf : "");
        free(sb.buf);
        return out;
    }
}

/* Python '==' across the shapes a session key can hold. */
static int tg_equal(const json_t *a, const json_t *b) {
    double x, y;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

static int tg_drop_count(const json_t *pending, const json_t *session_key) {
    size_t i, n;
    int dropped = 0;
    if (!pending || pending->type != JSON_ARRAY) return 0;
    n = json_len(pending);
    for (i = 0; i < n; i++) {
        const json_t *entry = json_get(pending, i);
        if (!entry || entry->type != JSON_OBJECT) continue;
        if (!tg_equal(json_obj_get(entry, "session"), session_key)) continue;
        dropped++;
    }
    return dropped;
}

json_t *w25_telegramin_d02e02_drop_matching(const json_t *pending,
                                            const json_t *session_key) {
    return json_number((double)tg_drop_count(pending, session_key));
}

json_t *w25_telegramin_d02e02_discard_media_buffers(const json_t *session_key,
                                                    const json_t *photo_batches,
                                                    const json_t *media_group_events) {
    int dropped = tg_drop_count(photo_batches, session_key)
                + tg_drop_count(media_group_events, session_key);
    return json_number((double)dropped);
}

json_t *w25_telegramin_d02e02_constants(void) {
    json_t *o = json_object();
    json_t *labels = json_array();
    json_append(labels, json_string("Name"));
    json_append(labels, json_string("Phone"));
    json_append(labels, json_string("Telegram user id"));
    json_set(o, "header", json_string(TG_HEADER));
    json_set(o, "labels", labels);
    json_set(o, "vcard_label", json_string("vCard:"));
    json_set(o, "separator", json_string("\n"));
    return o;
}
