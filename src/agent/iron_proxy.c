/*
 * port_iron_proxy.c — C port of agent/proxy_sources/iron_proxy.py
 * (path/version/asset helpers for the iron-proxy binary manager).
 * Self-contained; PoP-annotated per function.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

/* Cross-TU references (same module, separate files) */
extern char *ipx_read_persisted_nonce(const char *hermes_home);

#define IRON_PROXY_VERSION "0.39.0"

/* Module-global nonce set during start_proxy (mirrors Python _proxy_nonce).
 * NULL when no proxy was started in-process. */
char *ip_proxy_nonce = NULL;

#define IP_NONCE_ENV "HERMES_IRON_PROXY_NONCE"

static void ip_get_hermes_home(char *out, size_t outsz) {
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) snprintf(out, outsz, "%s", hh);
    else snprintf(out, outsz, "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
}

/* PoP: ip_proxy_state_dir_ro @ agent/proxy_sources/iron_proxy.py:_proxy_state_dir_ro */
char *ip_proxy_state_dir_ro(void) {
    char home[1024];
    ip_get_hermes_home(home, sizeof(home));
    char *out = malloc(strlen(home) + 8);
    sprintf(out, "%s/proxy", home);
    return out;
}

/* PoP: ip_proxy_state_dir @ agent/proxy_sources/iron_proxy.py:_proxy_state_dir */
char *ip_proxy_state_dir(void) {
    char *d = ip_proxy_state_dir_ro();
    mkdir(d, 0700);
    chmod(d, 0700); /* unconditional: tighten a slack umask */
    return d;
}

/* PoP: ip_platform_binary_name @ agent/proxy_sources/iron_proxy.py:_platform_binary_name */
const char *ip_platform_binary_name(void) {
#ifdef _WIN32
    return "iron-proxy.exe";
#else
    return "iron-proxy";
#endif
}

/* PoP: ip_platform_asset_name @ agent/proxy_sources/iron_proxy.py:_platform_asset_name */
char *ip_platform_asset_name(void) {
#ifdef __linux__
    const char *m = getenv("HOSTTYPE");
    const char *machine = m ? m : "x86_64";
    const char *arch = (strcmp(machine, "arm64") == 0 || strcmp(machine, "aarch64") == 0)
        ? "arm64" : "amd64";
    char *out = malloc(128);
    snprintf(out, 128, "iron-proxy_%s_linux_%s.tar.gz", IRON_PROXY_VERSION, arch);
    return out;
#elif defined(__APPLE__)
    const char *m = getenv("HOSTTYPE");
    const char *machine = m ? m : "x86_64";
    const char *arch = (strcmp(machine, "arm64") == 0 || strcmp(machine, "aarch64") == 0)
        ? "arm64" : "amd64";
    char *out = malloc(128);
    snprintf(out, 128, "iron-proxy_%s_darwin_%s.tar.gz", IRON_PROXY_VERSION, arch);
    return out;
#else
    return NULL; /* Windows: Python raises — no native binaries */
#endif
}

/* PoP: ip_expected_sha256 @ agent/proxy_sources/iron_proxy.py:_expected_sha256 */
char *ip_expected_sha256(const char *checksum_path, const char *asset_name) {
    /* Parse standard sha256sum output: "<hex>  <filename>". Returns the hex
     * for the entry whose filename matches asset_name, else NULL. */
    if (!checksum_path || !asset_name) return NULL;
    FILE *f = fopen(checksum_path, "r");
    if (!f) return NULL;
    char line[2048];
    char *found = NULL;
    while (fgets(line, sizeof(line), f)) {
        char *trim = line;
        while (*trim == ' ' || *trim == '\t') trim++;
        char *end = trim + strlen(trim);
        while (end > trim && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ')) *--end = '\0';
        if (!*trim) continue;
        char *sp = strchr(trim, ' ');
        if (!sp) continue;
        *sp = '\0';
        const char *name = sp + 1;
        while (*name == ' ') name++;
        if (strcmp(name, asset_name) == 0) { found = strdup(trim); break; }
    }
    fclose(f);
    return found;
}

/* PoP: ip_pick_tar_member @ agent/proxy_sources/iron_proxy.py:_pick_tar_member */
char *ip_pick_tar_member(const char *members, const char *binary_name) {
    /* Python scans a tar archive for the binary: regular files whose leaf
     * matches binary_name, no absolute paths, no "..". Arg = member list
     * (tab-separated); returns the first matching member (Python prefers
     * root-level candidates). */
    if (!members || !binary_name) return NULL;
    char *copy = strdup(members);
    char *save = NULL;
    char *root_match = NULL;
    char *dir_match = NULL;
    for (char *tok = strtok_r(copy, "\t", &save); tok; tok = strtok_r(NULL, "\t", &save)) {
        const char *m = tok;
        while (*m == ' ' || *m == '\t') m++;
        if (!*m) continue;
        if (m[0] == '/') continue;
        if (strstr(m, "..") != NULL) continue;
        const char *leaf = strrchr(m, '/');
        leaf = leaf ? leaf + 1 : m;
        if (strcmp(leaf, binary_name) != 0) continue;
        if (strchr(m, '/') == NULL) { root_match = strdup(m); break; }
        if (!dir_match) dir_match = strdup(m);
    }
    free(copy);
    if (root_match) { free(dir_match); return root_match; }
    return dir_match;
}

/* PoP: ip_iron_proxy_version @ agent/proxy_sources/iron_proxy.py:iron_proxy_version */
char *ip_iron_proxy_version(const char *binary) {
    /* Run "<binary> --version", strip whitespace; cached per binary path. */
    static char cache_path[1024];
    static char cache_val[256];
    static bool cached = false;
    if (!binary || !*binary) return NULL;
    if (cached && strcmp(cache_path, binary) == 0) return cache_val;
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "%s --version 2>/dev/null", binary);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char buf[256];
    if (!fgets(buf, sizeof(buf), fp)) { pclose(fp); return NULL; }
    pclose(fp);
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' ' || buf[n-1] == '\t'))
        buf[--n] = '\0';
    if (!n) return NULL;
    snprintf(cache_path, sizeof(cache_path), "%s", binary);
    snprintf(cache_val, sizeof(cache_val), "%s", buf);
    cached = true;
    return cache_val;
}

/* PoP: ip_management_token_path @ agent/proxy_sources/iron_proxy.py:_management_token_path */
char *ip_management_token_path(void) {
    char *d = ip_proxy_state_dir();
    char *out = malloc(strlen(d) + 20);
    sprintf(out, "%s/management.token", d);
    free(d);
    return out;
}

/* PoP: true @ agent/proxy_sources/iron_proxy.py:_pid_alive */
/* Return 1 iff pid is alive AND is an iron-proxy process.
 * Defends against PID reuse: /proc/<pid>/environ nonce match →
 * argv0 basename match → ps fallback → conservative true (exotic platforms). */
int ip_pid_alive(long pid) {
    if (pid <= 0) return 0;

    char hermes_home[1024];
    ip_get_hermes_home(hermes_home, sizeof(hermes_home));

    /* Signal 1: nonce env var in /proc/<pid>/environ (Linux). */
    char envpath[1024];
    snprintf(envpath, sizeof(envpath), "/proc/%ld/environ", pid);
    FILE *ef = fopen(envpath, "rb");
    if (ef) {
        char *buf = NULL; size_t cap = 0; size_t len = 0;
        int c;
        while ((c = fgetc(ef)) != EOF) {
            if (len + 1 >= cap) { cap = cap ? cap * 2 : 4096; buf = realloc(buf, cap); }
            if (buf) buf[len++] = (unsigned char)c;
        }
        fclose(ef);
        if (buf && len > 0) {
            char needle[64];
            snprintf(needle, sizeof(needle), "%s=", IP_NONCE_ENV);
            /* Build candidate nonces: ip_proxy_nonce (in-process), on-disk persisted. */
            char *on_disk = ipx_read_persisted_nonce(hermes_home);
            char *candidates[4] = {0};
            int n = 0;
            if (ip_proxy_nonce && *ip_proxy_nonce) candidates[n++] = ip_proxy_nonce;
            if (on_disk) candidates[n++] = on_disk;
            for (int i = 0; i < n; i++) {
                char full[320];
                snprintf(full, sizeof(full), "%s%s", needle, candidates[i]);
                size_t fl = strlen(full);
                if (len >= fl && memmem(buf, len, full, fl)) {
                    free(on_disk); free(buf); return 1;
                }
            }
            free(on_disk);
        }
        free(buf);
    }

    /* Signal 2: argv[0] basename starts with "iron-proxy" (Linux). */
    char cmdpath[1024];
    snprintf(cmdpath, sizeof(cmdpath), "/proc/%ld/cmdline", pid);
    FILE *cf = fopen(cmdpath, "rb");
    if (cf) {
        char *buf = NULL; size_t cap = 0; size_t len = 0;
        int c;
        while ((c = fgetc(cf)) != EOF) {
            if (len + 1 >= cap) { cap = cap ? cap * 2 : 4096; buf = realloc(buf, cap); }
            if (buf) buf[len++] = (unsigned char)c;
        }
        fclose(cf);
        if (buf && len > 0) {
            const char *argv0 = buf;
            const char *slash = strrchr(argv0, '/');
            const char *base = slash ? slash + 1 : argv0;
            int alive = (strncmp(base, "iron-proxy", 10) == 0);
            free(buf);
            if (alive) return 1;
        }
    }

    /* Signal 3: ps fallback (macOS / non-proc). */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ps -p %ld -o comm= 2>/dev/null", pid);
    FILE *pf = popen(cmd, "r");
    if (pf) {
        char comm[256];
        if (fgets(comm, sizeof(comm), pf)) {
            size_t n = strlen(comm);
            while (n > 0 && (comm[n-1] == '\n' || comm[n-1] == '\r' || comm[n-1] == ' '))
                comm[--n] = '\0';
            const char *slash = strrchr(comm, '/');
            const char *base = slash ? slash + 1 : comm;
            if (strncmp(base, "iron-proxy", 10) == 0) { pclose(pf); return 1; }
        }
        pclose(pf);
    }

    /* Exotic: be conservative — assume alive. */
    return 1;
}
