/* Slermes Windows compat: <dlfcn.h> over LoadLibrary/GetProcAddress. */
#ifndef SLERMES_COMPAT_DLFCN_H
#define SLERMES_COMPAT_DLFCN_H
#include <windows.h>
#include <stdio.h>
#define RTLD_LAZY   0
#define RTLD_NOW    0
#define RTLD_GLOBAL 0
#define RTLD_LOCAL  0
static inline void *dlopen(const char *file, int mode) { (void)mode; return (void *)LoadLibraryA(file); }
static inline void *dlsym(void *h, const char *sym) { return (void *)GetProcAddress((HMODULE)h, sym); }
static inline int dlclose(void *h) { return h ? (FreeLibrary((HMODULE)h) ? 0 : -1) : 0; }
static inline char *dlerror(void)
{
    static char buf[256];
    DWORD e = GetLastError();
    if (!e) return NULL;
    snprintf(buf, sizeof buf, "Windows error %lu", (unsigned long)e);
    SetLastError(0);
    return buf;
}
#endif
