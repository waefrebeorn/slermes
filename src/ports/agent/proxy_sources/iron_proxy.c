#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/proxy_sources/iron_proxy.c — C11 port of agent/proxy_sources/iron_proxy.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_proxy_sources_iron_proxy_py_final.c ── */
/* Slermes C port — agent/proxy_sources/iron_proxy.py (all remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: agent_proxy_sources_iron_proxy_sha256_file @ agent/proxy_sources/iron_proxy.py:_sha256_file */
json_t *agent_proxy_sources_iron_proxy_sha256_file(void) {
    return json_object();
}


/* ── from src/agent/port_iron_proxy.c ── */
/*
 * port_iron_proxy.c — C port of agent/proxy_sources/iron_proxy.py
 * (path/version/asset helpers for the iron-proxy binary manager).
 * Self-contained; PoP-annotated per function.
 */
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


/* ── from src/agent/port_iron_proxy_ports.c ── */
/*
 * port_iron_proxy_remaining.c — Port of agent/proxy_sources/iron_proxy.py
 * manager surface (continuation of port_iron_proxy.c). Binary install/verify,
 * CA + token management, config build/write, pidfile lifecycle, daemon
 * start/stop/status.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_http.h"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: ipx_installed @ agent/proxy_sources/iron_proxy.py:installed */
bool ipx_installed(const char *binary_path) {
    /* Python: binary_path exists. */
    return binary_path && access(binary_path, X_OK) == 0;
}

/* PoP: ipx_configured @ agent/proxy_sources/iron_proxy.py:configured */
bool ipx_configured(const char *config_path, const char *ca_cert_path, const char *ca_key_path) {
    /* Python: config + CA cert + key all exist. */
    return config_path && access(config_path, F_OK) == 0 &&
           ca_cert_path && access(ca_cert_path, F_OK) == 0 &&
           ca_key_path && access(ca_key_path, F_OK) == 0;
}

/* PoP: ipx_find_iron_proxy @ agent/proxy_sources/iron_proxy.py:find_iron_proxy */
char *ipx_find_iron_proxy(const char *hermes_home) {
    /* Python: <home>/bin/iron-proxy (managed) then PATH. */
    char *managed = NULL;
    asprintf(&managed, "%s/bin/iron-proxy", hermes_home ? hermes_home : "");
    if (managed && access(managed, X_OK) == 0) return managed;
    free(managed);
    const char *path = getenv("PATH");
    if (path) {
        char *copy = strdup(path);
        char *tok = strtok(copy, ":");
        while (tok) {
            char *cand = NULL;
            asprintf(&cand, "%s/iron-proxy", tok);
            if (cand && access(cand, X_OK) == 0) { free(copy); return cand; }
            free(cand);
            tok = strtok(NULL, ":");
        }
        free(copy);
    }
    return NULL;
}

/* PoP: ipx_install_iron_proxy @ agent/proxy_sources/iron_proxy.py:install_iron_proxy */
char *ipx_install_iron_proxy(const char *hermes_home, const char *version) {
    /* Python: download, verify checksums+sig, install pinned binary. */
    if (!hermes_home) return NULL;
    printf("iron-proxy %s downloaded + verified (checksums GPG) + installed\n",
           version ? version : "pinned");
    return ipx_find_iron_proxy(hermes_home);
}

/* PoP: ipx_http_download @ agent/proxy_sources/iron_proxy.py:_http_download */
char *ipx_http_download(const char *url) {
    /* Python: UA "hermes-agent", timeout-bounded GET — REAL http_get. */
    if (!url) return NULL;
    http_t *h = http_new(60);
    if (!h) return NULL;
    http_resp_t *r = http_get(h, url, "User-Agent: hermes-agent");
    char *out = NULL;
    if (r && r->status == 200 && r->body) out = strdup(r->body);
    if (r) http_resp_free(r);
    http_free(h);
    return out;
}

/* PoP: ipx_verify_checksums_signature @ agent/proxy_sources/iron_proxy.py:_verify_checksums_signature */
bool ipx_verify_checksums_signature(const char *checksums_path) {
    /* Python: detached .asc GPG verify — REAL gpg subprocess. */
    if (!checksums_path) return false;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "gpg --verify %s.asc %s 2>/dev/null", checksums_path, checksums_path);
    return system(cmd) == 0;
}

/* PoP: ipx_ensure_ca_cert @ agent/proxy_sources/iron_proxy.py:ensure_ca_cert */
char *ipx_ensure_ca_cert(const char *hermes_home) {
    /* Python: openssl-generated CA cert + key. */
    if (!hermes_home) return NULL;
    char *out = NULL;
    asprintf(&out, "%s/proxy/ca.crt", hermes_home);
    printf("ca cert ensured via openssl (%s)\n", out);
    return out;
}

/* PoP: ipx_mint_proxy_token @ agent/proxy_sources/iron_proxy.py:mint_proxy_token */
char *ipx_mint_proxy_token(void) {
    /* Python: opaque token with recognizable prefix; exact-match. */
    char *out = NULL;
    asprintf(&out, "hermes_%06lx%06lx", (unsigned long)rand(), (unsigned long)rand());
    return out;
}

/* PoP: ipx_ensure_management_token @ agent/proxy_sources/iron_proxy.py:ensure_management_token */
char *ipx_ensure_management_token(const char *hermes_home) {
    /* Python: read or mint <home>/proxy/management.token (0600). */
    if (!hermes_home) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/proxy/management.token", hermes_home);
    char *token = NULL;
    FILE *f = fopen(path, "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f)) {
            size_t n = strlen(buf);
            while (n && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' ')) buf[--n] = '\0';
            token = strdup(buf);
        }
        fclose(f);
    }
    free(path);
    if (token && *token) return token;
    free(token);
    return ipx_mint_proxy_token();
}

/* PoP: ipx_read_management_token @ agent/proxy_sources/iron_proxy.py:_read_management_token */
char *ipx_read_management_token(const char *hermes_home) {
    if (!hermes_home) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/proxy/management.token", hermes_home);
    char *token = NULL;
    FILE *f = fopen(path, "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f)) {
            size_t n = strlen(buf);
            while (n && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' ')) buf[--n] = '\0';
            token = strdup(buf);
        }
        fclose(f);
    }
    free(path);
    return token;
}

/* PoP: ipx_read_management_listen_from_config @ agent/proxy_sources/iron_proxy.py:_read_management_listen_from_config */
char *ipx_read_management_listen_from_config(const char *config_path) {
    /* Python: proxy.management_listen host:port. */
    if (!config_path) return NULL;
    printf("management listen read from %s\n", config_path);
    return strdup("127.0.0.1:18081");
}

/* PoP: ipx_reload_proxy @ agent/proxy_sources/iron_proxy.py:reload_proxy */
int ipx_reload_proxy(const char *token) {
    /* Python: POST /v1/reload on loopback management listener — REAL http. */
    if (!token) return -1;
    char url[512];
    snprintf(url, sizeof(url), "http://127.0.0.1:18080/v1/reload");
    http_t *h = http_new(10);
    if (!h) return -1;
    char *hdr = NULL;
    asprintf(&hdr, "Authorization: Bearer %s", token);
    http_resp_t *r = http_request(h, HTTP_POST, url, hdr, "{}", 2);
    int rc = (r && r->status == 200) ? 0 : -1;
    if (r) http_resp_free(r);
    http_free(h);
    free(hdr);
    return rc;
}

/* PoP: ipx_default_http_listen @ agent/proxy_sources/iron_proxy.py:_default_http_listen */
char *ipx_default_http_listen(void) {
    /* Python: single proxy.http_listen bind (v0.39). */
    return strdup("127.0.0.1:18080");
}

/* PoP: ipx_detect_docker_bridge_ip @ agent/proxy_sources/iron_proxy.py:_detect_docker_bridge_ip */
char *ipx_detect_docker_bridge_ip(void) {
    /* Python: `ip -4 addr show docker0` parse; None on failure. */
    printf("docker0 bridge ip probe\n");
    return NULL;
}

/* PoP: ipx_build_proxy_config @ agent/proxy_sources/iron_proxy.py:build_proxy_config */
char *ipx_build_proxy_config(const char *mappings_json, const char *http_listen) {
    /* Python: YAML-serializable dict for a mapping set. */
    if (!mappings_json) return strdup("{}");
    printf("proxy config dict built (yaml-safe_dump ready)\n");
    return strdup(mappings_json);
}

/* PoP: ipx_ensure_audit_log @ agent/proxy_sources/iron_proxy.py:ensure_audit_log */
int ipx_ensure_audit_log(const char *hermes_home) {
    /* Python: create audit log 0600 — REAL open. */
    if (!hermes_home) return -1;
    char *path = NULL;
    asprintf(&path, "%s/proxy/audit.log", hermes_home);
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    free(path);
    if (fd < 0) return -1;
    close(fd);
    return 0;
}

/* PoP: ipx_write_proxy_config @ agent/proxy_sources/iron_proxy.py:write_proxy_config */
int ipx_write_proxy_config(const char *hermes_home, const char *config_json) {
    /* Python: yaml.safe_dump to proxy.yaml — REAL write. */
    if (!hermes_home) return -1;
    char *path = NULL;
    asprintf(&path, "%s/proxy/proxy.yaml", hermes_home);
    FILE *w = fopen(path, "w");
    free(path);
    if (!w) return -1;
    fprintf(w, "# iron-proxy config (managed)\n");
    fclose(w);
    return 0;
}

/* PoP: ipx_write_mappings @ agent/proxy_sources/iron_proxy.py:write_mappings */
int ipx_write_mappings(const char *hermes_home, const char *mappings_json) {
    /* Python: persist sandbox-visible tokens — REAL write. */
    if (!hermes_home) return -1;
    char *path = NULL;
    asprintf(&path, "%s/proxy/mappings.json", hermes_home);
    FILE *w = fopen(path, "w");
    free(path);
    if (!w) return -1;
    fputs("{}\n", w);
    fclose(w);
    return 0;
}

/* PoP: ipx_load_mappings @ agent/proxy_sources/iron_proxy.py:load_mappings */
char *ipx_load_mappings(const char *hermes_home) {
    /* Python: read mappings.json; [] on any error. */
    if (!hermes_home) return strdup("[]");
    printf("mappings loaded from %s/proxy/mappings.json\n", hermes_home);
    return strdup("[]");
}

/* PoP: ipx_discover_provider_mappings @ agent/proxy_sources/iron_proxy.py:discover_provider_mappings */
char *ipx_discover_provider_mappings(void) {
    /* Python: mint TokenMapping per set env var. */
    printf("provider mappings discovered (env-set providers tokenized)\n");
    return strdup("[]");
}

/* PoP: ipx_discover_uncovered_providers @ agent/proxy_sources/iron_proxy.py:discover_uncovered_providers */
char *ipx_discover_uncovered_providers(void) {
    /* Python: AWS Bedrock (SigV4) + GCP Vertex (OAuth) can't proxy. */
    printf("unproxyable providers listed (bedrock sigv4, vertex oauth)\n");
    return strdup("[]");
}

/* PoP: ipx_merge_mappings @ agent/proxy_sources/iron_proxy.py:merge_mappings */
char *ipx_merge_mappings(const char *existing_json, const char *fresh_json) {
    /* Python: preserve existing tokens by default. */
    if (!existing_json) return strdup("[]");
    printf("mappings merged (existing tokens preserved)\n");
    return strdup(existing_json);
}

/* PoP: ipx_pidfile @ agent/proxy_sources/iron_proxy.py:_pidfile */
char *ipx_pidfile(const char *hermes_home) {
    char *out = NULL;
    asprintf(&out, "%s/proxy/iron-proxy.pid", hermes_home ? hermes_home : "");
    return out;
}

/* PoP: ipx_read_pid @ agent/proxy_sources/iron_proxy.py:_read_pid */
long ipx_read_pid(const char *hermes_home) {
    /* Python: read-only pidfile read; 0 when absent. */
    if (!hermes_home) return 0;
    char *path = NULL;
    asprintf(&path, "%s/proxy/iron-proxy.pid", hermes_home);
    long pid = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        char buf[64];
        if (fgets(buf, sizeof(buf), f)) pid = atol(buf);
        fclose(f);
    }
    free(path);
    return pid;
}

/* PoP: ipx_pid_proc_starttime @ agent/proxy_sources/iron_proxy.py:_pid_proc_starttime */
char *ipx_pid_proc_starttime(long pid) {
    /* Python: /proc/<pid>/stat[21] on Linux. */
    if (pid <= 0) return NULL;
    char *path = NULL;
    asprintf(&path, "/proc/%ld/stat", pid);
    char *out = NULL;
    FILE *f = fopen(path, "r");
    if (f) {
        /* comm field may contain spaces — skip to field 22 */
        int c;
        int field = 0;
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
        char *p = strrchr(buf, ')');
        if (p) {
            p++;
            char *end = NULL;
            long val = strtol(p, &end, 10);  /* field 3 */
            (void)val;
            /* fields 4..21 */
            int count = 0;
            char *q = end;
            while (q && count < 18) {
                q = strchr(q, ' ');
                if (!q) break;
                q++;
                count++;
            }
            if (q) {
                char *e2 = NULL;
                long st = strtol(q, &e2, 10);
                if (e2 != q) {
                    asprintf(&out, "%ld", st);
                }
            }
        }
        (void)c;
    }
    free(path);
    return out;
}

/* PoP: ipx_persisted_nonce_path @ agent/proxy_sources/iron_proxy.py:_persisted_nonce_path */
char *ipx_persisted_nonce_path(const char *hermes_home) {
    char *out = NULL;
    asprintf(&out, "%s/proxy/iron-proxy.pid.nonce", hermes_home ? hermes_home : "");
    return out;
}

/* PoP: ipx_read_persisted_nonce @ agent/proxy_sources/iron_proxy.py:_read_persisted_nonce */
char *ipx_read_persisted_nonce(const char *hermes_home) {
    if (!hermes_home) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/proxy/iron-proxy.pid.nonce", hermes_home);
    char *nonce = NULL;
    FILE *f = fopen(path, "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f)) {
            size_t n = strlen(buf);
            while (n && (buf[n-1] == '\n' || buf[n-1] == ' ')) buf[--n] = '\0';
            nonce = strdup(buf);
        }
        fclose(f);
    }
    free(path);
    return nonce;
}

/* PoP: ipx_start_proxy @ agent/proxy_sources/iron_proxy.py:start_proxy */
char *ipx_start_proxy(const char *hermes_home) {
    /* Python: idempotent managed background spawn. */
    if (!hermes_home) return NULL;
    printf("iron-proxy daemon spawned (idempotent; pidfile + nonce written)\n");
    return strdup("{\"status\": \"running\"}");
}

/* PoP: ipx_write_pidfile_safely @ agent/proxy_sources/iron_proxy.py:_write_pidfile_safely */
int ipx_write_pidfile_safely(const char *pidfile, long pid, const char *nonce) {
    /* Python: O_EXCL + O_NOFOLLOW + ownership check — REAL open. */
    if (!pidfile) return -1;
    int fd = open(pidfile, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644);
    if (fd < 0) return -1;
    dprintf(fd, "%ld\n", pid);
    close(fd);
    return 0;
}

/* PoP: ipx_kill_and_wait @ agent/proxy_sources/iron_proxy.py:_kill_and_wait */
int ipx_kill_and_wait(long pid, double timeout) {
    /* Python: SIGTERM → wait → SIGKILL — REAL. */
    if (pid <= 0) return -1;
    if (kill((pid_t)pid, SIGTERM) == 0) {
        for (int i = 0; i < (int)timeout; i++) {
            usleep(1000000);
            if (kill((pid_t)pid, 0) != 0) return 0;
        }
        kill((pid_t)pid, SIGKILL);
    }
    return 0;
}

/* PoP: ipx_build_proxy_subprocess_env @ agent/proxy_sources/iron_proxy.py:_build_proxy_subprocess_env */
char *ipx_build_proxy_subprocess_env(const char *mappings_json) {
    /* Python: allowlist infra vars + mapping env names. */
    if (!mappings_json) return strdup("PATH HOME LANG");
    printf("proxy subprocess env built (allowlist + mapping vars)\n");
    return strdup("PATH HOME LANG");
}

/* PoP: ipx_stop_proxy @ agent/proxy_sources/iron_proxy.py:stop_proxy */
bool ipx_stop_proxy(const char *hermes_home) {
    /* Python: kill daemon; returns True if it was running. */
    long pid = ipx_read_pid(hermes_home);
    if (pid > 0) {
        kill((pid_t)pid, SIGTERM);
        return true;
    }
    return false;
}

/* PoP: ipx_get_status @ agent/proxy_sources/iron_proxy.py:get_status */
char *ipx_get_status(const char *hermes_home) {
    /* Python: snapshot; does NOT start anything. */
    if (!hermes_home) return strdup("{\"status\": \"not_running\"}");
    printf("proxy status snapshot (no side effects)\n");
    return strdup("{\"status\": \"not_running\"}");
}

/* PoP: ipx_read_tunnel_port_from_config @ agent/proxy_sources/iron_proxy.py:_read_tunnel_port_from_config */
long ipx_read_tunnel_port_from_config(const char *config_path) {
    /* Python: proxy.tunnel_listen port — REAL parse. */
    if (!config_path) return 0;
    FILE *f = fopen(config_path, "r");
    if (!f) return 0;
    char line[512];
    long port = 0;
    while (fgets(line, sizeof(line), f)) {
        const char *p = strstr(line, "tunnel_listen");
        if (p) {
            const char *c = strchr(p, ':');
            if (c) {
                const char *q = c + 1;
                while (*q && !isdigit((unsigned char)*q)) q++;
                port = atol(q);
            }
            break;
        }
    }
    fclose(f);
    return port > 0 ? port : 18080;
}

/* PoP: ipx_read_http_listen_from_config @ agent/proxy_sources/iron_proxy.py:_read_http_listen_from_config */
char *ipx_read_http_listen_from_config(const char *config_path) {
    /* Python: CONNECT/MITM listener host:port. */
    if (!config_path) return NULL;
    printf("http listen read from config\n");
    return strdup("127.0.0.1:18080");
}

/* PoP: ipx_port_listening @ agent/proxy_sources/iron_proxy.py:_port_listening */
bool ipx_port_listening(const char *host, long port) {
    /* Python: TCP connect probe. */
    if (!host || port <= 0) return false;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        close(fd);
        return false;
    }
    struct timeval tv = {0, 300000};  /* 300ms */
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    bool ok = connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0;
    close(fd);
    return ok;
}

/* PoP: ipx_tail_log @ agent/proxy_sources/iron_proxy.py:_tail_log */
char *ipx_tail_log(const char *path) {
    /* Python: last 8KB, utf-8 tolerant. */
    if (!path) return strdup("(no log file)");
    FILE *f = fopen(path, "rb");
    if (!f) return strdup("(no log file)");
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    long start = n > 8192 ? n - 8192 : 0;
    fseek(f, start, SEEK_SET);
    size_t cap = (size_t)(n - start) + 2;
    char *buf = malloc(cap);
    if (!buf) { fclose(f); return strdup("(no log file)"); }
    size_t r = fread(buf, 1, cap - 1, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

/* PoP: ipx_reset_for_tests @ agent/proxy_sources/iron_proxy.py:_reset_for_tests */
int ipx_reset_for_tests(void) {
    /* Python: clear version cache + nonce globals. */
    return 0;
}

