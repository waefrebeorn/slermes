/*
 * products.c - C11 port of upstream hermes_platform/host/products.py
 * (gate w24_products_7eaca0).
 *
 * PoP: w24_products_7eaca0_is_nvidia_n1x_pci_id @ hermes_platform/host/products.py:is_nvidia_n1x_pci_id
 * PoP: w24_products_7eaca0_classify_nvidia_platform @ hermes_platform/host/products.py:classify_nvidia_platform
 * PoP: w24_products_7eaca0_constants @ hermes_platform/host/products.py:<module>
 *
 * Seams: ``facts.os_family`` / ``native_arch`` / ``cpu_model`` / ``gpu_class`` /
 * ``nvidia_gpus`` read the registry, sysfs, procfs and sysctl; here they are
 * arguments.  ``nvidia_platform`` is that seam plus this same classifier.
 * Ported: the NVML-packed N1X id test and the recognition ladder -- win32+arm64
 * with an N1X CPU marker or N1X GPU is rtx_spark, linux+arm64 with a GB10 /
 * DGX Spark driver name, CPU or device-tree model is dgx_spark, anything else
 * with an NVIDIA GPU is discrete, and no NVIDIA GPU is None.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_products_7eaca0.h"

#define RTX_SPARK_CPU_MARKER "N1X"
static const char *const DGX_SPARK_MARKERS[W24_PRODUCTS_7EACA0_DGX_MARKERS] = {
    "GB10", "DGX SPARK",
};

#define NVIDIA_PCI_VENDOR 0x10DE
#define N1X_DEVICE_MIN 0x2E00
#define N1X_DEVICE_MAX 0x2E3F

/* Python int(): only an int (or a bool, which is one) passes.  A JSON number
 * with a fractional part came from a float literal, where ``&``/``>>`` raise. */
static int pd_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val != (double)(long long)v->num_val) return 0;   /* float seam */
        *out = (long long)v->num_val;
        return 1;
    }
    return 0;
}

/* Upper-cased, ASCII-only, with "_" widened to a space (Python .replace/.upper
 * over the markers, which are ASCII). */
static char *pd_norm_cpu(const char *s) {
    size_t n = strlen(s), i;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = s[i] == '_' ? ' ' : s[i];
        out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    out[n] = '\0';
    return out;
}

static int pd_n1x_id(long long pci_id) {
    long long device = pci_id >> 16;              /* arithmetic, as Python's >> */
    return (pci_id & 0xFFFF) == NVIDIA_PCI_VENDOR &&
           device >= N1X_DEVICE_MIN && device <= N1X_DEVICE_MAX;
}

/* " ".join(gpu.name for gpu in gpus).upper() */
static char *pd_gpu_names(const json_t *gpus, size_t n) {
    size_t i, len = 0, cap = 64;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    for (i = 0; i < n; i++) {
        const json_t *g = json_get(gpus, i);
        const char *nm = g && g->type == JSON_OBJECT ? json_get_str(g, "name", "") : "";
        size_t ln = strlen(nm), k;
        if (len + ln + 2 > cap) {
            char *np;
            cap = (len + ln + 2) * 2;
            np = (char *)realloc(out, cap);
            if (!np) { free(out); return NULL; }
            out = np;
        }
        if (i > 0) out[len++] = ' ';
        for (k = 0; k < ln; k++) {
            char c = nm[k];
            out[len++] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
        }
        out[len] = '\0';
    }
    return out;
}

json_t *w24_products_7eaca0_is_nvidia_n1x_pci_id(const json_t *pci_id) {
    long long v = 0;
    if (!pd_int(pci_id, &v)) return json_null();  /* TypeError seam */
    return json_bool(pd_n1x_id(v) ? true : false);
}

json_t *w24_products_7eaca0_classify_nvidia_platform(const json_t *req) {
    const json_t *gpus, *cpu_v;
    const char *os_family, *native_arch, *gpu_class;
    char *cpu;
    size_t i, n;
    int arm64;

    if (!req || req->type != JSON_OBJECT) return json_null();
    os_family = json_get_str(req, "os_family", "");
    native_arch = json_get_str(req, "native_arch", "");
    gpu_class = json_get_str(req, "gpu_class", "");
    cpu_v = json_obj_get(req, "cpu_model");
    if (cpu_v && cpu_v->type != JSON_STRING && cpu_v->type != JSON_NULL)
        return json_null();                       /* .replace AttributeError seam */
    cpu = pd_norm_cpu(cpu_v && cpu_v->str_val ? cpu_v->str_val : "");
    if (!cpu) return json_null();
    gpus = json_obj_get(req, "nvidia_gpus");
    n = (gpus && gpus->type == JSON_ARRAY) ? json_len(gpus) : 0;
    arm64 = strcmp(native_arch, "arm64") == 0;

    if (arm64 && strcmp(os_family, "win32") == 0) {
        int n1x_gpu = 0;
        for (i = 0; i < n; i++) {
            const json_t *g = json_get(gpus, i);
            const json_t *pid = g && g->type == JSON_OBJECT ? json_obj_get(g, "pci_id") : NULL;
            long long v = 0;
            if (!pid || pid->type == JSON_NULL) continue;     /* pci_id is None */
            if (!pd_int(pid, &v)) { free(cpu); return json_null(); }
            if (pd_n1x_id(v)) { n1x_gpu = 1; break; }
        }
        if (n1x_gpu || strstr(cpu, RTX_SPARK_CPU_MARKER)) {
            free(cpu);
            return json_string("rtx_spark");
        }
    }
    if (arm64 && strncmp(os_family, "linux", 5) == 0) {
        int hit = 0;
        size_t j;
        char *names = pd_gpu_names(gpus, n);
        for (j = 0; j < W24_PRODUCTS_7EACA0_DGX_MARKERS; j++) {
            if ((names && strstr(names, DGX_SPARK_MARKERS[j])) ||
                strstr(cpu, DGX_SPARK_MARKERS[j])) { hit = 1; break; }
        }
        free(names);
        if (hit) { free(cpu); return json_string("dgx_spark"); }
    }
    free(cpu);
    if (strcmp(gpu_class, "nvidia") == 0) return json_string("discrete");
    return json_null();
}

json_t *w24_products_7eaca0_constants(void) {
    json_t *o = json_object();
    json_t *markers = json_array();
    size_t i;
    for (i = 0; i < W24_PRODUCTS_7EACA0_DGX_MARKERS; i++)
        json_append(markers, json_string(DGX_SPARK_MARKERS[i]));
    json_set(o, "rtx_spark_cpu_marker", json_string(RTX_SPARK_CPU_MARKER));
    json_set(o, "dgx_spark_markers", markers);
    return o;
}
