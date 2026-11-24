/*
 * adapter.c - C11 port of the PURE decision core of upstream
 * plugins/platforms/simplex/adapter.py (gates w31_adapter_c93276_p1 and
 * w31_adapter_c93276_p2).
 *
 * PoP: w31_adapter_c93276_p1_parse_comma_list   @ plugins/platforms/simplex/adapter.py:_parse_comma_list
 * PoP: w31_adapter_c93276_p1_redact_id          @ plugins/platforms/simplex/adapter.py:_redact_id
 * PoP: w31_adapter_c93276_p1_path_suffix        @ plugins/platforms/simplex/adapter.py:_handle_chat_item (Path(...).suffix)
 * PoP: w31_adapter_c93276_p1_is_image_ext       @ plugins/platforms/simplex/adapter.py:_is_image_ext
 * PoP: w31_adapter_c93276_p1_is_audio_ext       @ plugins/platforms/simplex/adapter.py:_is_audio_ext
 * PoP: w31_adapter_c93276_p1_mime_for_ext       @ plugins/platforms/simplex/adapter.py:_mime_for_ext
 * PoP: w31_adapter_c93276_p1_display_name       @ plugins/platforms/simplex/adapter.py:_display_name
 * PoP: w31_adapter_c93276_p1_may_receive_file   @ plugins/platforms/simplex/adapter.py:SimplexAdapter._may_receive_file
 * PoP: w31_adapter_c93276_p1_ts_iso             @ plugins/platforms/simplex/adapter.py:SimplexAdapter._handle_chat_item (datetime.fromisoformat)
 * PoP: w31_adapter_c93276_p1_chat_item          @ plugins/platforms/simplex/adapter.py:SimplexAdapter._handle_chat_item
 * PoP: w31_adapter_c93276_p2_send_cmd           @ plugins/platforms/simplex/adapter.py:_send_cmd
 * PoP: w31_adapter_c93276_p2_split_media        @ plugins/platforms/simplex/adapter.py:SimplexAdapter.send (MEDIA: tags)
 * PoP: w31_adapter_c93276_p2_is_voice_ext       @ plugins/platforms/simplex/adapter.py:_VOICE_TAG_EXTS
 * PoP: w31_adapter_c93276_p2_send_item          @ plugins/platforms/simplex/adapter.py:send_image/send_document/send_voice
 * PoP: w31_adapter_c93276_p2_corr_id            @ plugins/platforms/simplex/adapter.py:SimplexAdapter._make_corr_id + _standalone_send
 * PoP: w31_adapter_c93276_p2_retry_math         @ plugins/platforms/simplex/adapter.py:SimplexAdapter._ws_listener (backoff)
 * PoP: w31_adapter_c93276_p2_health_stale       @ plugins/platforms/simplex/adapter.py:SimplexAdapter._health_monitor
 * PoP: w31_adapter_c93276_p2_init_config        @ plugins/platforms/simplex/adapter.py:SimplexAdapter.__init__
 * PoP: w31_adapter_c93276_p2_config_gate        @ plugins/platforms/simplex/adapter.py:check_requirements/validate_config/is_connected
 * PoP: w31_adapter_c93276_p2_env_enablement     @ plugins/platforms/simplex/adapter.py:_env_enablement
 * PoP: w31_adapter_c93276_p2_get_chat_info      @ plugins/platforms/simplex/adapter.py:SimplexAdapter.get_chat_info
 * PoP: w31_adapter_c93276_p2_list_channels      @ plugins/platforms/simplex/adapter.py:SimplexAdapter.list_channels
 * PoP: w31_adapter_c93276_p2_prepare_image      @ plugins/platforms/simplex/adapter.py:SimplexAdapter._prepare_image (needs_png)
 * PoP: w31_adapter_c93276_p2_text_batch_key     @ plugins/platforms/simplex/adapter.py:SimplexAdapter._text_batch_key
 *
 * SEAMS (deliberately not ported; each is a fixture input to the gates above):
 *   - the simplex-chat daemon WebSocket: connect/send/recv, corrId replies,
 *     the reconnect loop's sleeps and the asyncio task plumbing;
 *   - websockets import availability (fed as a boolean);
 *   - per-profile env/secret reads (_get_scoped_secret / platform_gate_env /
 *     _seed_extra_from_env): the gates take the resulting values;
 *   - authz (_is_sender_authorized): the gates take the authorized id set;
 *   - build_source / MessageEvent construction: the record carries the
 *     arguments build_source would receive;
 *   - filesystem existence probes and Pillow/ImageMagick image conversion: the
 *     gates take the path/suffix and the produced thumbnail URI;
 *   - json.dumps of the /_send payload (libjson is a compact serializer, so the
 *     gate compares the addressed target and the item array structurally);
 *   - logging, counters and wall-clock time (fed as now_ms).
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w31_adapter_c93276_p1.h"
#include "noc_w31_adapter_c93276_p2.h"

/* ------------------------------------------------------------------ utils */

/* Python truthiness of a JSON value. */
static int ax_truthy(const json_t *v) {
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

static const char *ax_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static const char *ax_str_or(const json_t *v, const char *def) {
    const char *s = ax_str(v);
    return s ? s : def;
}

static const json_t *ax_obj(const json_t *v) {
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static char *ax_dup_n(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n && s) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* ASCII whitespace, the alphabet str.strip()/\\S work over in the fixtures. */
static int ax_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *ax_strip(const char *s) {
    size_t n;
    if (!s) return ax_dup_n("", 0);
    while (*s && ax_space((unsigned char)*s)) s++;
    n = strlen(s);
    while (n && ax_space((unsigned char)s[n - 1])) n--;
    return ax_dup_n(s, n);
}

static char *ax_lower(const char *s) {
    char *out;
    size_t i, n;
    if (!s) return ax_dup_n("", 0);
    n = strlen(s);
    out = ax_dup_n(s, n);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)out[i];
        if (c >= 'A' && c <= 'Z') out[i] = (char)(c - 'A' + 'a');
    }
    return out;
}

/* Python str() over the scalars a SimpleX id can be. */
static char *ax_value_str(const json_t *v) {
    char buf[64];
    if (!v) return ax_dup_n("", 0);
    if (v->type == JSON_STRING) return ax_dup_n(v->str_val ? v->str_val : "",
                                                v->str_val ? strlen(v->str_val) : 0);
    if (v->type == JSON_NULL) return ax_dup_n("", 0);
    if (v->type == JSON_BOOL) return ax_dup_n(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            snprintf(buf, sizeof buf, "%.17g", d);
        return ax_dup_n(buf, strlen(buf));
    }
    return ax_dup_n("", 0);
}

/* UTF-8 code-point count / prefix byte length / suffix pointer, so the
 * character indexing _redact_id does stays character indexing. */
static size_t ax_cp_count(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if ((*p & 0xC0) != 0x80) n++;
        p++;
    }
    return n;
}

static size_t ax_cp_prefix_bytes(const char *s, size_t cps) {
    const unsigned char *p = (const unsigned char *)s;
    size_t i = 0, seen = 0;
    while (p[i] && seen < cps) {
        i++;
        while (p[i] && (p[i] & 0xC0) == 0x80) i++;
        seen++;
    }
    return i;
}

static const char *ax_cp_suffix_ptr(const char *s, size_t cps) {
    const unsigned char *p = (const unsigned char *)s;
    size_t len = strlen(s), idx = len, seen = 0;
    while (idx > 0) {
        idx--;
        if ((p[idx] & 0xC0) != 0x80) {
            seen++;
            if (seen == cps) return s + idx;
        }
    }
    return s;
}

/* _IMAGE_EXTS / _AUDIO_EXTS / _VOICE_TAG_EXTS / _TEXT_BEARING_TYPES */
static const char *const AX_IMAGE_EXTS[] = {".jpg", ".jpeg", ".png", ".gif", ".webp"};
static const char *const AX_AUDIO_EXTS[] = {".mp3", ".wav", ".ogg", ".m4a", ".aac", ".opus"};
static const char *const AX_VOICE_EXTS[] = {".ogg", ".mp3", ".wav", ".m4a", ".opus"};
static const char *const AX_TEXT_TYPES[] = {"text", "file", "image", "voice", "link", "video"};

#define AX_COUNT(a) (sizeof(a) / sizeof((a)[0]))

static int ax_in_list_ci(const char *ext, const char *const *list, size_t n) {
    char *low;
    size_t i;
    int hit = 0;
    if (!ext) return 0;
    low = ax_lower(ext);
    if (!low) return 0;
    for (i = 0; i < n; i++)
        if (!strcmp(low, list[i])) { hit = 1; break; }
    free(low);
    return hit;
}

static int ax_in_list(const char *v, const char *const *list, size_t n) {
    size_t i;
    if (!v) return 0;
    for (i = 0; i < n; i++)
        if (!strcmp(v, list[i])) return 1;
    return 0;
}

/* ------------------------------------------------------------- gate p1 --- */

json_t *w31_adapter_c93276_p1_parse_comma_list(const char *value) {
    json_t *out = json_array();
    const char *p = value ? value : "";
    if (!out) return NULL;
    for (;;) {
        const char *comma = strchr(p, ',');
        size_t n = comma ? (size_t)(comma - p) : strlen(p);
        char *field = ax_dup_n(p, n);
        char *trim = ax_strip(field);
        if (trim && trim[0]) json_append(out, json_string(trim));
        free(trim);
        free(field);
        if (!comma) break;
        p = comma + 1;
    }
    return out;
}

json_t *w31_adapter_c93276_p1_redact_id(const json_t *value) {
    char *s, *out;
    size_t cps, head, tail_len;
    const char *tail;
    json_t *node;

    if (!ax_truthy(value)) return json_string("<none>");
    s = ax_value_str(value);
    if (!s || !s[0]) { free(s); return json_string("<none>"); }
    cps = ax_cp_count(s);
    if (cps <= 4) { node = json_string(s); free(s); return node; }
    head = ax_cp_prefix_bytes(s, 2);
    tail = ax_cp_suffix_ptr(s, 2);
    tail_len = strlen(tail);
    out = (char *)malloc(head + 2 + tail_len + 1);
    if (!out) { free(s); return json_string("<none>"); }
    memcpy(out, s, head);
    out[head] = '*';
    out[head + 1] = '*';
    memcpy(out + head + 2, tail, tail_len);
    out[head + 2 + tail_len] = '\0';
    node = json_string(out);
    free(out);
    free(s);
    return node;
}

json_t *w31_adapter_c93276_p1_path_suffix(const char *path) {
    const char *base, *dot, *q;
    char *low;
    json_t *out;

    if (!path) return json_string("");
    base = path;
    for (q = path; *q; q++)
        if (*q == '/' || *q == '\\') base = q + 1;
    dot = strrchr(base, '.');
    if (!dot || dot == base) return json_string("");
    low = ax_lower(dot);
    out = json_string(low ? low : "");
    free(low);
    return out;
}

json_t *w31_adapter_c93276_p1_is_image_ext(const char *ext) {
    return json_bool(ax_in_list_ci(ext, AX_IMAGE_EXTS, AX_COUNT(AX_IMAGE_EXTS)) != 0);
}

json_t *w31_adapter_c93276_p1_is_audio_ext(const char *ext) {
    return json_bool(ax_in_list_ci(ext, AX_AUDIO_EXTS, AX_COUNT(AX_AUDIO_EXTS)) != 0);
}

json_t *w31_adapter_c93276_p1_mime_for_ext(const char *ext) {
    const char *bare;
    char buf[128];
    if (ax_in_list_ci(ext, AX_IMAGE_EXTS, AX_COUNT(AX_IMAGE_EXTS))) {
        bare = ext;
        while (*bare == '.') bare++;
        snprintf(buf, sizeof buf, "image/%s", bare);
        return json_string(buf);
    }
    if (ax_in_list_ci(ext, AX_AUDIO_EXTS, AX_COUNT(AX_AUDIO_EXTS))) {
        bare = ext;
        while (*bare == '.') bare++;
        snprintf(buf, sizeof buf, "audio/%s", bare);
        return json_string(buf);
    }
    return json_string("application/octet-stream");
}

json_t *w31_adapter_c93276_p1_display_name(const json_t *obj, const char *profile_key,
                                           const json_t *def) {
    const json_t *local, *profile, *dn;
    if (obj && obj->type == JSON_OBJECT) {
        local = json_obj_get(obj, "localDisplayName");
        if (ax_truthy(local)) return json_copy(local);
        profile = json_obj_get(obj, profile_key ? profile_key : "");
        if (ax_truthy(profile) && profile->type == JSON_OBJECT) {
            dn = json_obj_get(profile, "displayName");
            if (dn) return json_copy(dn);
        }
    }
    if (def) return json_copy(def);
    return json_string("");
}

/* authz seam: the fixture's authorized id set. */
static int ax_authorized(const json_t *authorized, const char *sid) {
    size_t i, n;
    if (!authorized || authorized->type != JSON_ARRAY || !sid) return 0;
    n = json_len(authorized);
    for (i = 0; i < n; i++) {
        const char *a = ax_str(json_get(authorized, i));
        if (a && !strcmp(a, sid)) return 1;
    }
    return 0;
}

static int ax_group_allowed(const json_t *allow, const char *gid) {
    size_t i, n;
    if (!allow || allow->type != JSON_ARRAY) return 0;
    n = json_len(allow);
    for (i = 0; i < n; i++) {
        const char *a = ax_str(json_get(allow, i));
        if (!a) continue;
        if (!strcmp(a, "*")) return 1;
        if (gid && !strcmp(a, gid)) return 1;
    }
    return 0;
}

json_t *w31_adapter_c93276_p1_may_receive_file(const json_t *policy) {
    const json_t *item, *chat_info, *allow, *authorized;
    const char *type;

    if (!ax_obj(policy)) return json_bool(0);
    item = ax_obj(json_obj_get(policy, "item"));
    if (!item) return json_bool(0);
    chat_info = ax_obj(json_obj_get(item, "chatInfo"));
    type = ax_str_or(chat_info ? json_obj_get(chat_info, "type") : NULL, "");
    allow = json_obj_get(policy, "group_allow_from");
    authorized = json_obj_get(policy, "authorized_ids");

    if (!strcmp(type, "direct")) {
        const json_t *contact = chat_info ? ax_obj(json_obj_get(chat_info, "contact")) : NULL;
        char *sid = ax_value_str(contact ? json_obj_get(contact, "contactId") : NULL);
        int ok = ax_authorized(authorized, sid);
        free(sid);
        return json_bool(ok);
    }
    if (!strcmp(type, "group")) {
        const json_t *group_info = chat_info ? ax_obj(json_obj_get(chat_info, "groupInfo")) : NULL;
        const json_t *chat_item_data, *chat_dir, *member;
        char *gid = ax_value_str(group_info ? json_obj_get(group_info, "groupId") : NULL);
        char *sid;
        int ok;
        if (!ax_group_allowed(allow, gid)) { free(gid); return json_bool(0); }
        free(gid);
        chat_item_data = ax_obj(json_obj_get(item, "chatItem"));
        chat_dir = chat_item_data ? ax_obj(json_obj_get(chat_item_data, "chatDir")) : NULL;
        member = chat_dir ? ax_obj(json_obj_get(chat_dir, "groupMember")) : NULL;
        sid = ax_value_str(member ? json_obj_get(member, "memberId") : NULL);
        ok = ax_authorized(authorized, sid);
        free(sid);
        return json_bool(ok);
    }
    return json_bool(0);
}

/* ---- datetime.fromisoformat(ts).isoformat() ---------------------------- */

static int ax_days_in_month(int y, int m) {
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) return 0;
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return days[m - 1];
}

json_t *w31_adapter_c93276_p1_ts_iso(const char *ts) {
    const char *p;
    int Y = 0, M = 0, D = 0, h = 0, mi = 0, sec = 0, us = 0, has_tz = 0, off_min = 0;
    char buf[64];
    int i;

    if (!ts) return json_null();
    p = ts;
    for (i = 0; i < 4; i++) { if (!isdigit((unsigned char)p[i])) return json_null(); Y = Y * 10 + (p[i] - '0'); }
    if (p[4] != '-') return json_null();
    for (i = 0; i < 2; i++) { if (!isdigit((unsigned char)p[5 + i])) return json_null(); M = M * 10 + (p[5 + i] - '0'); }
    if (p[7] != '-') return json_null();
    for (i = 0; i < 2; i++) { if (!isdigit((unsigned char)p[8 + i])) return json_null(); D = D * 10 + (p[8 + i] - '0'); }
    p += 10;

    if (*p && !isdigit((unsigned char)*p)) {   /* any single separator char */
        p++;
        if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return json_null();
        h = (p[0] - '0') * 10 + (p[1] - '0');
        p += 2;
        if (*p == ':') {
            p++;
            if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return json_null();
            mi = (p[0] - '0') * 10 + (p[1] - '0');
            p += 2;
            if (*p == ':') {
                p++;
                if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return json_null();
                sec = (p[0] - '0') * 10 + (p[1] - '0');
                p += 2;
                if (*p == '.' || *p == ',') {
                    int digits = 0;
                    p++;
                    if (!isdigit((unsigned char)*p)) return json_null();
                    while (isdigit((unsigned char)*p)) {
                        if (digits < 6) { us = us * 10 + (*p - '0'); digits++; }
                        p++;
                    }
                    while (digits < 6) { us *= 10; digits++; }
                }
            }
        }
        if (*p == 'Z') {                     /* lowercase z is rejected by fromisoformat */
            has_tz = 1;
            off_min = 0;
            p++;
        } else if (*p == '+' || *p == '-') {
            int sign = (*p == '-') ? -1 : 1, oh, om = 0;
            p++;
            if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return json_null();
            oh = (p[0] - '0') * 10 + (p[1] - '0');
            p += 2;
            if (*p == ':') {
                p++;
                if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return json_null();
                om = (p[0] - '0') * 10 + (p[1] - '0');
                p += 2;
            } else if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1])) {
                om = (p[0] - '0') * 10 + (p[1] - '0');
                p += 2;
            }
            has_tz = 1;
            off_min = sign * (oh * 60 + om);
        }
    }
    if (*p) return json_null();                        /* trailing junk */
    if (M < 1 || M > 12 || D < 1 || D > ax_days_in_month(Y, M)) return json_null();
    if (h > 23 || mi > 59 || sec > 59) return json_null();
    if (has_tz && (off_min > 24 * 60 || off_min < -24 * 60)) return json_null();

    if (us)
        snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d.%06d", Y, M, D, h, mi, sec, us);
    else
        snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d", Y, M, D, h, mi, sec);
    if (has_tz) {
        int mag = off_min < 0 ? -off_min : off_min;
        size_t used = strlen(buf);
        snprintf(buf + used, sizeof buf - used, "%c%02d:%02d",
                 off_min < 0 ? '-' : '+', mag / 60, mag % 60);
    }
    return json_string(buf);
}

/* ---- SimplexAdapter._handle_chat_item ---------------------------------- */

static json_t *ax_record(const char *action, const char *reason) {
    json_t *o = json_object();
    (void)reason;                    /* the drop rule label is logging, not a value */
    json_set(o, "action", json_string(action));
    json_set(o, "text", json_null());
    json_set(o, "message_type", json_null());
    json_set(o, "media_urls", json_null());
    json_set(o, "media_types", json_null());
    json_set(o, "chat_id", json_null());
    json_set(o, "chat_name", json_null());
    json_set(o, "chat_type", json_null());
    json_set(o, "user_id", json_null());
    json_set(o, "user_name", json_null());
    json_set(o, "file_id", json_null());
    json_set(o, "command", json_null());
    json_set(o, "ts", json_null());
    return o;
}

json_t *w31_adapter_c93276_p1_chat_item(const json_t *policy) {
    const json_t *item, *chat_info, *chat_item_data, *meta, *content, *msg_content,
        *item_direction, *allow, *file_info;
    const char *chat_type, *direction_type, *content_type, *msg_type_str, *text;
    char *chat_id = NULL, *sender_id = NULL;
    json_t *sender_name = NULL, *chat_name = NULL, *record;
    int is_group, text_truthy, has_media = 0;
    const char *msg_type;

    if (!ax_obj(policy)) return ax_record("drop", "not_a_policy");
    item = ax_obj(json_obj_get(policy, "item"));
    if (!item) return ax_record("drop", "not_a_chat_item");
    chat_info = ax_obj(json_obj_get(item, "chatInfo"));
    chat_item_data = ax_obj(json_obj_get(item, "chatItem"));
    chat_type = ax_str_or(chat_info ? json_obj_get(chat_info, "type") : NULL, "");
    meta = chat_item_data ? ax_obj(json_obj_get(chat_item_data, "meta")) : NULL;
    content = chat_item_data ? ax_obj(json_obj_get(chat_item_data, "content")) : NULL;
    msg_content = content ? ax_obj(json_obj_get(content, "msgContent")) : NULL;
    item_direction = chat_item_data ? ax_obj(json_obj_get(chat_item_data, "chatDir")) : NULL;
    direction_type = item_direction ? ax_str_or(json_obj_get(item_direction, "type"), "") : "";

    if (!strcmp(direction_type, "directSnd") || !strcmp(direction_type, "groupSnd"))
        return ax_record("drop", "own_message");
    content_type = content ? ax_str_or(json_obj_get(content, "type"), "") : "";
    if (strcmp(content_type, "rcvMsgContent"))
        return ax_record("drop", "not_received_content");
    msg_type_str = msg_content ? ax_str_or(json_obj_get(msg_content, "type"), "") : "";

    text = "";
    text_truthy = 0;
    if (msg_content && ax_in_list(msg_type_str, AX_TEXT_TYPES, AX_COUNT(AX_TEXT_TYPES))) {
        const json_t *tv = json_obj_get(msg_content, "text");
        text_truthy = ax_truthy(tv);
        if (tv && tv->type == JSON_STRING) text = tv->str_val ? tv->str_val : "";
    }
    if (!text_truthy && strcmp(msg_type_str, "image") && strcmp(msg_type_str, "file")
        && strcmp(msg_type_str, "voice"))
        return ax_record("drop", "no_text");

    is_group = !strcmp(chat_type, "group");
    if (!strcmp(chat_type, "direct")) {
        const json_t *contact = chat_info ? ax_obj(json_obj_get(chat_info, "contact")) : NULL;
        sender_id = ax_value_str(contact ? json_obj_get(contact, "contactId") : NULL);
        chat_id = ax_dup_n(sender_id ? sender_id : "", sender_id ? strlen(sender_id) : 0);
        sender_name = w31_adapter_c93276_p1_display_name(contact, "profile", NULL);
        chat_name = json_copy(sender_name);
    } else if (is_group) {
        const json_t *group_info = chat_info ? ax_obj(json_obj_get(chat_info, "groupInfo")) : NULL;
        const json_t *member;
        char *gid = ax_value_str(group_info ? json_obj_get(group_info, "groupId") : NULL);
        size_t need = strlen(gid ? gid : "") + 7;
        chat_id = (char *)malloc(need);
        if (chat_id) snprintf(chat_id, need, "group:%s", gid ? gid : "");
        member = item_direction ? ax_obj(json_obj_get(item_direction, "groupMember")) : NULL;
        sender_id = ax_value_str(member ? json_obj_get(member, "memberId") : NULL);
        sender_name = w31_adapter_c93276_p1_display_name(member, "memberProfile", NULL);
        {
            json_t *def = json_string(chat_id ? chat_id : "");
            chat_name = w31_adapter_c93276_p1_display_name(group_info, "groupProfile", def);
            json_free(def);
        }
        allow = json_obj_get(policy, "group_allow_from");
        if (!allow || allow->type != JSON_ARRAY || json_len(allow) == 0) {
            free(chat_id); free(sender_id); free(gid);
            json_free(sender_name); json_free(chat_name);
            return ax_record("drop", "no_group_allowlist");
        }
        if (!ax_group_allowed(allow, gid)) {
            free(chat_id); free(sender_id); free(gid);
            json_free(sender_name); json_free(chat_name);
            return ax_record("drop", "group_not_allowed");
        }
        free(gid);
    } else {
        return ax_record("drop", "unsupported_chat_type");
    }
    if (!sender_id || !sender_id[0]) {
        free(chat_id); free(sender_id);
        json_free(sender_name); json_free(chat_name);
        return ax_record("drop", "no_sender");
    }

    record = ax_record("handle", NULL);
    json_set(record, "media_urls", json_array());   /* MessageEvent defaults: [] */
    json_set(record, "media_types", json_array());
    json_set(record, "chat_id", json_string(chat_id ? chat_id : ""));
    json_set(record, "chat_type", json_string(is_group ? "group" : "dm"));
    json_set(record, "user_id", json_string(sender_id));
    json_set(record, "user_name",
             json_string(ax_truthy(sender_name) ? ax_str_or(sender_name, sender_id) : sender_id));
    json_set(record, "chat_name",
             (chat_name && chat_name->type == JSON_STRING) ? json_copy(chat_name) : json_string(""));
    json_free(chat_name);

    /* attachment: chatItem.chatItem.file, sibling of meta/content/chatDir */
    file_info = chat_item_data ? json_obj_get(chat_item_data, "file") : NULL;
    if (file_info && file_info->type == JSON_OBJECT) {
        const json_t *file_source = ax_obj(json_obj_get(file_info, "fileSource"));
        const json_t *fp_node = file_source ? json_obj_get(file_source, "filePath") : NULL;
        const json_t *fid_node = json_obj_get(file_info, "fileId");
        const char *file_path = ax_str(fp_node);
        char *ext;

        if (file_path && file_path[0]) {
            json_t *suffix = w31_adapter_c93276_p1_path_suffix(file_path);
            ext = ax_dup_n(ax_str_or(suffix, ""), strlen(ax_str_or(suffix, "")));
            json_free(suffix);
        } else {
            const json_t *name_node = json_obj_get(file_info, "fileName");
            const char *name = ax_str(name_node);
            if (name && name[0]) {
                json_t *suffix = w31_adapter_c93276_p1_path_suffix(name);
                ext = ax_dup_n(ax_str_or(suffix, ""), strlen(ax_str_or(suffix, "")));
                json_free(suffix);
            } else {
                ext = ax_dup_n("", 0);
            }
        }
        if ((!file_path || !file_path[0])
            && ax_in_list_ci(ext, AX_AUDIO_EXTS, AX_COUNT(AX_AUDIO_EXTS))
            && fid_node && fid_node->type != JSON_NULL) {
            json_t *may = w31_adapter_c93276_p1_may_receive_file(policy);
            int ok = ax_truthy(may);
            json_free(may);
            if (ok) {
                /* the item is parked in _pending_file_transfers: no event yet */
                char buf[128];
                char *fid_s = ax_value_str(fid_node);
                json_t *deferred = ax_record("defer", NULL);
                snprintf(buf, sizeof buf, "/freceive %s", fid_s ? fid_s : "");
                json_set(deferred, "file_id", json_copy(fid_node));
                json_set(deferred, "command", json_string(buf));
                free(fid_s);
                free(chat_id); free(sender_id); free(ext);
                json_free(sender_name);
                json_free(record);
                return deferred;
            }
        }
        if (file_path && file_path[0]) {
            json_t *urls = json_array(), *types = json_array(), *mime;
            json_append(urls, json_string(file_path));
            mime = w31_adapter_c93276_p1_mime_for_ext(ext);
            json_append(types, mime);
            json_set(record, "media_urls", urls);
            json_set(record, "media_types", types);
            has_media = 1;
        }
        free(ext);
    }

    if (!has_media) {
        msg_type = "text";
    } else {
        const json_t *types = json_obj_get(record, "media_types");
        size_t i, n = types ? json_len(types) : 0;
        int matched = 0;
        msg_type = "document";
        for (i = 0; i < n && !matched; i++) {
            const char *mt = ax_str(json_get(types, i));
            if (mt && !strncmp(mt, "audio/", 6)) { msg_type = "voice"; matched = 1; }
            else if (mt && !strncmp(mt, "image/", 6)) { msg_type = "photo"; matched = 1; }
        }
    }
    json_set(record, "message_type", json_string(msg_type));

    {
        const json_t *t1 = meta ? json_obj_get(meta, "itemTs") : NULL;
        const json_t *t2 = meta ? json_obj_get(meta, "createdAt") : NULL;
        const char *ts_str = ax_truthy(t1) ? ax_str(t1) : (ax_truthy(t2) ? ax_str(t2) : NULL);
        if (ts_str && ts_str[0]) json_set(record, "ts", w31_adapter_c93276_p1_ts_iso(ts_str));
    }
    json_set(record, "text", json_string(text_truthy ? text : ""));
    json_set(record, "action",
             json_string((!strcmp(msg_type, "text") && text_truthy) ? "batch" : "handle"));

    free(chat_id);
    free(sender_id);
    json_free(sender_name);
    return record;
}

/* ------------------------------------------------------------- gate p2 --- */

json_t *w31_adapter_c93276_p2_send_cmd(const char *chat_id, const json_t *items) {
    json_t *out = json_object();
    const char *id = chat_id ? chat_id : "";
    char *target;
    if (!strncmp(id, "group:", 6)) {
        size_t n = strlen(id + 6) + 2;
        target = (char *)malloc(n);
        if (target) snprintf(target, n, "#%s", id + 6);
    } else {
        size_t n = strlen(id) + 2;
        target = (char *)malloc(n);
        if (target) snprintf(target, n, "@%s", id);
    }
    json_set(out, "target", json_string(target ? target : ""));
    json_set(out, "items", items ? json_copy(items) : json_array());
    free(target);
    return out;
}

json_t *w31_adapter_c93276_p2_split_media(const char *content) {
    json_t *out = json_object();
    json_t *media = json_array();
    const char *p = content ? content : "";
    char *kept;
    size_t n;

    /* re.findall(r"MEDIA:(\S+)") */
    while ((p = strstr(p, "MEDIA:")) != NULL) {
        const char *start = p + 6;
        const char *end = start;
        while (*end && !ax_space((unsigned char)*end)) end++;
        if (end > start) {
            char *one = ax_dup_n(start, (size_t)(end - start));
            json_append(media, json_string(one ? one : ""));
            free(one);
            p = end;
        } else {
            p = start;
        }
    }
    /* re.sub(r"MEDIA:\S+", "", content).strip() */
    kept = (char *)malloc(strlen(content ? content : "") + 1);
    n = 0;
    p = content ? content : "";
    while (*p) {
        if (!strncmp(p, "MEDIA:", 6)) {
            const char *end = p + 6;
            while (*end && !ax_space((unsigned char)*end)) end++;
            if (end > p + 6) { p = end; continue; }
        }
        if (kept) kept[n++] = *p;
        p++;
    }
    if (kept) kept[n] = '\0';
    {
        char *trim = ax_strip(kept ? kept : "");
        json_set(out, "text", json_string(trim ? trim : ""));
        free(trim);
    }
    free(kept);
    json_set(out, "media", media);
    return out;
}

json_t *w31_adapter_c93276_p2_is_voice_ext(const char *ext) {
    return json_bool(ax_in_list_ci(ext, AX_VOICE_EXTS, AX_COUNT(AX_VOICE_EXTS)) != 0);
}

/* caption or "" */
static const char *ax_caption(const json_t *caption) {
    if (!caption) return "";
    if (caption->type == JSON_STRING) return caption->str_val ? caption->str_val : "";
    if (ax_truthy(caption)) return "";
    return "";
}

json_t *w31_adapter_c93276_p2_send_item(const char *kind, const char *path,
                                        const json_t *caption, const json_t *duration,
                                        const char *thumb) {
    json_t *item = json_object(), *msg = json_object();
    const char *cap = ax_caption(caption);

    if (kind && !strcmp(kind, "image")) {
        json_set(msg, "type", json_string("image"));
        json_set(msg, "image", json_string(thumb ? thumb : ""));
        json_set(msg, "text", json_string(cap));
        json_set(item, "filePath", json_string(path ? path : ""));
        json_set(item, "msgContent", msg);
        return item;
    }
    if (kind && !strcmp(kind, "voice")) {
        json_set(msg, "type", json_string("voice"));
        json_set(msg, "text", json_string(cap));
        json_set(msg, "duration", json_number(duration ? json_number_value(duration) : 0.0));
        json_set(item, "msgContent", msg);
        {
            json_t *src = json_object();
            json_set(src, "filePath", json_string(path ? path : ""));
            json_set(item, "fileSource", src);
        }
        return item;
    }
    json_set(msg, "type", json_string("file"));
    json_set(msg, "text", json_string(cap));
    json_set(item, "filePath", json_string(path ? path : ""));
    json_set(item, "msgContent", msg);
    return item;
}

json_t *w31_adapter_c93276_p2_corr_id(const json_t *counter, const json_t *now_ms) {
    char buf[160];
    char *ms = ax_value_str(now_ms);
    if (counter && counter->type != JSON_NULL) {
        char *c = ax_value_str(counter);
        snprintf(buf, sizeof buf, "hermes-%s-%s", c ? c : "0", ms ? ms : "0");
        free(c);
    } else {
        snprintf(buf, sizeof buf, "hermes-snd-%s", ms ? ms : "0");
    }
    free(ms);
    return json_string(buf);
}

json_t *w31_adapter_c93276_p2_retry_math(const json_t *backoff, const json_t *rnd) {
    json_t *out = json_object();
    double b = (backoff && backoff->type == JSON_NUMBER) ? backoff->num_val : 2.0;
    double r = (rnd && rnd->type == JSON_NUMBER) ? rnd->num_val : 0.0;
    double next = b * 2.0;
    if (next > 60.0) next = 60.0;
    json_set(out, "next", json_number(next));
    json_set(out, "sleep", json_number(b + b * 0.2 * r));
    return out;
}

json_t *w31_adapter_c93276_p2_health_stale(const json_t *elapsed) {
    double e = (elapsed && elapsed->type == JSON_NUMBER) ? elapsed->num_val : 0.0;
    return json_bool(e > 300.0);
}

static const char *ax_env_str(const json_t *env, const char *key) {
    const json_t *v = ax_obj(env) ? json_obj_get(env, key) : NULL;
    return ax_str(v);
}

json_t *w31_adapter_c93276_p2_init_config(const json_t *env, const json_t *extra) {
    json_t *out = json_object();
    const json_t *raw_url = ax_obj(extra) ? json_obj_get(extra, "ws_url") : NULL;
    const char *url = ax_str(raw_url);
    const char *env_auto = ax_env_str(env, "SIMPLEX_AUTO_ACCEPT");
    const char *group_env = ax_env_str(env, "SIMPLEX_GROUP_ALLOWED");
    const char *group_extra = ax_obj(extra) ? ax_str(json_obj_get(extra, "group_allowed")) : NULL;
    const char *delay_env = ax_env_str(env, "HERMES_SIMPLEX_TEXT_BATCH_DELAY");
    char *ws_url;
    int auto_accept;
    double delay;

    if (!url) url = "ws://127.0.0.1:5225";
    ws_url = ax_dup_n(url, strlen(url));
    if (ws_url) {
        size_t n = strlen(ws_url);
        while (n && ws_url[n - 1] == '/') ws_url[--n] = '\0';
    }
    json_set(out, "ws_url", json_string(ws_url ? ws_url : ""));
    free(ws_url);

    if (env_auto) {
        char *trim = ax_strip(env_auto);
        char *low = ax_lower(trim ? trim : "");
        auto_accept = !(low && (!strcmp(low, "0") || !strcmp(low, "false")
                                || !strcmp(low, "no") || low[0] == '\0'));
        free(low);
        free(trim);
    } else {
        const json_t *ea = ax_obj(extra) ? json_obj_get(extra, "auto_accept") : NULL;
        auto_accept = ea ? ax_truthy(ea) : 1;
    }
    json_set(out, "auto_accept", json_bool(auto_accept));

    {
        /* group_allow_from is a set: membership is the contract, so it is
         * reported deduped and sorted (a set has no order to preserve). */
        const char *group_src = (group_env && group_env[0]) ? group_env
                                                            : (group_extra ? group_extra : "");
        json_t *parsed = w31_adapter_c93276_p1_parse_comma_list(group_src);
        json_t *group_list = json_array();
        size_t i, j, n = parsed ? json_len(parsed) : 0;
        for (i = 0; i < n; i++) {
            const char *v = ax_str(json_get(parsed, i));
            int seen = 0;
            for (j = 0; j < json_len(group_list); j++)
                if (v && !strcmp(ax_str_or(json_get(group_list, j), ""), v)) { seen = 1; break; }
            if (!seen && v) json_append(group_list, json_string(v));
        }
        json_free(parsed);
        if (json_len(group_list) > 1) {
            size_t m = json_len(group_list), a, b;
            char **vals = (char **)calloc(m, sizeof(char *));
            if (vals) {
                json_t *sorted = json_array();
                for (a = 0; a < m; a++) {
                    const char *s = ax_str_or(json_get(group_list, a), "");
                    vals[a] = ax_dup_n(s, strlen(s));
                }
                for (a = 1; a < m; a++) {
                    char *key = vals[a];
                    b = a;
                    while (b > 0 && strcmp(vals[b - 1] ? vals[b - 1] : "", key ? key : "") > 0) {
                        vals[b] = vals[b - 1];
                        b--;
                    }
                    vals[b] = key;
                }
                for (a = 0; a < m; a++) {
                    json_append(sorted, json_string(vals[a] ? vals[a] : ""));
                    free(vals[a]);
                }
                free(vals);
                json_free(group_list);
                group_list = sorted;
            }
        }
        json_set(out, "group_allowed", group_list);
    }

    delay = delay_env ? atof(delay_env) : 0.8;
    json_set(out, "batch_delay", json_number(delay));
    json_set(out, "split_delay", json_number(delay));
    return out;
}

json_t *w31_adapter_c93276_p2_config_gate(const json_t *env, const json_t *extra,
                                          const json_t *websockets) {
    json_t *out = json_object();
    const json_t *url_node = ax_obj(env) ? json_obj_get(env, "SIMPLEX_WS_URL") : NULL;
    const json_t *extra_url = ax_obj(extra) ? json_obj_get(extra, "ws_url") : NULL;
    int check = ax_truthy(url_node) && ax_truthy(websockets);
    int valid = ax_truthy(url_node) || ax_truthy(extra_url);
    json_set(out, "check_requirements", json_bool(check));
    json_set(out, "validate_config", json_bool(valid));
    json_set(out, "is_connected", json_bool(valid));
    return out;
}

json_t *w31_adapter_c93276_p2_env_enablement(const json_t *env) {
    json_t *out;
    char *ws_url = ax_strip(ax_env_str(env, "SIMPLEX_WS_URL"));
    const char *auto_raw = ax_env_str(env, "SIMPLEX_AUTO_ACCEPT");
    const char *group_raw = ax_env_str(env, "SIMPLEX_GROUP_ALLOWED");
    const char *home_raw = ax_env_str(env, "SIMPLEX_HOME_CHANNEL");
    const char *home_name = ax_env_str(env, "SIMPLEX_HOME_CHANNEL_NAME");

    if (!ws_url || !ws_url[0]) { free(ws_url); return json_null(); }
    out = json_object();
    json_set(out, "ws_url", json_string(ws_url));
    free(ws_url);

    if (auto_raw) {
        char *trim = ax_strip(auto_raw);
        if (trim && trim[0]) {
            char *low = ax_lower(trim);
            json_set(out, "auto_accept",
                     json_bool(!(low && (!strcmp(low, "0") || !strcmp(low, "false")
                                         || !strcmp(low, "no")))));
            free(low);
        }
        free(trim);
    }
    if (group_raw) {
        char *trim = ax_strip(group_raw);
        if (trim && trim[0]) json_set(out, "group_allowed", json_string(trim));
        free(trim);
    }
    if (home_raw) {
        char *home = ax_strip(home_raw);
        if (home && home[0]) {
            json_t *hc = json_object();
            json_set(hc, "chat_id", json_string(home));
            json_set(hc, "name", json_string(home_name ? home_name : "Home"));
            json_set(out, "home_channel", hc);
        }
        free(home);
    }
    return out;
}

json_t *w31_adapter_c93276_p2_get_chat_info(const char *chat_id) {
    json_t *out = json_object();
    const char *id = chat_id ? chat_id : "";
    int is_group = !strncmp(id, "group:", 6);
    json_set(out, "chat_id", json_string(id));
    json_set(out, "type", json_string(is_group ? "group" : "dm"));
    json_set(out, "name", json_string(is_group ? id + 6 : id));
    return out;
}

json_t *w31_adapter_c93276_p2_list_channels(const json_t *contacts_resp,
                                            const json_t *groups_resp) {
    json_t *out;
    const json_t *contacts, *groups;
    size_t i, n;

    if (!contacts_resp || contacts_resp->type == JSON_NULL) return json_null();
    out = json_array();
    contacts = ax_obj(contacts_resp) ? json_obj_get(contacts_resp, "contacts") : NULL;
    n = (contacts && contacts->type == JSON_ARRAY) ? json_len(contacts) : 0;
    for (i = 0; i < n; i++) {
        const json_t *c = json_get(contacts, i);
        const json_t *cid, *dl;
        json_t *name, *label, *row;
        char *cid_s;
        if (!c || c->type != JSON_OBJECT) continue;
        cid = json_obj_get(c, "contactId");
        name = w31_adapter_c93276_p1_display_name(c, "profile", NULL);
        if ((!cid || cid->type == JSON_NULL) && !ax_truthy(name)) { json_free(name); continue; }
        if (ax_truthy(name)) {
            label = json_copy(name);
        } else {
            cid_s = ax_value_str(cid);
            label = json_string(cid_s ? cid_s : "");
            free(cid_s);
        }
        json_free(name);
        (void)dl;
        row = json_object();
        json_set(row, "id", json_copy(label));
        json_set(row, "name", label);
        json_set(row, "type", json_string("dm"));
        json_append(out, row);
    }
    if (groups_resp && groups_resp->type == JSON_OBJECT) {
        groups = json_obj_get(groups_resp, "groups");
        n = (groups && groups->type == JSON_ARRAY) ? json_len(groups) : 0;
        for (i = 0; i < n; i++) {
            const json_t *g = json_get(groups, i);
            const json_t *gid;
            json_t *name, *row;
            char *gid_s, *id_buf;
            size_t need;
            if (g && g->type == JSON_ARRAY && json_len(g) > 0) g = json_get(g, 0);
            if (!g || g->type != JSON_OBJECT) continue;
            gid = json_obj_get(g, "groupId");
            if (!gid || gid->type == JSON_NULL) continue;
            name = w31_adapter_c93276_p1_display_name(g, "groupProfile", NULL);
            gid_s = ax_value_str(gid);
            need = strlen(gid_s ? gid_s : "") + 7;
            id_buf = (char *)malloc(need);
            if (id_buf) snprintf(id_buf, need, "group:%s", gid_s ? gid_s : "");
            row = json_object();
            json_set(row, "id", json_string(id_buf ? id_buf : ""));
            json_set(row, "name", ax_truthy(name) ? json_copy(name) : json_string(gid_s ? gid_s : ""));
            json_set(row, "type", json_string("group"));
            json_append(out, row);
            free(id_buf);
            free(gid_s);
            json_free(name);
        }
    }
    return out;
}

json_t *w31_adapter_c93276_p2_prepare_image(const char *file_path) {
    json_t *out = json_object();
    json_t *suffix = w31_adapter_c93276_p1_path_suffix(file_path);
    const char *s = ax_str_or(suffix, "");
    int needs_png = strcmp(s, ".png") && strcmp(s, ".jpg") && strcmp(s, ".jpeg");
    json_set(out, "needs_png", json_bool(needs_png));
    json_set(out, "suffix", json_copy(suffix));
    json_free(suffix);
    return out;
}

json_t *w31_adapter_c93276_p2_text_batch_key(const char *platform, const char *chat_id) {
    size_t need = strlen(platform ? platform : "") + strlen(chat_id ? chat_id : "") + 2;
    char *buf = (char *)malloc(need);
    json_t *out;
    if (buf) snprintf(buf, need, "%s:%s", platform ? platform : "", chat_id ? chat_id : "");
    out = json_string(buf ? buf : "");
    free(buf);
    return out;
}
