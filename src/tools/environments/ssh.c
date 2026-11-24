
/*
 * ssh.c - real C11 port of the pure helpers of upstream Hermes tools/environments/ssh.py:
 * the connection-error text, the private-runtime-dir test, the ControlMaster socket
 * naming (SHA-256 of the SendEnv name-set), the port/key flags and the ssh argv.
 * Process spawning and ControlMaster teardown stay in Python.
 * PoP: w19_ssh_5f7591_sync_error @ tools/environments/ssh.py:_sync_error
 * PoP: w19_ssh_5f7591_private_runtime_dir @ tools/environments/ssh.py:_is_private_runtime_dir
 * PoP: w19_ssh_5f7591_control_socket_for @ tools/environments/ssh.py:SSHEnvironment._control_socket_for
 * PoP: w19_ssh_5f7591_target_flags @ tools/environments/ssh.py:SSHEnvironment._target_flags
 * PoP: w19_ssh_5f7591_build_ssh_command @ tools/environments/ssh.py:SSHEnvironment._build_ssh_command
 * PoP: w19_ssh_5f7591_ensure_ssh_available @ tools/environments/ssh.py:_ensure_ssh_available
 */
#include "noc_w19_ssh_5f7591.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* === SHA-256 (FIPS 180-4), enough for the ControlMaster name digests === */
static const uint32_t SHA_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t ror32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256_block(uint32_t h[8], const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + SHA_K[i] + w[i];
        uint32_t S0 = ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

/* hex digest of the message; out must hold 65 bytes. */
static void sha256_hex(const unsigned char *msg, size_t len, char *out) {
    uint32_t h[8] = { 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u };
    size_t blocks = (len + 9 + 63) / 64;
    unsigned char *buf = (unsigned char *)calloc(blocks * 64, 1);
    if (!buf) { out[0] = '\0'; return; }
    memcpy(buf, msg, len);
    buf[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8u;
    for (int i = 0; i < 8; i++)
        buf[blocks * 64 - 1 - i] = (unsigned char)((bits >> (8 * i)) & 0xff);
    for (size_t i = 0; i < blocks; i++) sha256_block(h, buf + i * 64);
    free(buf);
    static const char *hex = "0123456789abcdef";
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++) {
            unsigned char byte = (unsigned char)((h[i] >> (24 - 8 * j)) & 0xff);
            out[(i * 4 + j) * 2] = hex[byte >> 4];
            out[(i * 4 + j) * 2 + 1] = hex[byte & 0x0f];
        }
    out[64] = '\0';
}

json_t *w19_ssh_5f7591_sync_error(const char *reason, const char *subject, const char *what) {
    json_t *out = json_object();
    const char *w = (what && what[0]) ? what : "the SSH connection";
    char hint[1024];
    /* f"{subject} failed \u2014 verify {what} is healthy, then retry." (UTF-8 em dash) */
    snprintf(hint, sizeof(hint), "%s failed \xe2\x80\x94 verify %s is healthy, then retry.",
             subject ? subject : "", w);
    json_set(out, "reason", json_string(reason ? reason : ""));
    json_set(out, "retry_hint", json_string(hint));
    return out;
}

int w19_ssh_5f7591_private_runtime_dir(int lstat_fails, long long mode, long long uid, long long my_uid) {
    if (lstat_fails) return 0;                       /* except OSError: return False */
    int is_dir = (mode & 0170000LL) == 0040000LL;    /* stat.S_ISDIR */
    if (!is_dir) return 0;
    if (uid != my_uid) return 0;
    if (mode & 077LL) return 0;                      /* not st.st_mode & 0o077 */
    return 1;
}

/* " ".join(names), optionally sorted (tuple(sorted(send_env))). */
static char *join_names(json_t *send_env, int sort) {
    size_t n = (send_env && send_env->type == JSON_ARRAY) ? json_len(send_env) : 0;
    const char **names = (const char **)calloc(n ? n : 1, sizeof(char *));
    if (!names) return dupstr("");
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(send_env, i);
        if (v && v->type == JSON_STRING && v->str_val) names[k++] = v->str_val;
    }
    if (sort) {
        for (size_t i = 1; i < k; i++) {              /* insertion sort, k is tiny */
            const char *cur = names[i];
            size_t j = i;
            while (j > 0 && strcmp(names[j - 1], cur) > 0) { names[j] = names[j - 1]; j--; }
            names[j] = cur;
        }
    }
    size_t total = 1;
    for (size_t i = 0; i < k; i++) total += strlen(names[i]) + 1;
    char *joined = (char *)malloc(total);
    if (!joined) { free(names); return dupstr(""); }
    joined[0] = '\0';
    for (size_t i = 0; i < k; i++) {
        if (i) strcat(joined, " ");
        strcat(joined, names[i]);
    }
    free(names);
    return joined;
}

/* <target-id[:8]><sha256(joined)[:8]>.sock beside the plain socket. */
static char *socket_for_joined(const char *plain, const char *joined) {
    char digest[65];
    sha256_hex((const unsigned char *)joined, strlen(joined), digest);
    const char *slash = strrchr(plain, '/');
    const char *bslash = strrchr(plain, '\\');
    const char *cut = slash;
    if (!cut || (bslash && bslash > cut)) cut = bslash;
    size_t dirlen = cut ? (size_t)(cut + 1 - plain) : 0;
    const char *base = plain + dirlen;
    size_t base_len = strlen(base);
    size_t stem_len = base_len;
    const char *dot = NULL;
    for (size_t i = base_len; i > 0; i--)
        if (base[i - 1] == '.') { dot = base + i - 1; break; }
    if (dot) stem_len = (size_t)(dot - base);        /* Path.stem drops the last suffix */
    if (stem_len > 8) stem_len = 8;
    char *out = (char *)malloc(dirlen + stem_len + 13 + 1);
    if (!out) return NULL;
    memcpy(out, plain, dirlen);
    memcpy(out + dirlen, base, stem_len);
    memcpy(out + dirlen + stem_len, digest, 8);
    memcpy(out + dirlen + stem_len + 8, ".sock", 5);
    out[dirlen + stem_len + 13] = '\0';
    return out;
}

char *w19_ssh_5f7591_control_socket_for(const char *control_socket, json_t *send_env) {
    const char *plain = control_socket ? control_socket : "";
    size_t n = (send_env && send_env->type == JSON_ARRAY) ? json_len(send_env) : 0;
    if (!n) return dupstr(plain);                    /* if not send_env: return plain */
    char *joined = join_names(send_env, 0);          /* the name-set as given, unsorted */
    char *out = socket_for_joined(plain, joined);
    free(joined);
    return out ? out : dupstr(plain);
}

json_t *w19_ssh_5f7591_target_flags(const char *port_flag, long long port, const char *key_path) {
    json_t *out = json_array();
    const char *pf = (port_flag && port_flag[0]) ? port_flag : "-p";
    if (port != 22) {
        json_append(out, json_string(pf));
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", port);
        json_append(out, json_string(buf));
    }
    if (key_path && key_path[0]) {
        json_append(out, json_string("-i"));
        json_append(out, json_string(key_path));
    }
    return out;
}

static void append_str(json_t *arr, const char *s) {
    json_append(arr, json_string(s ? s : ""));
}

json_t *w19_ssh_5f7591_build_ssh_command(const char *control_socket, const char *user,
                                        const char *host, long long port, const char *key_path,
                                        json_t *send_env, json_t *extra_args, int multiplex) {
    json_t *cmd = json_array();
    size_t n = (send_env && send_env->type == JSON_ARRAY) ? json_len(send_env) : 0;
    char *joined = join_names(send_env, 1);          /* send_env = tuple(sorted(send_env)) */
    append_str(cmd, "ssh");
    if (multiplex) {
        char *sock = n ? socket_for_joined(control_socket ? control_socket : "", joined)
                       : dupstr(control_socket ? control_socket : "");
        append_str(cmd, "-o");
        if (sock) {
            size_t len = strlen(sock) + 16;
            char *opt = (char *)malloc(len);
            if (opt) { snprintf(opt, len, "ControlPath=%s", sock); append_str(cmd, opt); free(opt); }
            free(sock);
        }
        append_str(cmd, "-o");
        append_str(cmd, "ControlMaster=auto");
        append_str(cmd, "-o");
        append_str(cmd, "ControlPersist=300");
    }
    append_str(cmd, "-o");
    append_str(cmd, "BatchMode=yes");
    append_str(cmd, "-o");
    append_str(cmd, "StrictHostKeyChecking=accept-new");
    append_str(cmd, "-o");
    append_str(cmd, "ConnectTimeout=10");
    /* one -o SendEnv=<name> per name, in sorted order */
    {
        const char *p = joined;
        while (*p) {
            const char *sp = strchr(p, ' ');
            size_t len = sp ? (size_t)(sp - p) : strlen(p);
            char name[512];
            if (len >= sizeof(name)) len = sizeof(name) - 1;
            memcpy(name, p, len);
            name[len] = '\0';
            append_str(cmd, "-o");
            size_t optlen = len + 16;
            char *opt = (char *)malloc(optlen);
            if (opt) { snprintf(opt, optlen, "SendEnv=%s", name); append_str(cmd, opt); free(opt); }
            if (!sp) break;
            p = sp + 1;
        }
    }
    free(joined);
    json_t *flags = w19_ssh_5f7591_target_flags("-p", port, key_path);
    for (size_t i = 0; i < json_len(flags); i++) json_append(cmd, json_copy(json_get(flags, i)));
    json_free(flags);
    if (extra_args && extra_args->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(extra_args); i++) json_append(cmd, json_copy(json_get(extra_args, i)));
    size_t tlen = strlen(user ? user : "") + strlen(host ? host : "") + 2;
    char *target = (char *)malloc(tlen);
    if (target) {
        snprintf(target, tlen, "%s@%s", user ? user : "", host ? host : "");
        append_str(cmd, target);
        free(target);
    }
    return cmd;
}

const char *w19_ssh_5f7591_ensure_ssh_available(int ssh_ok, int scp_ok) {
    static char msg[512];
    const char *missing = !ssh_ok ? "ssh" : (!scp_ok ? "scp" : NULL);
    if (!missing) return NULL;
    char upper[8];
    size_t i = 0;
    for (; missing[i] && i < sizeof(upper) - 1; i++) upper[i] = (char)(missing[i] - 'a' + 'A');
    upper[i] = '\0';
    snprintf(msg, sizeof(msg), "%s is not installed or not in PATH. "
             "Install OpenSSH client: apt install openssh-client", upper);
    return msg;
}
