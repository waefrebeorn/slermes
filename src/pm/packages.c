/*
 * packages.c — C home for upstream Hermes pm/packages.py (gate w5_packages_6d8d5a).
 *
 * PoP: w5_packages_6d8d5a_dmgbuild_fetch_url  @ pm/packages.py:Dmgbuild.fetch_url
 * PoP: w5_packages_6d8d5a_nodejs_fetch_url    @ pm/packages.py:Nodejs.fetch_url
 * PoP: w5_packages_6d8d5a_git_fetch_url       @ pm/packages.py:Git.fetch_url
 * PoP: w5_packages_6d8d5a_gh_fetch_url        @ pm/packages.py:Gh.fetch_url
 * PoP: w5_packages_6d8d5a_uv_fetch_url        @ pm/packages.py:Uv.fetch_url
 * PoP: w5_packages_6d8d5a_ripgrep_fetch_url   @ pm/packages.py:Ripgrep.fetch_url
 * PoP: w5_packages_6d8d5a_chromium_store_entry @ pm/packages.py:Chromium.store_entry
 * PoP: w5_packages_6d8d5a_npm_env             @ pm/packages.py:npm_env
 *
 * Faithful ports of the deterministic artifact-URL builders and the child
 * environment sanitiser. Network/filesystem/process work is out of scope.
 */
#include "noc_w5_packages_6d8d5a.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W5PK_BUF 8192

static char *w5pk_dup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static char *w5pk_fmt(const char *fmt, ...) {
    char buf[W5PK_BUF];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return w5pk_dup("");
    return w5pk_dup(buf);
}
static int w5pk_startswith(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}
static int w5pk_endswith(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && strcmp(s + a - b, suf) == 0;
}
static int w5pk_musl(const char *target) {
    return !strcmp(target, "linux-x64-musl") || !strcmp(target, "linux-arm64-musl");
}
static const char *w5pk_node_plat(const char *t) {
    if (!strcmp(t, "win32-x64")) return "win-x64";
    if (!strcmp(t, "win32-arm64")) return "win-arm64";
    if (!strcmp(t, "linux-x64")) return "linux-x64";
    if (!strcmp(t, "linux-arm64")) return "linux-arm64";
    if (!strcmp(t, "linux-x64-musl")) return "linux-x64-musl";
    if (!strcmp(t, "linux-arm64-musl")) return "linux-arm64-musl";
    if (!strcmp(t, "darwin-x64")) return "darwin-x64";
    if (!strcmp(t, "darwin-arm64")) return "darwin-arm64";
    return "";
}
static const char *w5pk_rust_triple(const char *t) {
    if (!strcmp(t, "win32-x64")) return "x86_64-pc-windows-msvc";
    if (!strcmp(t, "win32-arm64")) return "aarch64-pc-windows-msvc";
    if (!strcmp(t, "linux-x64")) return "x86_64-unknown-linux-gnu";
    if (!strcmp(t, "linux-arm64")) return "aarch64-unknown-linux-gnu";
    if (!strcmp(t, "linux-x64-musl")) return "x86_64-unknown-linux-musl";
    if (!strcmp(t, "linux-arm64-musl")) return "aarch64-unknown-linux-musl";
    if (!strcmp(t, "darwin-x64")) return "x86_64-apple-darwin";
    if (!strcmp(t, "darwin-arm64")) return "aarch64-apple-darwin";
    return "";
}

char *w5_packages_6d8d5a_dmgbuild_fetch_url(const char *version, const char *target) {
    if (!version) version = "";
    if (!target) target = "";
    /* version.partition("+") -> release, sep, revision */
    const char *plus = strchr(version, '+');
    char release[W5PK_BUF], revision[W5PK_BUF];
    if (plus) {
        size_t rl = (size_t)(plus - version);
        if (rl >= sizeof(release)) rl = sizeof(release) - 1;
        memcpy(release, version, rl); release[rl] = '\0';
        snprintf(revision, sizeof(revision), "%s", plus + 1);
    } else {
        snprintf(release, sizeof(release), "%s", version);
        revision[0] = '\0';
    }
    const char *arch = !strcmp(target, "darwin-arm64") ? "arm64"
                     : !strcmp(target, "darwin-x64") ? "x86_64" : "";
    return w5pk_fmt("https://github.com/electron-userland/electron-builder-binaries/releases/download/"
                    "dmg-builder@%s/dmgbuild-bundle-%s-%s.tar.gz", release, arch, revision);
}

char *w5_packages_6d8d5a_nodejs_fetch_url(const char *version, const char *target) {
    if (!version) version = "";
    if (!target) target = "";
    if (!strcmp(target, "linux-arm64-bionic"))
        return w5pk_fmt("https://packages.termux.dev/apt/termux-main/pool/main/n/nodejs/"
                        "nodejs_%s-1_aarch64.deb", version);
    const char *plat = w5pk_node_plat(target);
    const char *ext = w5pk_startswith(target, "win32") ? "zip" : "tar.xz";
    const char *base = w5pk_musl(target)
        ? "https://unofficial-builds.nodejs.org/download/release"
        : "https://nodejs.org/dist";
    return w5pk_fmt("%s/v%s/node-v%s-%s.%s", base, version, version, plat, ext);
}

char *w5_packages_6d8d5a_git_fetch_url(const char *version, const char *target) {
    if (!version) version = "";
    if (!target) target = "";
    const char *plus = strchr(version, '+');
    char tag[W5PK_BUF], build[W5PK_BUF];
    if (plus) {
        size_t tl = (size_t)(plus - version);
        if (tl >= sizeof(tag)) tl = sizeof(tag) - 1;
        memcpy(tag, version, tl); tag[tl] = '\0';
        snprintf(build, sizeof(build), "%s", plus + 1);
    } else {
        snprintf(tag, sizeof(tag), "%s", version);
        build[0] = '\0';
    }
    const char *arch = w5pk_endswith(target, "arm64") ? "arm64" : "64-bit";
    const char *file_rev = !strcmp(build, "1") ? "" : build;
    if (!strcmp(build, "1")) file_rev = "";
    else file_rev = w5pk_fmt(".%s", build);
    char *out = w5pk_fmt("https://github.com/git-for-windows/git/releases/download/"
                         "v%s.windows.%s/PortableGit-%s%s-%s.7z.exe",
                         tag, build, tag, file_rev ? file_rev : "", arch);
    if (strcmp(build, "1") != 0) free((void *)file_rev);
    return out;
}

char *w5_packages_6d8d5a_gh_fetch_url(const char *version, const char *target) {
    if (!version) version = "";
    if (!target) target = "";
    char lookup[W5PK_BUF];
    if (w5pk_musl(target) && w5pk_endswith(target, "-musl")) {
        size_t n = strlen(target) - 5;
        memcpy(lookup, target, n); lookup[n] = '\0';
    } else snprintf(lookup, sizeof(lookup), "%s", target);
    /* lookup.split("-") -> osname, arch */
    const char *dash = strchr(lookup, '-');
    char osname[128], arch[128];
    if (dash) {
        size_t n = (size_t)(dash - lookup);
        if (n >= sizeof(osname)) n = sizeof(osname) - 1;
        memcpy(osname, lookup, n); osname[n] = '\0';
        snprintf(arch, sizeof(arch), "%s", dash + 1);
    } else {
        snprintf(osname, sizeof(osname), "%s", lookup);
        arch[0] = '\0';
    }
    const char *plat = !strcmp(osname, "win32") ? "windows"
                     : !strcmp(osname, "linux") ? "linux"
                     : !strcmp(osname, "darwin") ? "macOS" : "";
    const char *arch2 = !strcmp(arch, "x64") ? "amd64"
                      : !strcmp(arch, "arm64") ? "arm64" : "";
    const char *ext = (!strcmp(osname, "win32") || !strcmp(osname, "darwin")) ? "zip" : "tar.gz";
    return w5pk_fmt("https://github.com/cli/cli/releases/download/v%s/gh_%s_%s_%s.%s",
                    version, version, plat, arch2, ext);
}

char *w5_packages_6d8d5a_uv_fetch_url(const char *version, const char *target) {
    if (!version) version = "";
    if (!target) target = "";
    if (!strcmp(target, "linux-arm64-bionic"))
        return w5pk_fmt("https://packages.termux.dev/apt/termux-main/pool/main/u/uv/"
                        "uv_%s_aarch64.deb", version);
    const char *triple = w5pk_rust_triple(target);
    const char *ext = w5pk_startswith(target, "win32") ? "zip" : "tar.gz";
    return w5pk_fmt("https://github.com/astral-sh/uv/releases/download/%s/uv-%s.%s",
                    version, triple, ext);
}

char *w5_packages_6d8d5a_ripgrep_fetch_url(const char *version, const char *target) {
    if (!version) version = "";
    if (!target) target = "";
    const char *t = !strcmp(target, "linux-arm64-bionic") ? "linux-arm64" : target;
    const char *triple0 = w5pk_rust_triple(t);
    char triple[W5PK_BUF];
    const char *old = "-unknown-linux-gnu";
    const char *pos = strstr(triple0, old);
    if (pos) {
        size_t pre = (size_t)(pos - triple0);
        memcpy(triple, triple0, pre);
        snprintf(triple + pre, sizeof(triple) - pre, "-unknown-linux-musl%s", pos + strlen(old));
    } else snprintf(triple, sizeof(triple), "%s", triple0);
    const char *ext = w5pk_startswith(t, "win32") ? "zip" : "tar.gz";
    return w5pk_fmt("https://github.com/BurntSushi/ripgrep/releases/download/%s/"
                    "ripgrep-%s-%s.%s", version, version, triple, ext);
}

char *w5_packages_6d8d5a_chromium_store_entry(const char *version) {
    if (!version) version = "";
    const char *plus = strchr(version, '+');
    char rev[W5PK_BUF];
    if (plus) {
        size_t n = (size_t)(plus - version);
        if (n >= sizeof(rev)) n = sizeof(rev) - 1;
        memcpy(rev, version, n); rev[n] = '\0';
    } else snprintf(rev, sizeof(rev), "%s", version);
    return w5pk_fmt("chromium-%s", rev);
}

static int w5pk_ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}
static int w5pk_istartswith(const char *s, const char *p) {
    while (*p) {
        if (!*s || tolower((unsigned char)*s) != tolower((unsigned char)*p)) return 0;
        s++; p++;
    }
    return 1;
}

json_t *w5_packages_6d8d5a_npm_env(const char *cache_dir, const json_t *base_env) {
    json_t *out = json_object();
    if (base_env && base_env->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(base_env); i++) {
            const char *k = json_object_get_key_at(base_env, i);
            json_t *v = json_object_get_at(base_env, i);
            if (!k) continue;
            if (w5pk_istartswith(k, "npm_config_")) continue;
            if (w5pk_ieq(k, "NODE_OPTIONS") || w5pk_ieq(k, "NODE_PATH") || w5pk_ieq(k, "NODE_ENV")) continue;
            json_set(out, k, json_copy(v));
        }
    }
    json_set(out, "npm_config_cache", json_string(cache_dir ? cache_dir : ""));
    return out;
}

