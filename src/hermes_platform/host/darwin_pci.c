/*
 * darwin_pci.c — C11 port of upstream Hermes hermes_platform/host/darwin_pci.py
 * (gate w25_darwinpci_ff026b).
 * STATUS: real behaviour port of the value half; the IOKit registry, the
 *         CFData handles and the ctypes bindings are the seam.
 * PoP: w25_darwinpci_ff026b_data_int @ hermes_platform/host/darwin_pci.py:_data_int
 * PoP: w25_darwinpci_ff026b_format_vendor @ hermes_platform/host/darwin_pci.py:display_vendor_ids
 * PoP: w25_darwinpci_ff026b_display_vendor_ids @ hermes_platform/host/darwin_pci.py:display_vendor_ids
 * PoP: w25_darwinpci_ff026b_constants @ hermes_platform/host/darwin_pci.py:_DISPLAY_CLASS
 *
 * Intel Macs name their GPU vendors here; Apple silicon has no PCI display
 * controller.  A property arrives as the bytes its CFData carried, so the
 * decisions that remain are: little-endian assembly, the base-class filter
 * (class-code >> 16 == 0x03) and the "vendor & 0xffff:04x" spelling.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_darwinpci_ff026b.h"

/* _DISPLAY_CLASS / _MAX_DEVICES / _UTF8 (kCFStringEncodingUTF8) */
#define DP_DISPLAY_CLASS 0x03
#define DP_MAX_DEVICES 256
#define DP_UTF8 0x08000100

/* Hex digit value, or -1. */
static int dp_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* The bytes a CFData property carried, or 0 when the shape is not data. */
static int dp_bytes(const json_t *hex, unsigned char *buf, size_t cap, size_t *n) {
    size_t len, i;
    const char *s;
    if (!hex || hex->type != JSON_STRING || !hex->str_val) return 0;
    s = hex->str_val;
    len = strlen(s);
    if (len % 2 != 0) return 0;
    if (len / 2 > cap) return 0;
    for (i = 0; i < len; i += 2) {
        int hi = dp_hexval(s[i]), lo = dp_hexval(s[i + 1]);
        if (hi < 0 || lo < 0) return 0;
        buf[i / 2] = (unsigned char)((hi << 4) | lo);
    }
    *n = len / 2;
    return 1;
}

json_t *w25_darwinpci_ff026b_data_int(const json_t *hex) {
    unsigned char buf[16];
    size_t n = 0, i;
    unsigned long long acc = 0;

    if (!hex || hex->type == JSON_NULL) return json_null(); /* missing / not data */
    if (!dp_bytes(hex, buf, sizeof(buf), &n)) return json_null();
    if (n == 0) return json_null(); /* CFDataGetLength() == 0 */
    /* int.from_bytes(bytes, "little") */
    for (i = n; i > 0; i--) acc = (acc << 8) | buf[i - 1];
    return json_number((double)acc);
}

static int dp_value(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

json_t *w25_darwinpci_ff026b_format_vendor(const json_t *value) {
    char buf[16];
    double d;
    if (!dp_value(value, &d)) return json_string(""); /* vendor-id missing */
    snprintf(buf, sizeof(buf), "%04x", (unsigned)((long long)d & 0xFFFF));
    return json_string(buf);
}

json_t *w25_darwinpci_ff026b_display_vendor_ids(const json_t *devices, const json_t *readable) {
    json_t *out;
    size_t n, i;

    if (readable && readable->type == JSON_BOOL && !readable->bool_val)
        return json_null(); /* IOKit could not be read */
    if (!devices || devices->type != JSON_ARRAY) return json_null();
    out = json_array();
    n = json_len(devices);
    if (n > DP_MAX_DEVICES) n = DP_MAX_DEVICES; /* for _ in range(_MAX_DEVICES) */
    for (i = 0; i < n; i++) {
        const json_t *dev = json_get(devices, i);
        json_t *class_code, *vendor;
        double cc = 0.0;
        if (!dev || dev->type != JSON_OBJECT) continue;
        class_code = w25_darwinpci_ff026b_data_int(json_obj_get(dev, "class_code"));
        vendor = w25_darwinpci_ff026b_data_int(json_obj_get(dev, "vendor_id"));
        if (class_code && dp_value(class_code, &cc) &&
            ((long long)cc >> 16) == DP_DISPLAY_CLASS) {
            json_append(out, w25_darwinpci_ff026b_format_vendor(vendor));
        }
        json_free(class_code);
        json_free(vendor);
    }
    return out;
}

json_t *w25_darwinpci_ff026b_constants(void) {
    json_t *o = json_object();
    json_set(o, "display_class", json_number(DP_DISPLAY_CLASS));
    json_set(o, "max_devices", json_number(DP_MAX_DEVICES));
    json_set(o, "utf8_encoding", json_number(DP_UTF8));
    json_set(o, "service_class", json_string("IOPCIDevice"));
    json_set(o, "class_code_key", json_string("class-code"));
    json_set(o, "vendor_id_key", json_string("vendor-id"));
    return o;
}
