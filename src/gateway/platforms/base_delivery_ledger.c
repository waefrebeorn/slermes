/*
 * base_delivery_ledger.c -- real C11 port of upstream Hermes
 *   gateway/platforms/base_delivery_ledger.py.
 *
 * PoP: w21_basedelive_f1f9e2_record_final @ gateway/platforms/base_delivery_ledger.py:record_final_obligation
 * PoP: w21_basedelive_f1f9e2_finalize_action @ gateway/platforms/base_delivery_ledger.py:finalize_final_obligation
 * PoP: w21_basedelive_f1f9e2_env_probe @ gateway/platforms/base_delivery_ledger.py:record_final_obligation
 *
 * The coroutines are the ledger store's callers, so the store (record_obligation,
 * mark_attempting, mark_delivered, mark_failed), the runner redelivery hooks and
 * the two classifier verdicts arrive as `world` fields. Everything the module
 * itself decides -- the slash-command skip, the ledger_enabled gate, which
 * message id names the row, the obligation id hash, and which redelivery arm a
 * failed send takes -- is implemented here.
 */
#include "hermes_json.h"
#include "hash.h"
#include "noc_w21_basedelive_f1f9e2.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness of a decoded JSON value; absent (NULL) reads as None. */
static int bdl_truthy(const json_t *v) {
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

static int bdl_is_py_int(double d) {
    if (!isfinite(d)) return 0;
    if (floor(d) != d) return 0;
    return d >= -9.007199254740992e15 && d <= 9.007199254740992e15;
}

/* str(value) for the wire scalars this module can be handed. */
static void bdl_str(const json_t *v, char *buf, size_t cap) {
    double d;
    if (!v || v->type == JSON_NULL) {
        if (cap) buf[0] = '\0';
        return;
    }
    switch (v->type) {
    case JSON_NULL:                       /* handled above; keeps -Wswitch quiet */
        if (cap) buf[0] = '\0';
        return;
    case JSON_STRING:
        snprintf(buf, cap, "%s", v->str_val ? v->str_val : "");
        return;
    case JSON_BOOL:
        snprintf(buf, cap, "%s", v->bool_val ? "True" : "False");
        return;
    case JSON_NUMBER:
        d = v->num_val;
        if (bdl_is_py_int(d)) snprintf(buf, cap, "%lld", (long long)d);
        else snprintf(buf, cap, "%.17g", d);
        return;
    case JSON_ARRAY:
        snprintf(buf, cap, "[");
        return;
    case JSON_OBJECT:
        snprintf(buf, cap, "{");
        return;
    }
    if (cap) buf[0] = '\0';
}

/* str(v or ""): the falsy value collapses to the empty string first. */
static void bdl_str_or_empty(const json_t *v, char *buf, size_t cap) {
    if (!bdl_truthy(v)) {
        if (cap) buf[0] = '\0';
        return;
    }
    bdl_str(v, buf, cap);
}

static const char *bdl_skip_ws(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
    return s;
}

json_t *w21_basedelive_f1f9e2_record_final(const json_t *world) {
    char text[8192];
    char prefix[256];
    char ref[4096];
    char content[16384];
    char payload[32768];
    const char *head;
    char *hex;
    json_t *out;

    if (!world || world->type != JSON_OBJECT) world = NULL;

    bdl_str_or_empty(world ? json_obj_get(world, "text") : NULL, text, sizeof(text));

    /* if is_ephemeral_response or str(event.text or "").lstrip().startswith(
     *        ("/", adapter.typed_command_prefix or "!")): return None */
    if (world && json_get_bool(world, "is_ephemeral", false)) return json_null();
    {
        json_t *pv = world ? json_obj_get(world, "typed_command_prefix") : NULL;
        if (!bdl_truthy(pv)) {
            /* adapter.typed_command_prefix or "!" */
            snprintf(prefix, sizeof(prefix), "!");
        } else if (pv->type != JSON_STRING) {
            /* startswith(("/", 5)) raises TypeError: caught, nothing saved. */
            return json_null();
        } else {
            snprintf(prefix, sizeof(prefix), "%s", pv->str_val ? pv->str_val : "");
        }
    }
    head = bdl_skip_ws(text);
    if (head[0] == '/' || (prefix[0] && head[0] == prefix[0])) return json_null();

    /* if not await asyncio.to_thread(ledger_enabled): return None */
    if (!world || !json_get_bool(world, "ledger_enabled", false)) return json_null();

    /* _ledger_id = getattr(event, "ledger_message_id", None)
     * if _ledger_id is None: _ledger_id = getattr(event, "message_id", "") */
    {
        json_t *lid = world ? json_obj_get(world, "ledger_message_id") : NULL;
        if (!lid || lid->type == JSON_NULL) lid = json_obj_get(world, "message_id");
        bdl_str_or_empty(lid, ref, sizeof(ref));
    }

    snprintf(content, sizeof(content), "%s",
             world ? json_get_str(world, "content", "") : "");

    /* compute_obligation_id(session_key, str(_ledger_id or ""), text_content):
     * sha256(f"{session_key}|{message_ref}|{content}").hexdigest()[:24] */
    snprintf(payload, sizeof(payload), "%s|%s|%s",
             world ? json_get_str(world, "session_key", "") : "", ref, content);
    hex = hash_sha256_hex((const unsigned char *)payload, strlen(payload));
    if (!hex) return json_null();
    if (strlen(hex) > 24) hex[24] = '\0';      /* NUL-terminate after shortening */
    out = json_string(hex);
    free(hex);
    return out;
}

json_t *w21_basedelive_f1f9e2_finalize_action(const json_t *world) {
    json_t *out = json_object();
    const char *action = "none";

    if (!world || world->type != JSON_OBJECT) world = NULL;

    if (world && json_get_bool(world, "success", false)) {
        /* await asyncio.to_thread(mark_delivered, obligation_id); return */
        json_set(out, "mark", json_string("delivered"));
        json_set(out, "action", json_string("none"));
        return out;
    }

    /* mark_failed(obligation_id, str(getattr(result, "error", "") or "")) */
    if (world && json_get_bool(world, "reconnect_only", false)) {
        json_t *dead = json_obj_get(world, "dead_kind");
        (void)dead;                     /* the elif is unreachable on this arm */
        if (!json_get_bool(world, "live_is_delivery_adapter", false) &&
            json_get_bool(world, "redeliver_callable", false)) {
            action = "redeliver";
        }
    } else {
        json_t *dead = world ? json_obj_get(world, "dead_kind") : NULL;
        if (!dead || dead->type == JSON_NULL) {
            if (world && json_get_bool(world, "schedule_callable", false)) action = "schedule";
        }
    }
    json_set(out, "mark", json_string("failed"));
    json_set(out, "action", json_string(action));
    return out;
}

json_t *w21_basedelive_f1f9e2_env_probe(void) {
    const char *sler = getenv("SLERMES_HOME");
    const char *home = getenv("HOME");
    json_t *out = json_object();
    json_set(out, "sler", json_bool(sler && sler[0]));
    json_set(out, "home", json_bool(home && home[0]));
    json_set(out, "same", json_bool(sler && home && !strcmp(sler, home)));
    return out;
}
