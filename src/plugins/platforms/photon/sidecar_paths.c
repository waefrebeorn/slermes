/*
 * sidecar_paths.c - C11 port of upstream
 * plugins/platforms/photon/sidecar_paths.py (gate w23_sidecarpat_472c0c).
 *
 * PoP: w23_sidecarpat_472c0c_resolve @ plugins/platforms/photon/sidecar_paths.py:resolve_sidecar_dir
 * PoP: w23_sidecarpat_472c0c_sidecar_dir @ plugins/platforms/photon/sidecar_paths.py:_sidecar_dir
 * PoP: w23_sidecarpat_472c0c_npm_error_log @ plugins/platforms/photon/sidecar_paths.py:_npm_error_log
 * PoP: w23_sidecarpat_472c0c_dir_writable @ plugins/platforms/photon/sidecar_paths.py:dir_writable
 * PoP: w23_sidecarpat_472c0c_source_default @ plugins/platforms/photon/sidecar_paths.py:SOURCE_SIDECAR_DIR
 * PoP: w23_sidecarpat_472c0c_mirror_file @ plugins/platforms/photon/sidecar_paths.py:_MIRROR_FILES
 *
 * The filesystem is a seam: the caller supplies the probe result, the
 * node_modules presence, the lockfile freshness, the mirror outcome and the
 * per-file copy facts.  This file owns only the decision ladder, the exact
 * copy filter and the path text Path would render.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_sidecarpat_472c0c.h"

static const char *const k_mirror_files[] = {
    "index.mjs",
    "package.json",
    "package-lock.json",
    "patch-spectrum-mixed-attachments.mjs",
    ".npmrc"
};
#define SP_MIRROR_N (sizeof(k_mirror_files) / sizeof(k_mirror_files[0]))

static char *sp_dup(const char *text) {
    size_t n;
    char *p;
    if (!text) text = "";
    n = strlen(text);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, text, n + 1);
    return p;
}

size_t w23_sidecarpat_472c0c_mirror_file_count(void) { return SP_MIRROR_N; }

const char *w23_sidecarpat_472c0c_mirror_file(size_t index) {
    if (index >= SP_MIRROR_N) return NULL;
    return k_mirror_files[index];
}

char *w23_sidecarpat_472c0c_path_norm(const char *path) {
    size_t i, j = 0, len;
    int prev_sep = 0;
    char *out;

    if (!path || path[0] == '\0') return sp_dup(".");
    len = strlen(path);
    out = (char *)malloc(len + 2);
    if (!out) return NULL;
    for (i = 0; i < len; i++) {
        char c = path[i];
        if (c == '/') c = '\\';
        if (c == '\\') {
            if (prev_sep) continue;
            prev_sep = 1;
        } else {
            prev_sep = 0;
        }
        out[j++] = c;
    }
    out[j] = '\0';
    /* Path drops a trailing separator but keeps a bare drive root ("C:\"). */
    while (j > 1 && out[j - 1] == '\\') {
        if (j == 3 && out[1] == ':') break;
        out[--j] = '\0';
    }
    if (j == 0) {
        out[j++] = '\\';
        out[j] = '\0';
    }
    return out;
}

static char *sp_join(const char *dir, const char *name) {
    size_t dl, nl;
    int need_sep;
    char *o;
    /* Path(".") / "x" == Path("x"): a bare "." base contributes nothing. */
    if (!dir || dir[0] == '\0' || (dir[0] == '.' && dir[1] == '\0'))
        return sp_dup(name);
    dl = strlen(dir);
    nl = strlen(name);
    need_sep = (dir[dl - 1] != '\\');
    o = (char *)malloc(dl + nl + (need_sep ? 1u : 0u) + 1u);
    if (!o) return NULL;
    memcpy(o, dir, dl);
    if (need_sep) o[dl++] = '\\';
    memcpy(o + dl, name, nl + 1);
    return o;
}

static int sp_list_has(const json_t *arr, const char *name) {
    size_t i, n;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    n = json_len(arr);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, name) == 0)
            return 1;
    }
    return 0;
}

char *w23_sidecarpat_472c0c_source_default(void) {
    return w23_sidecarpat_472c0c_path_norm(W23_SIDECARPAT_472C0C_SOURCE_SIDECAR_DIR);
}

static json_t *sp_resolve(const json_t *world, int from_world);

json_t *w23_sidecarpat_472c0c_resolve(const json_t *world) {
    return sp_resolve(world, 1);
}

static json_t *sp_resolve(const json_t *world, int from_world) {
    const json_t *v, *src_files, *dst_files;
    char *source = NULL, *dir = NULL;
    json_t *out, *copies;
    int writable, node_modules, lock_newer, mirror_fail, same;
    size_t i;

    out = json_object();
    copies = json_array();

    /* source = Path(source_dir) if source_dir is not None else SOURCE_SIDECAR_DIR */
    v = from_world && world ? json_obj_get(world, "source_dir") : NULL;
    if (v && v->type == JSON_STRING) source = w23_sidecarpat_472c0c_path_norm(v->str_val);
    else source = w23_sidecarpat_472c0c_source_default();
    if (!source) goto finish;

    /* (1) PHOTON_SIDECAR_DIR override is returned as-is. */
    v = world ? json_obj_get(world, "override") : NULL;
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0] != '\0') {
        dir = w23_sidecarpat_472c0c_path_norm(v->str_val);
        goto finish;
    }

    /* (2) writable source dir -> run in place. */
    writable = json_get_bool(world, "writable", 0) ? 1 : 0;
    if (writable) {
        dir = sp_dup(source);
        goto finish;
    }

    /* (3) read-only tree with baked, current deps -> run in place. */
    node_modules = json_get_bool(world, "node_modules", 0) ? 1 : 0;
    lock_newer = json_get_bool(world, "lock_newer", 0) ? 1 : 0;
    if (node_modules && !lock_newer) {
        dir = sp_dup(source);
        goto finish;
    }

    /* (4) mirror to $HERMES_HOME/photon/sidecar, or fall back on OSError. */
    mirror_fail = json_get_bool(world, "mirror_fail", 0) ? 1 : 0;
    if (mirror_fail) {
        dir = sp_dup(source);
        goto finish;
    }
    {
        const json_t *h = world ? json_obj_get(world, "hermes_home") : NULL;
        char *home = w23_sidecarpat_472c0c_path_norm(
            (h && h->type == JSON_STRING) ? h->str_val : "");
        char *photon = home ? sp_join(home, "photon") : NULL;
        dir = photon ? sp_join(photon, "sidecar") : NULL;
        free(photon);
        free(home);
    }

    /* The mirrored set: source exists and mirror missing or content-different. */
    src_files = world ? json_obj_get(world, "src_files") : NULL;
    dst_files = world ? json_obj_get(world, "dst_files") : NULL;
    same = json_get_bool(world, "same", 0) ? 1 : 0;
    for (i = 0; i < SP_MIRROR_N; i++) {
        int have_src = sp_list_has(src_files, k_mirror_files[i]);
        int have_dst = sp_list_has(dst_files, k_mirror_files[i]);
        if (have_src && (!have_dst || !same))
            json_append(copies, json_string(k_mirror_files[i]));
    }

finish:
    json_set(out, "dir", json_string(dir ? dir : ""));
    json_set(out, "copies", copies);
    free(dir);
    free(source);
    return out;
}

char *w23_sidecarpat_472c0c_sidecar_dir(const json_t *world) {
    const json_t *v = world ? json_obj_get(world, "cached") : NULL;
    json_t *resolved;
    char *out;

    if (v && v->type == JSON_STRING)
        return w23_sidecarpat_472c0c_path_norm(v->str_val);
    resolved = sp_resolve(world, 0);
    out = sp_dup(resolved ? json_get_str(resolved, "dir", "") : "");
    json_free(resolved);
    return out;
}

char *w23_sidecarpat_472c0c_npm_error_log(const json_t *world) {
    const json_t *v = world ? json_obj_get(world, "log") : NULL;
    char *base, *out;

    if (v && v->type == JSON_STRING)
        return w23_sidecarpat_472c0c_path_norm(v->str_val);
    v = world ? json_obj_get(world, "sidecar_dir") : NULL;
    if (v && v->type == JSON_STRING)
        base = w23_sidecarpat_472c0c_path_norm(v->str_val);
    else
        base = w23_sidecarpat_472c0c_sidecar_dir(world);
    if (!base) return NULL;
    out = sp_join(base, ".photon-npm-error.log");
    free(base);
    return out;
}

int w23_sidecarpat_472c0c_dir_writable(const json_t *world) {
    return json_get_bool(world, "touch_ok", 0) ? 1 : 0;
}

/* Kept from the C-home placeholder; nothing links against it. */
void plugins_platforms_photon_sidecar_paths_port_pending(void) { (void)0; }
