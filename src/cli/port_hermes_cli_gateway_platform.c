/*
 * port_hermes_cli_gateway_platform.c — C port of platform/environment
 * helpers from hermes_cli/gateway.py.
 *
 * Only dependency-light, faithful re-implementations are ported here.
 * Functions that shell out to systemd/launchd are deferred.
 */

#include "hermes_logger.h"
#include "libcrypto/crypto.h"
#include "hermes_util_str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/utsname.h>

/* PoP: is_linux @ hermes_cli/gateway.py:is_linux */
int is_linux(void)
{
    struct utsname u;
    if (uname(&u) != 0) return 0;
    return strncmp(u.sysname, "Linux", 5) == 0;
}

/* PoP: is_macos @ hermes_cli/gateway.py:is_macos */
int is_macos(void)
{
    struct utsname u;
    if (uname(&u) != 0) return 0;
    return strcmp(u.sysname, "Darwin") == 0;
}

/* PoP: is_windows @ hermes_cli/uninstall.py:_is_windows */
/* PoP: is_windows @ hermes_cli/stdio.py:is_windows */
/* PoP: is_windows @ hermes_cli/main.py:_is_windows */
/* PoP: is_windows @ hermes_cli/gateway.py:is_windows */
int is_windows(void)
{
    struct utsname u;
    if (uname(&u) != 0) return 0;
    return strcmp(u.sysname, "Windows") == 0;
}

/* Valid profile name: ^[a-z0-9][a-z0-9_-]{0,63}$ */
static int valid_profile_name(const char *s)
{
    if (!s || !s[0]) return 0;
    if (!(s[0] >= 'a' && s[0] <= 'z') && !(s[0] >= '0' && s[0] <= '9')) return 0;
    for (const char *p = s + 1; *p; p++)
        if (!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-')) return 0;
    return 1;
}

/* PoP: _profile_suffix @ hermes_cli/gateway.py:_profile_suffix */
/* Returns malloc'd suffix ("" for default root, profile name, or short
 * hash for arbitrary paths). Caller frees. */
/* PoP: gateway_profile_suffix @ hermes_cli/gateway.py:_profile_suffix */
char *gateway_profile_suffix(void)
{
    char home[PATH_MAX], def[PATH_MAX];
    hermes_home_dir(home, sizeof(home));
    hermes_home_dir(def, sizeof(def));
    if (strcmp(home, def) == 0) return strdup("");

    /* profiles_root = default/profiles */
    char profiles_root[PATH_MAX];
    snprintf(profiles_root, sizeof(profiles_root), "%s/profiles", def);
    /* Check if home is directly under profiles_root */
    size_t plen = strlen(profiles_root);
    if (strncmp(home, profiles_root, plen) == 0 && home[plen] == '/' &&
        strchr(home + plen + 1, '/') == NULL) {
        const char *name = home + plen + 1;
        if (valid_profile_name(name)) return strdup(name);
    }
    /* Fallback: short sha256 of home */
    unsigned char hash[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)home, strlen(home), hash);
    char *hex = malloc(9);
    static const char *hx = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        hex[i] = hx[hash[i] >> 4];
        hex[i+1] = hx[hash[i] & 0xf];
    }
    hex[8] = '\0';
    return hex;
}

/* PoP: _profile_arg @ hermes_cli/gateway.py:_profile_arg */
/* Returns malloc'd "--profile <name>" for named profiles, "" otherwise. */
/* PoP: gateway_profile_arg @ hermes_cli/gateway.py:_profile_arg */
char *gateway_profile_arg(const char *hermes_home)
{
    char home[PATH_MAX], def[PATH_MAX];
    if (hermes_home && hermes_home[0]) snprintf(home, sizeof(home), "%s", hermes_home);
    else hermes_home_dir(home, sizeof(home));
    hermes_home_dir(def, sizeof(def));
    if (strcmp(home, def) == 0) return strdup("");

    char profiles_root[PATH_MAX];
    snprintf(profiles_root, sizeof(profiles_root), "%s/profiles", def);
    size_t plen = strlen(profiles_root);
    if (strncmp(home, profiles_root, plen) == 0 && home[plen] == '/' &&
        strchr(home + plen + 1, '/') == NULL) {
        const char *name = home + plen + 1;
        if (valid_profile_name(name)) {
            char *r = malloc(strlen(name) + 11);
            sprintf(r, "--profile %s", name);
            return r;
        }
    }
    return strdup("");
}
