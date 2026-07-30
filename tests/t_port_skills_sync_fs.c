/*
 * t_port_skills_sync_fs.c — oracle harness for skills_sync_fs helpers.
 * Includes the module .c directly (no duplicate symbol); links OpenSSL
 * (md5) + stdlib. Tests dir_hash (md5 vs live Python) and
 * safe_rel_install_path (traversal rejection) byte-for-byte.
 */
#include "skills_sync_fs.c"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void mkfile(const char *p, const char *c)
{
    FILE *f = fopen(p, "w");
    if (f) { fwrite(c, 1, strlen(c), f); fclose(f); }
}
static void mkpath(const char *p)
{
    char *dup = strdup(p);
    char *s = dup;
    while ((s = strchr(s, '/'))) {
        *s = '\0';
        mkdir(dup, 0755);
        *s = '/';
        s++;
    }
    free(dup);
    mkdir(p, 0755);
}

/* rotating JSON escaper (>=4 slots) */
#define JS_CAP 2048
static char g_js[4][JS_CAP];
static int  g_js_cur = 0;
static const char *js(const char *s)
{
    char *b = g_js[g_js_cur];
    g_js_cur = (g_js_cur + 1) % 4;
    if (!s) s = "";
    size_t j = 0;
    b[j++] = '"';
    for (const char *p = s; *p && j < JS_CAP - 4; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { b[j++] = '\\'; b[j++] = (char)c; }
        else if (c == '\n') { b[j++] = '\\'; b[j++] = 'n'; }
        else if (c == '\t') { b[j++] = '\\'; b[j++] = 't'; }
        else if (c < 0x20) {
            static const char *hx = "0123456789abcdef";
            b[j++] = '\\'; b[j++] = 'u'; b[j++] = '0'; b[j++] = '0';
            b[j++] = hx[(c>>4)&0xF]; b[j++] = hx[c&0xF];
        } else b[j++] = (char)c;
    }
    b[j++] = '"'; b[j] = '\0';
    return b;
}

int main(void)
{
    const char *root = "/tmp/ssfs_test";
    char rm[512]; snprintf(rm, sizeof(rm), "rm -rf %s", root); system(rm);
    mkpath(root);
    mkpath("/tmp/ssfs_test/sub");
    mkfile("/tmp/ssfs_test/a.txt", "hello");
    mkfile("/tmp/ssfs_test/sub/b.txt", "world");

    char *h = skills_sync_fs_dir_hash("/tmp/ssfs_test");
    printf("{\"fn\":\"skills_sync_fs_dir_hash\",\"in\":%s,\"out\":%s}\n",
           js("/tmp/ssfs_test"), js(h ? h : ""));
    free(h);

    /* safe_rel_install_path: valid relative */
    char *ok = skills_sync_fs_safe_rel_install_path("/base/foo/bar", "/base");
    printf("{\"fn\":\"safe_rel_ok\",\"in\":%s,\"out\":%s}\n",
           js("/base/foo/bar"), js(ok ? ok : "NULL"));
    free(ok);
    /* traversal rejected */
    char *bad = skills_sync_fs_safe_rel_install_path("/base/foo/../etc", "/base");
    printf("{\"fn\":\"safe_rel_traversal\",\"in\":%s,\"out\":%s}\n",
           js("/base/foo/../etc"), js(bad ? bad : "NULL"));
    free(bad);
    /* absolute rejected */
    char *abs = skills_sync_fs_safe_rel_install_path("/etc/passwd", "/base");
    printf("{\"fn\":\"safe_rel_absolute\",\"in\":%s,\"out\":%s}\n",
           js("/etc/passwd"), js(abs ? abs : "NULL"));
    free(abs);

    /* NOTE: do NOT rm the test dir here — the oracle reads it live. */
    return 0;
}
