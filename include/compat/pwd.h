/* Slermes Windows compat: <pwd.h> — single-user model derived from env. */
#ifndef SLERMES_COMPAT_PWD_H
#define SLERMES_COMPAT_PWD_H
#include <stdlib.h>
#include <sys/types.h>

struct passwd {
    char  *pw_name;
    char  *pw_passwd;
    uid_t  pw_uid;
    gid_t  pw_gid;
    char  *pw_gecos;
    char  *pw_dir;
    char  *pw_shell;
};

static inline struct passwd *getpwuid(uid_t uid)
{
    static struct passwd pw;
    static char name[256];
    const char *u;
    (void)uid;
    u = getenv("USERNAME");
    if (!u) u = "user";
    snprintf(name, sizeof name, "%s", u);
    pw.pw_name = name;
    pw.pw_passwd = (char *)"";
    pw.pw_uid = 0;
    pw.pw_gid = 0;
    pw.pw_gecos = name;
    pw.pw_dir = getenv("USERPROFILE") ? getenv("USERPROFILE") : (char *)"C:\\";
    pw.pw_shell = (char *)"cmd.exe";
    return &pw;
}

static inline struct passwd *getpwnam(const char *n) { (void)n; return getpwuid(0); }
static inline struct passwd *getpwuid_r(uid_t uid, struct passwd *p, char *buf,
                                        size_t len, struct passwd **res)
{
    struct passwd *s = getpwuid(uid);
    (void)buf; (void)len;
    if (p && s) { *p = *s; }
    if (res) *res = p;
    return p;
}
#endif
