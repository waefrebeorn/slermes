/*
 * lazy_forward.c — C port of upstream Hermes agent/lazy_forward.py.
 *
 * Python's forward() builds a closure whose only call-time-independent,
 * deterministic behaviour is the metadata it stamps on the forwarder:
 *   forwarder.__name__ = forwarder.__qualname__ = name
 *   forwarder.__doc__  = f"Forwarder \u2014 see ``{module}.{name}``."
 *   result = staticmethod(forwarder) when static=True else forwarder
 * The lazy import/attribute resolution (lazy_attr) is intentionally not
 * reproducible off the C side; the gate compares the stamped metadata.
 *
 * PoP: lazyfwd_forward @ agent/lazy_forward.py:forward
 * PoP: lazyfwd_forward_static @ agent/lazy_forward.py:forward_static
 * PoP: lazyfwd_doc @ agent/lazy_forward.py:forward
 */
#include "noc_lazyfwd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python: f"Forwarder \u2014 see ``{module}.{name}``."  (U+2014 = E2 80 94) */
char *lazyfwd_doc(const char *module, const char *name) {
    static const char pre[]  = "Forwarder \xE2\x80\x94 see ``";
    static const char post[] = "``.";
    const char *m = module ? module : "";
    const char *n = name ? name : "";
    size_t lm = strlen(m), ln = strlen(n);
    size_t len = sizeof(pre) - 1 + lm + 1 + ln + sizeof(post) - 1;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    size_t k = 0;
    memcpy(out + k, pre, sizeof(pre) - 1); k += sizeof(pre) - 1;
    memcpy(out + k, m, lm); k += lm;
    out[k++] = '.';
    memcpy(out + k, n, ln); k += ln;
    memcpy(out + k, post, sizeof(post) - 1); k += sizeof(post) - 1;
    out[k] = '\0';
    return out;
}

/* PoP: lazyfwd_forward @ agent/lazy_forward.py:forward */
void lazyfwd_forward(const char *module, const char *name, int static_flag,
                     lazyfwd_forwarder_t *out) {
    if (!out) return;
    out->name = strdup(name ? name : "");
    out->qualname = strdup(name ? name : "");
    out->doc = lazyfwd_doc(module, name);
    out->is_static = static_flag ? 1 : 0;
}

/* PoP: lazyfwd_forward_static @ agent/lazy_forward.py:forward_static
 * forward_static(module, name) -> forward(module, name, static=True) */
void lazyfwd_forward_static(const char *module, const char *name,
                            lazyfwd_forwarder_t *out) {
    lazyfwd_forward(module, name, 1, out);
}

void lazyfwd_forwarder_free(lazyfwd_forwarder_t *f) {
    if (!f) return;
    free(f->name);
    free(f->qualname);
    free(f->doc);
    f->name = f->qualname = f->doc = NULL;
    f->is_static = 0;
}
