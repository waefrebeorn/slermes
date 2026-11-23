/*
 * __init__.c — C port of upstream Hermes hermes_platform/__init__.py.
 *
 * The live __init__.py is documentation-only ("Machine facts and resource
 * resolution"); the functions that implement those machine facts live in
 * hermes_platform/host/facts.py. This port implements those pure fact decoders.
 *
 * PoP: w11_init_b19b81_normalize_arch @ hermes_platform/host/facts.py:normalize_arch
 * PoP: w11_init_b19b81_windows_native_arch @ hermes_platform/host/facts.py:windows_native_arch
 * PoP: w11_init_b19b81_classify_gpu_vendors @ hermes_platform/host/facts.py:classify_gpu_vendors
 * PoP: w11_init_b19b81_parse_cpuinfo @ hermes_platform/host/facts.py:parse_cpuinfo
 * PoP: w11_init_b19b81_parse_cpuinfo_vendor @ hermes_platform/host/facts.py:parse_cpuinfo_vendor
 * PoP: w11_init_b19b81_parse_meminfo_total @ hermes_platform/host/facts.py:parse_meminfo_total
 */
#include "noc_w11_init_b19b81.h"
#include "hermes_json.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *trim(const char *s) {
    static char buf[8192];
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';
    return buf;
}

static void lower_ascii(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static const char *normalize_arch_raw(const char *raw) {
    static char buf[64];
    const char *v = trim(raw);
    if (v) { size_t n = strlen(v); if (n >= sizeof(buf)) n = sizeof(buf) - 1; memcpy(buf, v, n); buf[n] = 0; lower_ascii(buf); }
    else buf[0] = 0;
    if (!strcmp(buf, "amd64") || !strcmp(buf, "x86_64") || !strcmp(buf, "x64")) return "amd64";
    if (!strcmp(buf, "arm64") || !strcmp(buf, "aarch64")) return "arm64";
    if (!strcmp(buf, "x86") || !strcmp(buf, "i386") || !strcmp(buf, "i686")) return "x86";
    return "unknown";
}

struct json_t *w11_init_b19b81_normalize_arch(const char *raw) {
    return json_string(normalize_arch_raw(raw));
}

struct json_t *w11_init_b19b81_windows_native_arch(const char *wow64_native, const char *machine) {
    long long v = 0;
    if (wow64_native && *wow64_native && strcmp(wow64_native, "none") != 0)
        v = strtoll(wow64_native, NULL, 10);
    if (v == 0x8664) return json_string("amd64");
    if (v == 0xAA64) return json_string("arm64");
    if (v == 0x014C) return json_string("x86");
    return json_string(normalize_arch_raw(machine));
}

struct json_t *w11_init_b19b81_classify_gpu_vendors(const struct json_t *vendor_ids) {
    int any_nonblank = 0, found_nvidia = 0, found_amd = 0, found_intel = 0, count = 0;
    if (vendor_ids && vendor_ids->type == JSON_ARRAY) {
        count = (int)json_len(vendor_ids);
        for (size_t i = 0; i < json_len(vendor_ids); i++) {
            json_t *e = json_get(vendor_ids, i);
            const char *v = (e && e->type == JSON_STRING) ? e->str_val : "";
            const char *t = trim(v);
            if (*t) any_nonblank = 1;
            char norm[64];
            size_t n = strlen(t); if (n >= sizeof(norm)) n = sizeof(norm) - 1;
            memcpy(norm, t, n); norm[n] = 0; lower_ascii(norm);
            if (!strncmp(norm, "0x", 2)) memmove(norm, norm + 2, strlen(norm + 2) + 1);
            if (!strcmp(norm, "10de")) found_nvidia = 1;
            else if (!strcmp(norm, "1002")) found_amd = 1;
            else if (!strcmp(norm, "8086")) found_intel = 1;
        }
    }
    if (count > 0 && !any_nonblank) return json_string("unknown");
    if (found_nvidia) return json_string("nvidia");
    if (found_amd) return json_string("amd");
    if (found_intel) return json_string("intel");
    return json_string("none");
}

/* iterate lines like Python str.splitlines() (ASCII \n / \r terminators) */
static const char *next_line(const char **cur) {
    const char *start = *cur;
    if (!*start) return NULL;
    const char *p = start;
    while (*p && *p != '\n' && *p != '\r') p++;
    size_t n = (size_t)(p - start);
    static char line[8192];
    if (n >= sizeof(line)) n = sizeof(line) - 1;
    memcpy(line, start, n); line[n] = 0;
    if (*p == '\r') p++;
    if (*p == '\n') p++;
    *cur = p;
    return line;
}

static int starts_with_ci(const char *s, const char *prefix) {
    while (*prefix) {
        if (!*s) return 0;
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return 0;
        s++; prefix++;
    }
    return 1;
}

static const char *after_colon_stripped(const char *line) {
    const char *c = strchr(line, ':');
    if (!c) return NULL;
    return trim(c + 1);
}

struct json_t *w11_init_b19b81_parse_cpuinfo(const char *text, const char *device_tree_model) {
    const char *keys[2] = {"model name", "Hardware"};
    for (int k = 0; k < 2; k++) {
        const char *cur = text ? text : "";
        const char *line;
        while ((line = next_line(&cur)) != NULL) {
            if (starts_with_ci(line, keys[k]) && strchr(line, ':')) {
                return json_string(after_colon_stripped(line));
            }
        }
    }
    static char out[8192];
    const char *d = device_tree_model ? device_tree_model : "";
    size_t j = 0;
    for (const char *p = d; *p && j + 1 < sizeof(out); p++) {
        out[j++] = (*p == '_') ? ' ' : (*p == '\0' ? ' ' : *p);
    }
    out[j] = 0;
    return json_string(trim(out));
}

struct json_t *w11_init_b19b81_parse_cpuinfo_vendor(const char *text) {
    char vendor[1024] = "", implementer[1024] = "";
    const char *cur = text ? text : "";
    const char *line;
    while ((line = next_line(&cur)) != NULL) {
        const char *c = strchr(line, ':');
        if (!c) continue;
        char key[512], val[1024];
        size_t n = (size_t)(c - line); if (n >= sizeof(key)) n = sizeof(key) - 1;
        memcpy(key, line, n); key[n] = 0;
        const char *v = trim(c + 1);
        n = strlen(v); if (n >= sizeof(val)) n = sizeof(val) - 1;
        memcpy(val, v, n); val[n] = 0;
        const char *kt = trim(key);
        if (!strcmp(kt, "vendor_id")) { snprintf(vendor, sizeof(vendor), "%s", val); }
        else if (!strcmp(kt, "CPU implementer")) { snprintf(implementer, sizeof(implementer), "%s", val); }
    }
    if (*vendor) return json_string(vendor);
    char *end = NULL;
    unsigned long long code = strtoull(implementer, &end, 16);
    if (end == implementer || *end) return json_string("");
    if (code == 0x41) return json_string("ARM");
    if (code == 0x42) return json_string("Broadcom");
    if (code == 0x43) return json_string("Cavium");
    if (code == 0x46) return json_string("Fujitsu");
    if (code == 0x48) return json_string("HiSilicon");
    if (code == 0x4E) return json_string("NVIDIA");
    if (code == 0x50) return json_string("Ampere");
    if (code == 0x51) return json_string("Qualcomm");
    if (code == 0x61) return json_string("Apple");
    if (code == 0x69) return json_string("Intel");
    if (code == 0x6D) return json_string("Microsoft");
    if (code == 0xC0) return json_string("Ampere");
    return json_string("");
}

struct json_t *w11_init_b19b81_parse_meminfo_total(const char *text) {
    const char *cur = text ? text : "";
    const char *line;
    while ((line = next_line(&cur)) != NULL) {
        if (strncmp(line, "MemTotal:", 9) != 0) continue;
        /* fields = line.split(); fields[1].isdigit() */
        char field[512] = "";
        const char *p = line + 9;
        while (*p && isspace((unsigned char)*p)) p++;
        const char *q = p;
        while (*q && !isspace((unsigned char)*q)) q++;
        size_t n = (size_t)(q - p); if (n >= sizeof(field)) n = sizeof(field) - 1;
        memcpy(field, p, n); field[n] = 0;
        int digits = *field != 0;
        for (const char *z = field; *z; z++) if (!isdigit((unsigned char)*z)) { digits = 0; break; }
        if (digits) {
            unsigned long long kb = strtoull(field, NULL, 10);
            return json_number((double)kb * 1024.0);
        }
    }
    return json_null();
}
