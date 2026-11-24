/*
 * feishu_admission_diagnostics.c -- real C11 port of the operator-facing
 * admission diagnostic in upstream Hermes
 *   plugins/platforms/feishu/feishu_admission_diagnostics.py.
 *
 * PoP: w22_feishuadmi_eb8884_empty_allowlist_drop_warning @ plugins/platforms/feishu/feishu_admission_diagnostics.py:empty_allowlist_drop_warning
 *
 * The module has one decision: a group drop is only surfaced when the deny was
 * never configured. The rest (the adapter's DEBUG drops, the
 * FEISHU_ALLOWED_USERS parse) is bound to a running adapter and stays upstream.
 */
#include "hermes_json.h"
#include "noc_w22_feishuadmi_eb8884.h"

#include <stdlib.h>
#include <string.h>

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int fa8884_truthy(const json_t *v) {
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

/* `chat_id in group_rules`: mapping keys, sequence elements, or substring. */
static int fa8884_contains(const json_t *container, const char *chat_id) {
    size_t i;
    if (!container || !chat_id) return 0;
    switch (container->type) {
    case JSON_OBJECT:
        return json_obj_get(container, chat_id) != NULL;
    case JSON_ARRAY:
        for (i = 0; i < container->c.count; i++) {
            const json_t *item = container->c.items[i];
            if (item && item->type == JSON_STRING && item->str_val &&
                !strcmp(item->str_val, chat_id)) return 1;
        }
        return 0;
    case JSON_STRING:
        return container->str_val && strstr(container->str_val, chat_id) != NULL;
    default:
        return 0;
    }
}

/* Growable byte buffer: always NUL-terminated, even with nothing appended. */
typedef struct { char *buf; size_t len, cap; } fa8884_buf;

static int fa8884_buf_init(fa8884_buf *b) {
    b->cap = 512;
    b->len = 0;
    b->buf = (char *)malloc(b->cap);
    if (!b->buf) { b->cap = 0; return 0; }
    b->buf[0] = '\0';
    return 1;
}

static int fa8884_buf_add(fa8884_buf *b, const char *s) {
    size_t n = s ? strlen(s) : 0;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *grown;
        while (b->len + n + 1 > cap) cap *= 2;
        grown = (char *)realloc(b->buf, cap);
        if (!grown) return 0;
        b->buf = grown;
        b->cap = cap;
    }
    if (n) memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
    return 1;
}

static const char *const FA8884_HEAD =
    "[Feishu] Dropped a group message in chat ";

/* The warning body verbatim, in sentence-sized chunks (ASCII source only). */
static const char *const FA8884_BODY[] = {
    ": group policy is 'allowlist' and FEISHU_ALLOWED_USERS is empty for this profile, "
    "so every human group message is rejected. ",
    "Set FEISHU_GROUP_POLICY=open or FEISHU_ALLOWED_USERS=<open_id,...> in this profile's .env ",
    "(under multiplex each profile reads only its own .env; the default profile's value is not ",
    "inherited), or platforms.feishu.extra.group_rules in its config.yaml. ",
    "Further drops are logged at DEBUG.",
    NULL
};

json_t *w22_feishuadmi_eb8884_empty_allowlist_drop_warning(const char *chat_id,
                                                           const json_t *group_rules,
                                                           const char *default_group_policy,
                                                           const json_t *allowed_group_users) {
    fa8884_buf b;
    json_t *out;
    int i;

    /* `chat_id in group_rules or default_group_policy != "allowlist" or allowed_group_users` */
    if (fa8884_contains(group_rules, chat_id ? chat_id : "") ||
        strcmp(default_group_policy ? default_group_policy : "", "allowlist") != 0 ||
        fa8884_truthy(allowed_group_users)) {
        return json_null();
    }

    if (!fa8884_buf_init(&b)) return json_null();
    if (!fa8884_buf_add(&b, FA8884_HEAD)) { free(b.buf); return json_null(); }
    if (!fa8884_buf_add(&b, chat_id ? chat_id : "")) { free(b.buf); return json_null(); }
    for (i = 0; FA8884_BODY[i]; i++) {
        if (!fa8884_buf_add(&b, FA8884_BODY[i])) { free(b.buf); return json_null(); }
    }
    out = json_string(b.buf);
    free(b.buf);
    return out;
}
