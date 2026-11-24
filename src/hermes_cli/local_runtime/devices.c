/*
 * devices.c — C home for upstream Hermes hermes_cli/local_runtime/devices.py.
 * STATUS: BEHAVIOR PORT — value->value device-filter parity, gate w25_devices_9a1f72.
 * PoP: hermes_cli_local_runtime_devices_pending @ hermes_cli/local_runtime/devices.py:<module>
 * PoP: w25_devices_9a1f72_probe_devices @ hermes_cli/local_runtime/devices.py:probe_devices
 * PoP: w25_devices_9a1f72_library @ hermes_cli/local_runtime/devices.py:_library
 * PoP: w25_devices_9a1f72_constants @ hermes_cli/local_runtime/devices.py:GGML_DEVICE_GPU,GGML_DEVICE_IGPU
 *
 * ``probe_devices`` forks a child (``python -I devices.py <dir> <backend>``)
 * with a 20s deadline and reads its stdout, because loading a GPU driver can
 * crash or hang the caller.  That subprocess is the seam: the exit status, the
 * captured stdout and the timeout/OSError outcome arrive here as fixed inputs.
 * What is ported is what the module does with them: JSON-decode when the child
 * exited cleanly, demand a list, then keep only dicts whose type is a discrete
 * or integrated GPU, whose ``total`` is a positive int and whose
 * ``description`` is a string.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_devices_9a1f72.h"

/* enum ggml_backend_dev_type (ggml-backend.h): CPU, GPU, IGPU, ACCEL. */
#define DV_GGML_DEVICE_GPU  1
#define DV_GGML_DEVICE_IGPU 2

/* Python 'x in (1, 2)': equality across bool/int, None matches nothing. */
static int dv_type_admitted(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val == (double)DV_GGML_DEVICE_GPU
                                       || v->num_val == (double)DV_GGML_DEVICE_IGPU;
    return 0;
}

/* isinstance(x, int): bools are ints in Python; floats are not (an integral
 * JSON literal arrives as a double either way, so fixtures spell ints as bare
 * integer literals). */
static int dv_total_admitted(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type != JSON_NUMBER) return 0;
    if (v->num_val != (double)(long long)v->num_val) return 0;
    return v->num_val > 0.0;
}

json_t *w25_devices_9a1f72_probe_devices(const json_t *request) {
    const json_t *stdout_value = json_obj_get(request, "stdout");
    const json_t *raise_value = json_obj_get(request, "raise");
    const char *raise_mode = raise_value && raise_value->type == JSON_STRING ? raise_value->str_val : "none";
    const char *text = stdout_value && stdout_value->type == JSON_STRING ? stdout_value->str_val : NULL;
    double rc = json_get_num(request, "returncode", 0.0);
    json_t *decoded;
    json_t *out = json_array();
    size_t i;

    /* The child raised OSError / timed out: the caller answers with []. */
    if (raise_mode && (!strcmp(raise_mode, "timeout") || !strcmp(raise_mode, "oserror"))) return out;
    /* A nonzero exit means no trustworthy stdout, whatever it holds. */
    if (rc != 0.0) return out;
    if (!text) return out;
    decoded = json_parse(text, NULL);
    if (!decoded) return out;                      /* json.loads -> ValueError */
    if (decoded->type != JSON_ARRAY) { json_free(decoded); return out; }

    for (i = 0; i < json_len(decoded); i++) {
        const json_t *d = json_get(decoded, i);
        const json_t *desc;
        if (!d || d->type != JSON_OBJECT) continue;
        if (!dv_type_admitted(json_obj_get(d, "type"))) continue;
        if (!dv_total_admitted(json_obj_get(d, "total"))) continue;
        desc = json_obj_get(d, "description");
        if (!desc || desc->type != JSON_STRING) continue;
        json_append(out, json_copy(d));
    }
    json_free(decoded);
    return out;
}

json_t *w25_devices_9a1f72_library(const json_t *request) {
    const char *directory = json_get_str(request, "directory", "");
    const char *name = json_get_str(request, "name", "");
    int nt = json_get_bool(request, "nt", true);
    size_t dir_len = strlen(directory), need;
    char *buf, *p;

    while (dir_len > 1 && (directory[dir_len - 1] == '/' || directory[dir_len - 1] == '\\')) dir_len--;
    need = dir_len + strlen(name) + 16;
    buf = (char *)malloc(need);
    if (!buf) return json_string("");
    p = buf;
    memcpy(p, directory, dir_len);
    p += dir_len;
    if (dir_len > 0 && directory[dir_len - 1] != '/') *p++ = '/';
    if (nt) p += sprintf(p, "%s.dll", name);
    else p += sprintf(p, "lib%s.so", name);
    *p = '\0';
    {
        json_t *out = json_string(buf);
        free(buf);
        return out;
    }
}

json_t *w25_devices_9a1f72_constants(void) {
    json_t *out = json_object();
    json_set(out, "ggml_device_gpu", json_number(DV_GGML_DEVICE_GPU));
    json_set(out, "ggml_device_igpu", json_number(DV_GGML_DEVICE_IGPU));
    return out;
}
