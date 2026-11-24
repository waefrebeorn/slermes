/*
 * noc_w23_aio_785f45.h - C surface for upstream persistence/core/aio.py
 * (gate w23_aio_785f45).
 *
 * The awaitable form of every persistence verb: store.aio.<repository>.<verb>
 * runs the plain verb on the store's executor with the caller's context.  Two
 * kinds of verb have no awaitable form -- one returning a context manager, one
 * returning an iterator -- and asking for them raises AttributeError with the
 * reason the module names.
 *
 * Seams: this layer is pure reflection, so the harness supplies the facts
 * reflection would read -- attribute presence, callability, whether the
 * protocol declares the verb, and what the verb's return annotation resolves
 * to.  "returns" is one of:
 *   none | plain | iterable | iterator | context_manager
 *   | string_iterator | string_context_manager | string_unresolved
 * where string_* are the str annotations _refusal re-evals in the protocol
 * module's globals.
 */
#ifndef NOC_W23_AIO_785F45_H
#define NOC_W23_AIO_785F45_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _NO_AWAITABLE_FORM's reasons, sorted: the two refused declared returns. */
json_t *w23_aio_785f45_no_awaitable_form(void);

/* _refusal(protocol, name).
 * world: name, has_protocol, protocol_has, returns, annotation (the free name
 * for string_unresolved).  Returns {"reason": null|string} or
 * {"status":"error","error":"NameError","message":...}. */
json_t *w23_aio_785f45_refusal(const json_t *world);

/* AwaitableRepository.__getattr__.
 * world: name, repo_type, has_attr, callable, has_protocol, protocol_has,
 * returns, annotation.  Returns {"status":"awaitable"} or
 * {"status":"error","error":"AttributeError"|"NameError","message":...}. */
json_t *w23_aio_785f45_repository_attr(const json_t *world);

/* AwaitableStore.__getattr__.
 * world: name, store_type, has_attr.  Returns {"status":"repository"} or an
 * AttributeError; "_"-prefixed names never touch the store. */
json_t *w23_aio_785f45_store_attr(const json_t *world);

/* _complete: kind "generator" drains items into a tuple, anything else is
 * handed back untouched.  Returns {"kind":"tuple","items":[...]} or
 * {"kind":"value","value":...}. */
json_t *w23_aio_785f45_complete(const json_t *world);

#ifdef __cplusplus
}
#endif
#endif
