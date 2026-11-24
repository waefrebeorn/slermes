/*
 * cua_backend_input.c - real C11 port of the input side of upstream Hermes
 * tools/computer_use/cua_backend_input.py (_InputMixin: pointer / keyboard / value-setter
 * actions against the sticky target). The transport (_action), the session capability probes
 * and bring_to_front are injected through the request, so every decision rung is native C.
 * PoP: w19_cuabackend_3d8b54_refuse @ tools/computer_use/cua_backend_input.py:_refuse
 * PoP: w19_cuabackend_3d8b54_target_args @ tools/computer_use/cua_backend_input.py:_InputMixin._target_args
 * PoP: w19_cuabackend_3d8b54_apply_delivery @ tools/computer_use/cua_backend_input.py:_InputMixin._apply_delivery
 * PoP: w19_cuabackend_3d8b54_parse_key_combo @ tools/computer_use/cua_backend_parse.py:_parse_key_combo
 * PoP: w19_cuabackend_3d8b54_run_input_action @ tools/computer_use/cua_backend_input.py:_InputMixin._run_input_action
 * PoP: w19_cuabackend_3d8b54_click @ tools/computer_use/cua_backend_input.py:_InputMixin.click
 * PoP: w19_cuabackend_3d8b54_drag @ tools/computer_use/cua_backend_input.py:_InputMixin.drag
 * PoP: w19_cuabackend_3d8b54_scroll @ tools/computer_use/cua_backend_input.py:_InputMixin.scroll
 * PoP: w19_cuabackend_3d8b54_type_text @ tools/computer_use/cua_backend_input.py:_InputMixin.type_text
 * PoP: w19_cuabackend_3d8b54_key @ tools/computer_use/cua_backend_input.py:_InputMixin.key
 * PoP: w19_cuabackend_3d8b54_set_value @ tools/computer_use/cua_backend_input.py:_InputMixin.set_value
 */
#include "noc_w19_cuabackend_3d8b54.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The em dash is UTF-8 in the message payload but ASCII source, per the port rules. */
#define NO_TARGET_MSG     "No active window \xE2\x80\x94" " call capture() first."
#define BTF_UNSUPPORTED   "The connected cua-driver does not advertise the standalone bring_to_front tool."
#define FOREGROUND_UNSUP  "The connected cua-driver action schema does not accept delivery_mode, so " \
                          "foreground delivery is unavailable. Use another verified rung without " \
                          "assuming the reported package version describes the live schema."

typedef struct {
    int has_pid;
    int has_window;
    int supports_delivery_mode;
    int supports_scroll_coords;
    int has_btf_tool;
    int btf_ok;
    int action_ok;
} cua_state;

static const json_t *jget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static int present(const json_t *o, const char *k) {
    const json_t *v = jget(o, k);
    return v && v->type != JSON_NULL;
}

static int as_bool(const json_t *o, const char *k, int def) {
    const json_t *v = jget(o, k);
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    return def;
}

static double node_num(const json_t *v, double def) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static double as_num(const json_t *o, const char *k, double def) {
    return node_num(jget(o, k), def);
}

static const char *as_str(const json_t *o, const char *k, const char *def) {
    const json_t *v = jget(o, k);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}

static cua_state state_of(const json_t *req) {
    cua_state st;
    st.has_pid = present(req, "pid");
    st.has_window = present(req, "window_id");
    st.supports_delivery_mode = as_bool(req, "supports_delivery_mode", 0);
    st.supports_scroll_coords = as_bool(req, "supports_scroll_coords", 0);
    st.has_btf_tool = as_bool(req, "has_btf_tool", 0);
    st.btf_ok = as_bool(req, "btf_ok", 1);
    st.action_ok = as_bool(req, "action_ok", 1);
    return st;
}

static json_t *base_result(int ok, const char *action, const char *message) {
    json_t *r = json_object();
    json_set(r, "ok", json_bool(ok ? 1 : 0));
    json_set(r, "action", json_string(action ? action : ""));
    json_set(r, "message", json_string(message ? message : ""));
    json_set(r, "meta", json_object());
    return r;
}

json_t *w19_cuabackend_3d8b54_refuse(const char *action, const char *message,
                                     const char *code, const char *delivery_mode) {
    json_t *r = base_result(0, action, message);
    if (code) json_set(r, "code", json_string(code));
    if (delivery_mode) json_set(r, "delivery_mode", json_string(delivery_mode));
    return r;
}

static void merge_obj(json_t *dst, const json_t *src) {
    size_t i;
    if (!dst || !src || src->type != JSON_OBJECT) return;
    for (i = 0; i < json_object_size(src); i++) {
        const char *k = json_object_get_key_at(src, i);
        if (!k) continue;
        json_set(dst, k, json_copy(json_object_get_at(src, i)));
    }
}

/* The refusal a planner reply carries, or NULL when it proceeds. */
static const json_t *refusal_of(const json_t *reply) {
    const json_t *v = jget(reply, "refusal");
    return (v && v->type != JSON_NULL) ? v : NULL;
}

json_t *w19_cuabackend_3d8b54_target_args(const char *action, const json_t *pid,
                                          const json_t *window_id, int need_window) {
    json_t *out = json_object();
    int has_pid = pid && pid->type != JSON_NULL;
    int has_window = window_id && window_id->type != JSON_NULL;
    if (!has_pid || (need_window && !has_window)) {
        json_set(out, "refusal", w19_cuabackend_3d8b54_refuse(action, NO_TARGET_MSG, NULL, NULL));
        json_set(out, "args", json_object());
    } else {
        json_t *args = json_object();
        json_set(args, "pid", json_copy(pid));
        if (need_window) json_set(args, "window_id", json_copy(window_id));
        json_set(out, "refusal", json_null());
        json_set(out, "args", args);
    }
    return out;
}

json_t *w19_cuabackend_3d8b54_apply_delivery(const char *action, const char *delivery_mode,
                                             int supports_delivery_mode) {
    json_t *out = json_object();
    json_t *args = json_object();
    const char *dm = delivery_mode ? delivery_mode : "";
    json_set(out, "args", args);
    if (!dm[0] || !strcmp(dm, "background")) {
        json_set(out, "refusal", json_null());
        return out;
    }
    if (strcmp(dm, "foreground") != 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "unknown delivery_mode '%s' \xE2\x80\x94"
                 " use background|foreground.", dm);
        json_set(out, "refusal", w19_cuabackend_3d8b54_refuse(action, buf, "bad_delivery_mode", NULL));
        return out;
    }
    if (!supports_delivery_mode) {
        json_set(out, "refusal",
                 w19_cuabackend_3d8b54_refuse(action, FOREGROUND_UNSUP, "foreground_unsupported", "foreground"));
        return out;
    }
    json_set(args, "delivery_mode", json_string("foreground"));
    json_set(out, "refusal", json_null());
    return out;
}

/* _parse_key_combo: split on '+' or '-', strip + lower each part, map aliases, last key wins. */
json_t *w19_cuabackend_3d8b54_parse_key_combo(const char *keys) {
    static const char *const MODIFIERS[] = {"cmd", "command", "shift", "option", "alt", "ctrl",
                                            "control", "fn", NULL};
    json_t *out = json_object();
    json_t *mods = json_array();
    char *key_copy = NULL;
    size_t len = strlen(keys ? keys : "");
    char *work = (char *)malloc(len + 1);

    if (work) {
        char *p;
        strcpy(work, keys ? keys : "");
        p = work;
        for (;;) {
            char *sep = p;
            char *part;
            size_t n;
            while (*sep && *sep != '+' && *sep != '-') sep++;
            n = (size_t)(sep - p);
            part = (char *)malloc(n + 1);
            if (part) {
                size_t a = 0, b = n;
                memcpy(part, p, n);
                part[n] = '\0';
                while (a < b && (part[a] == ' ' || part[a] == '\t' || part[a] == '\n'
                                 || part[a] == '\r' || part[a] == '\f' || part[a] == '\v')) a++;
                while (b > a && (part[b - 1] == ' ' || part[b - 1] == '\t' || part[b - 1] == '\n'
                                 || part[b - 1] == '\r' || part[b - 1] == '\f' || part[b - 1] == '\v')) b--;
                part[b] = '\0';
                if (b > a) {
                    char *lower = part + a;
                    const char *norm;
                    size_t j;
                    int is_mod = 0;
                    int i;
                    for (j = 0; lower[j]; j++)
                        if (lower[j] >= 'A' && lower[j] <= 'Z') lower[j] = (char)(lower[j] - 'A' + 'a');
                    /* _KEY_ALIASES */
                    norm = lower;
                    if (!strcmp(lower, "command")) norm = "cmd";
                    else if (!strcmp(lower, "alt")) norm = "option";
                    else if (!strcmp(lower, "control")) norm = "ctrl";
                    for (i = 0; MODIFIERS[i]; i++)
                        if (!strcmp(MODIFIERS[i], norm)) { is_mod = 1; break; }
                    if (is_mod) {
                        json_append(mods, json_string(norm));
                    } else {
                        size_t ln = strlen(lower) + 1;
                        char *copy = (char *)malloc(ln);
                        if (copy) memcpy(copy, lower, ln);
                        free(key_copy);
                        key_copy = copy;
                    }
                }
                free(part);
            }
            if (!*sep) break;
            p = sep + 1;
        }
        free(work);
    }
    json_set(out, "key", key_copy ? json_string(key_copy) : json_null());
    json_set(out, "modifiers", mods);
    free(key_copy);
    return out;
}

static json_t *pointer_with(json_t *args, const cua_state *st, const json_t *window_id,
                            const json_t *extra1, const char *what1,
                            const json_t *extra2, const char *what2,
                            const char *missing_msg, const char *tool) {
    const json_t *extra = NULL;
    const char *what = NULL;
    if (extra1) { extra = extra1; what = what1; }
    else if (extra2) { extra = extra2; what = what2; }
    if (extra) {
        if (!st->has_window) {
            char buf[128];
            snprintf(buf, sizeof(buf), "No active window_id for %s.", what);
            return w19_cuabackend_3d8b54_refuse(tool, buf, NULL, NULL);
        }
        merge_obj(args, extra);
        json_set(args, "window_id", json_copy(window_id));
        return NULL;
    }
    return missing_msg ? w19_cuabackend_3d8b54_refuse(tool, missing_msg, NULL, NULL) : NULL;
}

static json_t *execute(const char *action, const json_t *args, const cua_state *st) {
    json_t *r = base_result(st->action_ok, action, st->action_ok ? "" : "action failed");
    json_set(r, "sent", json_copy(args));
    return r;
}

static json_t *run_ladder(const char *action, json_t *args, const char *delivery_mode,
                          int bring_to_front, const cua_state *st) {
    json_t *dl = w19_cuabackend_3d8b54_apply_delivery(action, delivery_mode, st->supports_delivery_mode);
    const json_t *ref = refusal_of(dl);
    if (ref) {
        json_t *r = json_copy(ref);
        json_free(dl);
        return r;
    }
    merge_obj(args, jget(dl, "args"));
    json_free(dl);

    if (bring_to_front) {
        if (!delivery_mode || strcmp(delivery_mode, "foreground") != 0) {
            return w19_cuabackend_3d8b54_refuse(action, "bring_to_front requires delivery_mode='foreground'.",
                                                "bring_to_front_requires_foreground", NULL);
        }
        if (!st->has_btf_tool) {
            return w19_cuabackend_3d8b54_refuse(action, BTF_UNSUPPORTED,
                                                "bring_to_front_unsupported", "foreground");
        }
        if (!st->has_pid || !st->has_window) {
            return w19_cuabackend_3d8b54_refuse(action,
                                                "Capture an exact target before requesting persistent foreground focus.",
                                                "bring_to_front_target_required", "foreground");
        }
        if (!st->btf_ok) return base_result(0, "bring_to_front", "focus failed");
    }

    {
        json_t *result = execute(action, args, st);
        if (bring_to_front) {
            json_t *focus = json_object();
            json_set(focus, "invoked", json_bool(1));
            json_set(focus, "tool", json_string("bring_to_front"));
            json_set(json_obj_get(result, "meta"), "foreground_focus", focus);
        }
        return result;
    }
}

static const char *req_delivery(const json_t *req) {
    return present(req, "delivery_mode") ? as_str(req, "delivery_mode", "") : NULL;
}

json_t *w19_cuabackend_3d8b54_run_input_action(const json_t *req) {
    cua_state st = state_of(req);
    const char *action = as_str(req, "action", "");
    json_t *args = json_object();
    json_t *out;
    const json_t *given = jget(req, "args");
    if (given) merge_obj(args, given);
    out = run_ladder(action, args, req_delivery(req), as_bool(req, "bring_to_front", 0), &st);
    json_free(args);
    return out;
}

json_t *w19_cuabackend_3d8b54_click(const json_t *req) {
    cua_state st = state_of(req);
    const json_t *wid = jget(req, "window_id");
    json_t *ta = w19_cuabackend_3d8b54_target_args("click", jget(req, "pid"), wid, 0);
    const json_t *ref = refusal_of(ta);
    json_t *args;
    json_t *out;
    const char *tool;
    const char *button_raw = as_str(req, "button", NULL);
    char *button_norm;
    json_t *variant1 = NULL;
    json_t *variant2 = NULL;
    const json_t *mods;

    if (ref) {
        out = json_copy(ref);
        json_free(ta);
        return out;
    }
    args = json_obj_get(ta, "args");

    /* button_norm = (button or "left").lower() */
    {
        const char *src = (button_raw && button_raw[0]) ? button_raw : "left";
        size_t i;
        button_norm = (char *)malloc(strlen(src) + 1);
        if (button_norm) {
            strcpy(button_norm, src);
            for (i = 0; button_norm[i]; i++)
                if (button_norm[i] >= 'A' && button_norm[i] <= 'Z')
                    button_norm[i] = (char)(button_norm[i] - 'A' + 'a');
        }
    }
    if (!button_norm || (strcmp(button_norm, "left") && strcmp(button_norm, "right")
                         && strcmp(button_norm, "middle"))) {
        char buf[256];
        snprintf(buf, sizeof(buf), "unknown button '%s' \xE2\x80\x94 expected left, right, middle.",
                 button_raw ? button_raw : "");
        free(button_norm);
        json_free(ta);
        return w19_cuabackend_3d8b54_refuse("click", buf, NULL, NULL);
    }
    tool = (node_num(jget(req, "click_count"), 1.0) == 2.0) ? "double_click" : "click";
    json_set(args, "button", json_string(button_norm));
    free(button_norm);

    if (present(req, "element")) {
        variant1 = json_object();
        json_set(variant1, "element_index", json_copy(jget(req, "element")));
    }
    if (present(req, "x") && present(req, "y")) {
        variant2 = json_object();
        json_set(variant2, "x", json_copy(jget(req, "x")));
        json_set(variant2, "y", json_copy(jget(req, "y")));
    }
    out = pointer_with(args, &st, wid, variant1, "element_index click", variant2, "coordinate click",
                       "click requires element= or x/y.", tool);
    json_free(variant1);
    json_free(variant2);
    if (out) {
        json_free(ta);
        return out;
    }
    mods = jget(req, "modifiers");
    if (mods && mods->type == JSON_ARRAY && json_len(mods) > 0)
        json_set(args, "modifier", json_copy(mods));

    out = run_ladder(tool, args, req_delivery(req), as_bool(req, "bring_to_front", 0), &st);
    json_free(ta);
    return out;
}

json_t *w19_cuabackend_3d8b54_drag(const json_t *req) {
    cua_state st = state_of(req);
    const json_t *wid = jget(req, "window_id");
    json_t *ta = w19_cuabackend_3d8b54_target_args("drag", jget(req, "pid"), wid, 0);
    const json_t *ref = refusal_of(ta);
    json_t *args;
    json_t *variant1 = NULL;
    json_t *variant2 = NULL;
    json_t *out;

    if (ref) {
        out = json_copy(ref);
        json_free(ta);
        return out;
    }
    args = json_obj_get(ta, "args");
    if (present(req, "from_element") && present(req, "to_element")) {
        variant1 = json_object();
        json_set(variant1, "from_element", json_copy(jget(req, "from_element")));
        json_set(variant1, "to_element", json_copy(jget(req, "to_element")));
    }
    if (present(req, "from_xy") && present(req, "to_xy")) {
        const json_t *f = jget(req, "from_xy");
        const json_t *t = jget(req, "to_xy");
        if (f->type == JSON_ARRAY && t->type == JSON_ARRAY && json_len(f) >= 2 && json_len(t) >= 2) {
            variant2 = json_object();
            json_set(variant2, "from_x", json_number((double)(long long)node_num(json_get(f, 0), 0.0)));
            json_set(variant2, "from_y", json_number((double)(long long)node_num(json_get(f, 1), 0.0)));
            json_set(variant2, "to_x", json_number((double)(long long)node_num(json_get(t, 0), 0.0)));
            json_set(variant2, "to_y", json_number((double)(long long)node_num(json_get(t, 1), 0.0)));
        }
    }
    out = pointer_with(args, &st, wid, variant1, "element-based drag", variant2, "coordinate drag",
                       "drag requires from_element/to_element or from_coordinate/to_coordinate.",
                       "drag");
    json_free(variant1);
    json_free(variant2);
    if (out) {
        json_free(ta);
        return out;
    }
    out = run_ladder("drag", args, req_delivery(req), as_bool(req, "bring_to_front", 0), &st);
    json_free(ta);
    return out;
}

json_t *w19_cuabackend_3d8b54_scroll(const json_t *req) {
    cua_state st = state_of(req);
    const json_t *wid = jget(req, "window_id");
    json_t *ta = w19_cuabackend_3d8b54_target_args("scroll", jget(req, "pid"), wid, 0);
    const json_t *ref = refusal_of(ta);
    json_t *args;
    json_t *variant1 = NULL;
    json_t *variant2 = NULL;
    json_t *out;
    double amount;

    if (ref) {
        out = json_copy(ref);
        json_free(ta);
        return out;
    }
    args = json_obj_get(ta, "args");
    amount = as_num(req, "amount", 3.0);
    if (amount < 1.0) amount = 1.0;
    if (amount > 50.0) amount = 50.0;
    json_set(args, "direction", json_string(as_str(req, "direction", "")));
    json_set(args, "amount", json_number(amount));

    if (present(req, "element") && st.has_window) {
        variant1 = json_object();
        json_set(variant1, "element_index", json_copy(jget(req, "element")));
    }
    if (present(req, "x") && present(req, "y")) {
        variant2 = json_object();
        if (st.supports_scroll_coords) {
            json_set(variant2, "x", json_copy(jget(req, "x")));
            json_set(variant2, "y", json_copy(jget(req, "y")));
        }
    }
    out = pointer_with(args, &st, wid, variant1, "element scroll", variant2, "coordinate scroll",
                       NULL, "scroll");
    json_free(variant1);
    json_free(variant2);
    if (out) {
        json_free(ta);
        return out;
    }
    out = run_ladder("scroll", args, req_delivery(req), as_bool(req, "bring_to_front", 0), &st);
    json_free(ta);
    return out;
}

json_t *w19_cuabackend_3d8b54_type_text(const json_t *req) {
    cua_state st = state_of(req);
    json_t *ta = w19_cuabackend_3d8b54_target_args("type_text", jget(req, "pid"), jget(req, "window_id"), 1);
    const json_t *ref = refusal_of(ta);
    json_t *args;
    json_t *out;

    if (ref) {
        out = json_copy(ref);
        json_free(ta);
        return out;
    }
    args = json_obj_get(ta, "args");
    json_set(args, "text", json_string(as_str(req, "text", "")));
    out = run_ladder("type_text", args, req_delivery(req), as_bool(req, "bring_to_front", 0), &st);
    json_free(ta);
    return out;
}

json_t *w19_cuabackend_3d8b54_key(const json_t *req) {
    cua_state st = state_of(req);
    const char *keys = as_str(req, "keys", "");
    json_t *ta = w19_cuabackend_3d8b54_target_args("key", jget(req, "pid"), jget(req, "window_id"), 1);
    const json_t *ref = refusal_of(ta);
    json_t *args;
    json_t *parsed;
    json_t *out;
    const json_t *key_node;
    const json_t *mods;

    if (ref) {
        out = json_copy(ref);
        json_free(ta);
        return out;
    }
    args = json_obj_get(ta, "args");
    parsed = w19_cuabackend_3d8b54_parse_key_combo(keys);
    key_node = jget(parsed, "key");
    if (!key_node || key_node->type != JSON_STRING || !key_node->str_val || !key_node->str_val[0]) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Could not parse key from '%s'.", keys);
        json_free(parsed);
        json_free(ta);
        return w19_cuabackend_3d8b54_refuse("key", buf, NULL, NULL);
    }
    mods = jget(parsed, "modifiers");
    if (mods && mods->type == JSON_ARRAY && json_len(mods) > 0) {
        json_t *hot = json_array();
        size_t i;
        for (i = 0; i < json_len(mods); i++) json_append(hot, json_copy(json_get(mods, i)));
        json_append(hot, json_copy(key_node));
        json_set(args, "keys", hot);
        out = run_ladder("hotkey", args, req_delivery(req), as_bool(req, "bring_to_front", 0), &st);
    } else {
        json_set(args, "key", json_copy(key_node));
        out = run_ladder("press_key", args, req_delivery(req), as_bool(req, "bring_to_front", 0), &st);
    }
    json_free(parsed);
    json_free(ta);
    return out;
}

json_t *w19_cuabackend_3d8b54_set_value(const json_t *req) {
    cua_state st = state_of(req);
    json_t *ta = w19_cuabackend_3d8b54_target_args("set_value", jget(req, "pid"), jget(req, "window_id"), 1);
    const json_t *ref = refusal_of(ta);
    json_t *args;
    json_t *out;

    if (ref) {
        out = json_copy(ref);
        json_free(ta);
        return out;
    }
    if (!present(req, "element")) {
        json_free(ta);
        return w19_cuabackend_3d8b54_refuse("set_value", "set_value requires element= (element index).",
                                             NULL, NULL);
    }
    args = json_obj_get(ta, "args");
    json_set(args, "element_index", json_copy(jget(req, "element")));
    json_set(args, "value", json_string(as_str(req, "value", "")));
    out = execute("set_value", args, &st);
    json_free(ta);
    return out;
}
