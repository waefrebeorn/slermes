/*
 * aio.c - C11 port of upstream persistence/core/aio.py (gate w23_aio_785f45).
 *
 * PoP: w23_aio_785f45_no_awaitable_form @ persistence/core/aio.py:_NO_AWAITABLE_FORM
 * PoP: w23_aio_785f45_refusal @ persistence/core/aio.py:_refusal
 * PoP: w23_aio_785f45_repository_attr @ persistence/core/aio.py:AwaitableRepository.__getattr__
 * PoP: w23_aio_785f45_store_attr @ persistence/core/aio.py:AwaitableStore.__getattr__
 * PoP: w23_aio_785f45_complete @ persistence/core/aio.py:_complete
 *
 * The module is a reflection layer over Python objects, so its seams are the
 * facts reflection reads: does the repository/store carry the attribute, is it
 * callable, does the protocol declare the verb, and what does the verb's
 * return ANNOTATION resolve to (a type, a generic alias whose origin is one of
 * the two refused classes, or a string annotation that eval() cannot resolve).
 * Every decision and every message below is the module's own.
 */
#include <stdlib.h>
#include <string.h>
#include "noc_w23_aio_785f45.h"

static json_t *aio_error(const char *kind, const char *message)
{
    json_t *o = json_object();
    json_set(o, "status", json_string("error"));
    json_set(o, "error", json_string(kind));
    json_set(o, "message", json_string(message ? message : ""));
    return o;
}

/* f"'{type} object has no attribute '{name}'" -- what getattr raises. */
static json_t *aio_lookup_error(const char *type_name, const char *name)
{
    static const char *mid = "' object has no attribute '";
    const char *t = type_name ? type_name : "object";
    const char *n = name ? name : "";
    size_t la = strlen(t) + 1, lm = strlen(mid), ln = strlen(n);
    char *msg = (char *)malloc(la + lm + ln + 2);
    json_t *out;
    if (!msg) return NULL;
    memcpy(msg, "'", 1);
    memcpy(msg + 1, t, la - 1);
    memcpy(msg + la, mid, lm);
    memcpy(msg + la + lm, n, ln);
    msg[la + lm + ln] = '\'';
    msg[la + lm + ln + 1] = '\0';
    out = aio_error("AttributeError", msg);
    free(msg);
    return out;
}

/* _NO_AWAITABLE_FORM: the annotation origin -> the reason the refusal names. */
static const char *aio_refusal_reason(const char *returns)
{
    if (!returns) return NULL;
    if (!strcmp(returns, "context_manager") || !strcmp(returns, "string_context_manager"))
        return "returns a context manager";
    if (!strcmp(returns, "iterator") || !strcmp(returns, "string_iterator"))
        return "streams its rows";
    return NULL;   /* a plain class, a non-refused alias, or no annotation */
}

/* `if isinstance(returns, str): returns = eval(returns, verb.__globals__)` --
 * a string annotation naming nothing in the protocol's module raises. */
static int aio_annotation_unresolved(const char *returns)
{
    return returns && !strcmp(returns, "string_unresolved");
}

json_t *w23_aio_785f45_no_awaitable_form(void)
{
    json_t *arr = json_array();
    json_append(arr, json_string("returns a context manager"));
    json_append(arr, json_string("streams its rows"));
    return arr;
}

/* _refusal(protocol, name): why the verb has no awaitable form, or None. */
json_t *w23_aio_785f45_refusal(const json_t *world)
{
    const char *returns = json_get_str(world, "returns", "none");
    const char *name = json_get_str(world, "name", "");
    const char *reason;
    json_t *out;

    if (!json_get_bool(world, "has_protocol", true)) {
        out = json_object();
        json_set(out, "reason", json_null());
        return out;
    }
    /* getattr(protocol, name, None) that is not callable -> no refusal */
    if (!json_get_bool(world, "protocol_has", true)) {
        out = json_object();
        json_set(out, "reason", json_null());
        return out;
    }
    if (aio_annotation_unresolved(returns)) {
        const char *free_name = json_get_str(world, "annotation", "Zed");
        static const char *pre = "name '";
        static const char *post = "' is not defined";
        char *msg = (char *)malloc(strlen(pre) + strlen(free_name) + strlen(post) + 1);
        if (!msg) return NULL;
        sprintf(msg, "%s%s%s", pre, free_name, post);
        out = aio_error("NameError", msg);
        free(msg);
        return out;
    }
    reason = aio_refusal_reason(returns);
    out = json_object();
    if (!reason) {
        json_set(out, "reason", json_null());
    } else {
        json_set(out, "reason", json_string(reason));
    }
    (void)name;
    return out;
}

/* AwaitableRepository.__getattr__ */
json_t *w23_aio_785f45_repository_attr(const json_t *world)
{
    const char *name = json_get_str(world, "name", "");
    const char *repo_type = json_get_str(world, "repo_type", "Repo");
    const char *returns = json_get_str(world, "returns", "none");
    const char *reason;
    json_t *out;

    /* verb = getattr(self._repository, name) */
    if (!json_get_bool(world, "has_attr", true))
        return aio_lookup_error(repo_type, name);

    /* if name.startswith("_") or not callable(verb): raise AttributeError(name) */
    if ((name && name[0] == '_') || !json_get_bool(world, "callable", true))
        return aio_error("AttributeError", name);

    /* if self._protocol is not None and (reason := _refusal(...)) */
    if (json_get_bool(world, "has_protocol", true) && json_get_bool(world, "protocol_has", true)) {
        if (aio_annotation_unresolved(returns)) {
            const char *free_name = json_get_str(world, "annotation", "Zed");
            static const char *pre = "name '";
            static const char *post = "' is not defined";
            char *msg = (char *)malloc(strlen(pre) + strlen(free_name) + strlen(post) + 1);
            if (!msg) return NULL;
            sprintf(msg, "%s%s%s", pre, free_name, post);
            out = aio_error("NameError", msg);
            free(msg);
            return out;
        }
        reason = aio_refusal_reason(returns);
        if (reason) {
            static const char *tail = " and has no awaitable form: run the plain verb off the event loop";
            char *msg = (char *)malloc(strlen(name) + 1 + strlen(reason) + strlen(tail) + 1);
            if (!msg) return NULL;
            strcpy(msg, name);
            strcat(msg, " ");
            strcat(msg, reason);
            strcat(msg, tail);
            out = aio_error("AttributeError", msg);
            free(msg);
            return out;
        }
    }
    out = json_object();
    json_set(out, "status", json_string("awaitable"));
    return out;
}

/* AwaitableStore.__getattr__ */
json_t *w23_aio_785f45_store_attr(const json_t *world)
{
    const char *name = json_get_str(world, "name", "");
    const char *store_type = json_get_str(world, "store_type", "Store");
    json_t *out;

    if (name && name[0] == '_') return aio_error("AttributeError", name);
    if (!json_get_bool(world, "has_attr", true))
        return aio_lookup_error(store_type, name);
    out = json_object();
    json_set(out, "status", json_string("repository"));
    return out;
}

/* _complete: a generator is drained here, anything else passes through. */
json_t *w23_aio_785f45_complete(const json_t *world)
{
    const char *kind = json_get_str(world, "kind", "value");
    const json_t *items = json_obj_get(world, "items");
    const json_t *value = json_obj_get(world, "value");
    json_t *out = json_object();
    if (!out) return NULL;
    if (!strcmp(kind, "generator")) {
        json_t *arr = json_array();
        size_t i, n = json_len(items);
        for (i = 0; i < n; i++) json_append(arr, json_copy(json_get(items, i)));
        json_set(out, "kind", json_string("tuple"));
        json_set(out, "items", arr);
    } else {
        json_set(out, "kind", json_string("value"));
        json_set(out, "value", value ? json_copy(value) : json_null());
    }
    return out;
}
