/*
 * _doctor.c - real C11 port of upstream Hermes persistence/sqlite/admin/_doctor.py
 * pure helpers: the on-disk header journal mode, the mode-conversion admission
 * refusal, and the newest mtime over a set of stat results.
 * PoP: w19_doctor_60f499_header_mode @ persistence/sqlite/admin/_doctor.py:_header_mode
 * PoP: w19_doctor_60f499_refusal @ persistence/sqlite/admin/_doctor.py:_refusal
 * PoP: w19_doctor_60f499_newest_mtime_ns @ persistence/sqlite/admin/_doctor.py:_newest_mtime_ns
 */
#include "noc_w19_doctor_60f499.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Above this the snapshot copy a held store needs costs more than the probe is worth. */
static const long long WRITE_PROBE_SNAPSHOT_MAX_BYTES = 1LL << 30;
/* SQLite file header: bytes 18 (write version) / 19 (read version). */
static const char SQLITE_MAGIC[16] = {
    'S', 'Q', 'L', 'i', 't', 'e', ' ', 'f', 'o', 'r', 'm', 'a', 't', ' ', '3', '\0'
};

/* Compose a malloc'd string node (no strdup anywhere in this port). */
static json_t *json_printf_string(const char *fmt, const char *a, const char *b) {
    size_t need = strlen(fmt) + (a ? strlen(a) : 6) + (b ? strlen(b) : 6) + 2;
    char *buf = (char *)malloc(need);
    if (!buf) return json_null();
    snprintf(buf, need, fmt, a ? a : "(null)", b ? b : "(null)");
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode hex text into bytes; returns the number of bytes decoded (stops at the first
 * malformed pair), never more than `cap`. */
static size_t hex_decode(const char *hex, unsigned char *out, size_t cap) {
    size_t n = 0;
    if (!hex) return 0;
    while (hex[0] && hex[1] && n < cap) {
        int hi = hex_val(hex[0]), lo = hex_val(hex[1]);
        if (hi < 0 || lo < 0) break;
        out[n++] = (unsigned char)((hi << 4) | lo);
        hex += 2;
    }
    return n;
}

json_t *w19_doctor_60f499_header_mode(const char *header_hex) {
    unsigned char head[32];
    size_t len = hex_decode(header_hex, head, sizeof(head));
    char current[32];
    if (len < 20 || memcmp(head, SQLITE_MAGIC, 16) != 0)
        return json_string("not-a-database");
    unsigned w = head[18], r = head[19];
    if (w == 1 && r == 1) return json_string("delete");
    if (w == 2 && r == 2) return json_string("wal");
    snprintf(current, sizeof(current), "unknown(%u/%u)", w, r);
    return json_string(current);
}

json_t *w19_doctor_60f499_refusal(const char *target, const char *current, int on_cross_vm_fs) {
    if (!current) return json_null();
    if (!strcmp(current, "not-a-database") || !strncmp(current, "unknown(", 8)) {
        return json_printf_string(
            "its file header reads %s, so it is not a Hermes SQLite store this command "
            "can convert (pass --db PATH to point at one).", current, NULL);
    }
    if (target && !strcmp(target, "wal") && on_cross_vm_fs) {
        return json_string(
            "WAL shared memory silently corrupts on cross-VM filesystems (virtiofs/9p "
            "\xE2\x80\x94 Docker Desktop, OrbStack, Lima bind mounts). Keep this store on "
            "journal_mode=delete or move it to a native filesystem.");
    }
    return json_null();
}

/* Parse one decimal/JSON scalar mtime; returns 1 when a value was produced. */
static int mtime_value(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        if (!s || !*s) return 0;
        char *end = NULL;
        long long x = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
        if (*end) return 0;
        *out = x;
        return 1;
    }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    return 0;
}

json_t *w19_doctor_60f499_newest_mtime_ns(json_t *values) {
    int have = 0;
    long long best = 0;
    if (values && values->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(values); i++) {
            long long x;
            if (!mtime_value(json_get(values, i), &x)) continue;
            if (!have || x > best) { best = x; have = 1; }
        }
    } else {
        long long x;
        if (mtime_value(values, &x)) { best = x; have = 1; }
    }
    if (!have) return json_null();
    (void)WRITE_PROBE_SNAPSHOT_MAX_BYTES;
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", best);
    return json_string(buf);
}
