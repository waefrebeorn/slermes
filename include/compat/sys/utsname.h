/* Slermes Windows compat: <sys/utsname.h> — uname() from Win32 APIs. */
#ifndef SLERMES_COMPAT_SYS_UTSNAME_H
#define SLERMES_COMPAT_SYS_UTSNAME_H
#include <windows.h>
#include <stdio.h>

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

static inline int uname(struct utsname *u)
{
    DWORD sz = sizeof u->nodename;
    if (!u) return -1;
    snprintf(u->sysname, sizeof u->sysname, "Windows_NT");
    if (!GetComputerNameA(u->nodename, &sz)) snprintf(u->nodename, sizeof u->nodename, "localhost");
    snprintf(u->release, sizeof u->release, "%lu", (unsigned long)GetVersion());
    snprintf(u->version, sizeof u->version, "Windows");
    snprintf(u->machine, sizeof u->machine, "x86_64");
    return 0;
}
#endif
