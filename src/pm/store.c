/*
 * store.c — C port of upstream Hermes pm/store.py (gate w5_store_63be39).
 *
 * PoP: w5_store_63be39_entry_component @ pm/store.py:entry_component
 * PoP: w5_store_63be39_fetch_entry_name @ pm/store.py:fetch_entry_name
 * PoP: w5_store_63be39_current_target @ pm/store.py:current_target
 *
 * The hash/extract/publish machinery is IO-bound and stays in Python; the
 * three pure decision points above are mirrored exactly. For current_target
 * the three OS probes are injected by the gate so the port stays deterministic.
 */
#include "noc_w5_store_63be39.h"

#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s)
{
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* re.fullmatch(r"[a-f0-9]{64}", digest) */
static int is_sha256(const char *s)
{
    if (!s || strlen(s) != 64) return 0;
    for (int i = 0; i < 64; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'f') || (c >= '0' && c <= '9'))) return 0;
    }
    return 1;
}

/* PoP: w5_store_63be39_entry_component @ pm/store.py:entry_component */
int w5_store_63be39_entry_component(const char *name, char **out_name)
{
    if (!name || !out_name) return 0;
    *out_name = NULL;
    if (!*name || !strcmp(name, ".") || !strcmp(name, "..")) return 0;
    if (strchr(name, '/') || strchr(name, '\\')) return 0;
    /* Path(name).name != name for a Windows drive-relative name (C:foo). */
    if (((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z'))
            && name[1] == ':')
        return 0;
    *out_name = dupstr(name);
    return *out_name ? 1 : 0;
}

/* PoP: w5_store_63be39_fetch_entry_name @ pm/store.py:fetch_entry_name */
int w5_store_63be39_fetch_entry_name(const char *digest, char **out_name)
{
    if (!out_name) return 0;
    *out_name = NULL;
    if (!is_sha256(digest)) return 0;
    char *out = (char *)malloc(6 + 64 + 1);   /* "fetch-" + digest */
    if (!out) return 0;
    memcpy(out, "fetch-", 6);
    memcpy(out + 6, digest, 64);
    out[70] = '\0';
    *out_name = out;
    return 1;
}

/* PoP: w5_store_63be39_current_target @ pm/store.py:current_target */
char *w5_store_63be39_current_target(const char *machine, const char *sys_platform,
                                     int bionic, int musl)
{
    const char *arch;
    if (!machine) return NULL;
    if (!strcmp(machine, "arm64") || !strcmp(machine, "aarch64"))
        arch = "arm64";
    else if (!strcmp(machine, "x86_64") || !strcmp(machine, "amd64") || !strcmp(machine, "x64"))
        arch = "x64";
    else
        return NULL;    /* Python raises RuntimeError(unsupported architecture) */

    const char *sp = sys_platform ? sys_platform : "";
    char buf[64];
    if (!strncmp(sp, "win", 3))
        snprintf(buf, sizeof(buf), "win32-%s", arch);
    else if (!strcmp(sp, "darwin"))
        snprintf(buf, sizeof(buf), "darwin-%s", arch);
    else if (bionic)
        snprintf(buf, sizeof(buf), "linux-%s-bionic", arch);
    else if (musl)
        snprintf(buf, sizeof(buf), "linux-%s-musl", arch);
    else
        snprintf(buf, sizeof(buf), "linux-%s", arch);
    return dupstr(buf);
}

void w5_store_63be39_free(char *s) { free(s); }
