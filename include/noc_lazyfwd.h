#ifndef NOC_LAZYFWD_H
#define NOC_LAZYFWD_H
#include <stddef.h>

/* Observable shape of the forwarder built by agent/lazy_forward.forward(). */
typedef struct {
    char *name;       /* forwarder.__name__ */
    char *qualname;   /* forwarder.__qualname__ */
    char *doc;        /* forwarder.__doc__ */
    int  is_static;   /* isinstance(result, staticmethod) */
} lazyfwd_forwarder_t;

/* PoP: lazyfwd_forward @ agent/lazy_forward.py:forward */
void lazyfwd_forward(const char *module, const char *name, int static_flag,
                     lazyfwd_forwarder_t *out);
/* PoP: lazyfwd_forward_static @ agent/lazy_forward.py:forward_static */
void lazyfwd_forward_static(const char *module, const char *name,
                            lazyfwd_forwarder_t *out);
/* PoP: lazyfwd_doc @ agent/lazy_forward.py:forward -- the f-string doc */
char *lazyfwd_doc(const char *module, const char *name); /* malloc'd */
void lazyfwd_forwarder_free(lazyfwd_forwarder_t *f);

#endif /* NOC_LAZYFWD_H */
