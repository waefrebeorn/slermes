#ifndef NOC_W5_INIT_703DD3_H
#define NOC_W5_INIT_703DD3_H
#include <stddef.h>

/* PoP: w5init_all @ pm/__init__.py:__all__ (list(_HOME), insertion order). */
const char *const *w5init_all(size_t *outn);
/* PoP: w5init_home @ pm/__init__.py:_HOME.get(name): module string or NULL. */
const char *w5init_home(const char *name);
/* PoP: w5init_has @ pm/__init__.py:name in _HOME. */
int w5init_has(const char *name);

#endif /* NOC_W5_INIT_703DD3_H */
