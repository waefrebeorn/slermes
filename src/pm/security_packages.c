/*
 * security_packages.c — faithful C11 port of the pure URL builders in
 * upstream Hermes pm/security_packages.py (gate w7_securitypac_7eac2a).
 *
 * PoP: w7_securitypac_7eac2a_secpack_bws_fetch_url @ pm/security_packages.py:Bws.fetch_url
 * PoP: w7_securitypac_7eac2a_secpack_iron_fetch_url @ pm/security_packages.py:IronProxy.fetch_url
 * PoP: w7_securitypac_7eac2a_secpack_iron_fetch_urls @ pm/security_packages.py:IronProxy.fetch_urls
 *
 * Only the deterministic string logic is ported: network/filesystem/GPG work
 * (stage/verify_provenance) is intentionally out of scope for a data oracle.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

#include "noc_w7_securitypac_7eac2a.h"

/* pm/packages.py:_RUST_TRIPLE */
static const char *rust_triple(const char *target) {
    if (!target) return NULL;
    if (!strcmp(target, "win32-x64")) return "x86_64-pc-windows-msvc";
    if (!strcmp(target, "win32-arm64")) return "aarch64-pc-windows-msvc";
    if (!strcmp(target, "linux-x64")) return "x86_64-unknown-linux-gnu";
    if (!strcmp(target, "linux-arm64")) return "aarch64-unknown-linux-gnu";
    if (!strcmp(target, "linux-x64-musl")) return "x86_64-unknown-linux-musl";
    if (!strcmp(target, "linux-arm64-musl")) return "aarch64-unknown-linux-musl";
    if (!strcmp(target, "darwin-x64")) return "x86_64-apple-darwin";
    if (!strcmp(target, "darwin-arm64")) return "aarch64-apple-darwin";
    return NULL;
}

/* Replace every occurrence of "linux-gnu" with "linux-musl" (str.replace). */
static void linux_gnu_to_musl(const char *src, char *dst, size_t cap) {
    size_t o = 0;
    const char *needle = "linux-gnu";
    size_t nlen = strlen(needle);
    while (*src && o + 1 < cap) {
        if (!strncmp(src, needle, nlen)) {
            const char *rep = "linux-musl";
            size_t rlen = strlen(rep);
            if (o + rlen >= cap) rlen = cap - o - 1;
            memcpy(dst + o, rep, rlen);
            o += rlen;
            src += nlen;
        } else {
            dst[o++] = *src++;
        }
    }
    dst[o] = '\0';
}

struct json_t *secpack_bws_fetch_url(const char *version, const char *target) {
    const char *triple = rust_triple(target);
    if (!triple || !version) return json_null();
    char platform_buf[128];
    if (target && !strncmp(target, "darwin", 6)) {
        snprintf(platform_buf, sizeof(platform_buf), "macos-universal");
    } else {
        linux_gnu_to_musl(triple, platform_buf, sizeof(platform_buf));
    }
    char out[1024];
    snprintf(out, sizeof(out),
             "https://github.com/bitwarden/sdk-sm/releases/download/bws-v%s/bws-%s-%s.zip",
             version, platform_buf, version);
    return json_string(out);
}

struct json_t *secpack_iron_fetch_url(const char *version, const char *target) {
    if (!version || !target) return json_null();
    /* pm/store.py:MUSL_TARGETS = {"linux-x64-musl", "linux-arm64-musl"} */
    int musl = !strcmp(target, "linux-x64-musl") || !strcmp(target, "linux-arm64-musl");
    char lookup[128];
    if (musl) {
        size_t len = strlen(target);
        if (len >= 5 && !strcmp(target + len - 5, "-musl")) len -= 5;
        if (len >= sizeof(lookup)) len = sizeof(lookup) - 1;
        memcpy(lookup, target, len);
        lookup[len] = '\0';
    } else {
        snprintf(lookup, sizeof(lookup), "%s", target);
    }
    /* lookup_target.split("-") -> exactly two parts */
    char *dash = strchr(lookup, '-');
    if (!dash) return json_null();
    *dash = '\0';
    const char *platform = lookup;
    const char *arch = dash + 1;
    if (strchr(arch, '-')) return json_null(); /* too many values to unpack */
    if (!strcmp(arch, "x64")) arch = "amd64";
    char out[1024];
    snprintf(out, sizeof(out),
             "https://github.com/paradigmxyz/iron-proxy/releases/download/v%s/iron-proxy_%s_%s_%s.tar.gz",
             version, version, platform, arch);
    return json_string(out);
}

struct json_t *secpack_iron_fetch_urls(const char *version, const char *target) {
    json_t *archive = secpack_iron_fetch_url(version, target);
    if (!archive || archive->type != JSON_STRING) {
        if (archive) json_free(archive);
        return json_null();
    }
    const char *url = archive->str_val;
    const char *slash = strrchr(url, '/');
    size_t base_len = slash ? (size_t)(slash - url) : 0;
    char base[1024];
    if (base_len >= sizeof(base)) base_len = sizeof(base) - 1;
    memcpy(base, url, base_len);
    base[base_len] = '\0';

    char u2[1200], u3[1200];
    snprintf(u2, sizeof(u2), "%s/checksums.txt", base);
    snprintf(u3, sizeof(u3), "%s/checksums.txt.asc", base);
    static const char *signing_key =
        "https://raw.githubusercontent.com/paradigmxyz/iron-proxy/"
        "be5f255d0d9d10d8573bd65f480dd48a07772bf1/public-key.asc";

    json_t *arr = json_array();
    json_append(arr, json_string(url));
    json_append(arr, json_string(u2));
    json_append(arr, json_string(u3));
    json_append(arr, json_string(signing_key));
    json_free(archive);
    return arr;
}
