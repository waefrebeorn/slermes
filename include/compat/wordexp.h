/* Slermes Windows compat: <wordexp.h> — no shell word expansion. */
#ifndef SLERMES_COMPAT_WORDEXP_H
#define SLERMES_COMPAT_WORDEXP_H
#include <stddef.h>
#define WRDE_NOSPACE 1
#define WRDE_BADCHAR 2
#define WRDE_BADVAL  3
#define WRDE_CMDSUB  4
#define WRDE_SYNTAX  5
#define WRDE_NOCMD   (1 << 2)
#define WRDE_SHOWERR (1 << 6)
#define WRDE_UNDEF   (1 << 7)
typedef struct { size_t we_wordc; char **we_wordv; size_t we_offs; } wordexp_t;
static inline int wordexp(const char *words, wordexp_t *p, int flags)
{ (void)words; (void)p; (void)flags; return WRDE_NOSPACE; }
static inline void wordfree(wordexp_t *p) { (void)p; }
#endif
