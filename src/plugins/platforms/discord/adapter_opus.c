/*
 * adapter_opus.c -- real C11 port of the once-per-process opus codec probe of
 * plugins/platforms/discord/adapter_opus.py.
 *
 * PoP: w21_adapteropu_d51886_bundled_opus_path @ plugins/platforms/discord/adapter_opus.py:_find_discord_windows_bundled_opus
 * PoP: w21_adapteropu_d51886_load_opus_codec_plan @ plugins/platforms/discord/adapter_opus.py:load_opus_codec
 *
 * Faithful details:
 *  - the bundled DLL is only probed on win32, and its architecture comes from
 *    the pointer size ("x64" when calcsize("P")*8 > 32, else "x86");
 *  - the Homebrew paths are an `elif`: they are only tried when
 *    find_library("opus") came back empty, and only on darwin;
 *  - a candidate that raises is warned about and the loop continues; the
 *    "codec not found" warning fires once when nothing loaded.
 */
#include "noc_w21_adapteropu_d51886.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ap_dup_n(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n && s) memcpy(out, s, n);
    else if (n) memset(out, 0, n);
    out[n] = '\0';
    return out;
}

static char *ap_dup(const char *s) {
    if (!s) s = "";
    return ap_dup_n(s, strlen(s));
}

static int ap_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* A path whose parent directory is everything before the last separator.
 * Separators are rendered as '/' (the gate's Path stand-in form), so a Windows
 * backslash path compares equal to the posix spelling of the same file. */
static char *ap_dirname(const char *path) {
    size_t n, i;
    char *out;
    if (!path || !path[0]) return ap_dup("");
    n = strlen(path);
    out = NULL;
    for (i = n; i > 0; i--) {
        char c = path[i - 1];
        if (c == '/' || c == '\\') {
            out = ap_dup_n(path, i - 1);
            break;
        }
    }
    if (!out) out = ap_dup("");
    if (out) {
        char *p;
        for (p = out; *p; p++) {
            if (*p == '\\') *p = '/';
        }
    }
    return out;
}

/* os.path.join with the forward slash the port's Path stand-in renders. */
static char *ap_join3(const char *a, const char *b, const char *c) {
    size_t need = strlen(a) + strlen(b) + strlen(c) + 3;
    char *out = (char *)malloc(need);
    if (!out) return NULL;
    snprintf(out, need, "%s/%s/%s", a, b, c);
    return out;
}

static int ap_in_files(const json_t *files, const char *path) {
    size_t i;
    if (!files || files->type != JSON_ARRAY || !path) return 0;
    for (i = 0; i < files->c.count; i++) {
        const json_t *v = files->c.items[i];
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, path)) return 1;
    }
    return 0;
}

json_t *w21_adapteropu_d51886_bundled_opus_path(const json_t *opus_file,
                                               const json_t *platform,
                                               int ptr_bits,
                                               const json_t *files) {
    const char *plat = "linux";
    const char *file = NULL;
    const char *target;
    char name[64];
    char *dir, *joined;
    json_t *out;

    if (platform && platform->type == JSON_STRING && platform->str_val) plat = platform->str_val;
    if (opus_file && opus_file->type == JSON_STRING && opus_file->str_val) file = opus_file->str_val;

    if (strcmp(plat, "win32") != 0) return json_null();      /* not Windows */
    if (!file || !file[0]) return json_null();               /* no discord.opus.__file__ */

    target = ptr_bits > 32 ? "x64" : "x86";
    snprintf(name, sizeof(name), "libopus-0.%s.dll", target);
    dir = ap_dirname(file);                                  /* Path(...).resolve().parent */
    if (!dir) return json_null();
    joined = ap_join3(dir, "bin", name);
    free(dir);
    if (!joined) return json_null();
    if (!ap_in_files(files, joined)) {                       /* bundled.is_file() */
        free(joined);
        return json_null();
    }
    out = json_string(joined);
    free(joined);
    return out;
}

json_t *w21_adapteropu_d51886_load_opus_codec_plan(const json_t *in) {
    const json_t *platform, *opus_file, *files, *find_library, *script;
    const char *plat = "linux";
    const char *found = NULL;
    char **candidates = NULL;
    size_t ncand = 0, i;
    int ptr_bits = 64, already_loaded = 0, probed = 0;
    int loaded = 0, failures = 0, not_found = 0, early = 0;
    json_t *out;

    if (!in || in->type != JSON_OBJECT) in = json_object();
    platform = json_obj_get(in, "platform");
    opus_file = json_obj_get(in, "opus_file");
    files = json_obj_get(in, "files");
    find_library = json_obj_get(in, "find_library");
    script = json_obj_get(in, "load_script");
    if (platform && platform->type == JSON_STRING && platform->str_val) plat = platform->str_val;
    if (find_library && find_library->type == JSON_STRING && find_library->str_val)
        found = find_library->str_val;
    {
        const json_t *pb = json_obj_get(in, "ptr_bits");
        if (pb && pb->type == JSON_NUMBER) ptr_bits = (int)pb->num_val;
        already_loaded = ap_truthy(json_obj_get(in, "already_loaded"));
        probed = ap_truthy(json_obj_get(in, "probed"));
    }
    loaded = already_loaded;

    out = json_object();
    if (probed || already_loaded) {
        /* `if _probed or discord.opus.is_loaded(): return` */
        early = 1;
        json_set(out, "attempts", json_array());
        json_set(out, "load_failures", json_number(0));
        json_set(out, "not_found_warning", json_bool(0));
        json_set(out, "returned_early", json_bool(1));
        return out;
    }

    candidates = (char **)calloc(8, sizeof(*candidates));
    if (!candidates) {
        json_set(out, "attempts", json_array());
        json_set(out, "load_failures", json_number(0));
        json_set(out, "not_found_warning", json_bool(1));
        json_set(out, "returned_early", json_bool(0));
        return out;
    }

    /* 1. bundled Windows DLL */
    {
        json_t *bundled = w21_adapteropu_d51886_bundled_opus_path(opus_file, platform,
                                                                 ptr_bits, files);
        if (bundled && bundled->type == JSON_STRING && bundled->str_val) {
            candidates[ncand++] = ap_dup(bundled->str_val);
        }
        json_free(bundled);
    }
    /* 2. ctypes.util.find_library("opus") -- else the Homebrew paths on darwin */
    if (found && found[0]) {
        candidates[ncand++] = ap_dup(found);
    } else if (!strcmp(plat, "darwin")) {
        static const char *const HOMEBREW[] = {
            "/opt/homebrew/lib/libopus.dylib",    /* Apple Silicon */
            "/usr/local/lib/libopus.dylib",       /* Intel */
        };
        for (i = 0; i < 2; i++) {
            if (ap_in_files(files, HOMEBREW[i])) {
                candidates[ncand++] = ap_dup(HOMEBREW[i]);
                break;
            }
        }
    }

    for (i = 0; i < ncand; i++) {
        const char *outcome = "no";
        const json_t *step = NULL;
        if (script && script->type == JSON_ARRAY && i < script->c.count) step = script->c.items[i];
        if (step && step->type == JSON_STRING && step->str_val) outcome = step->str_val;
        if (!strcmp(outcome, "fail")) {
            failures++;                       /* `except Exception: logger.warning(...)` */
            continue;
        }
        if (!strcmp(outcome, "ok")) {
            loaded = 1;
            break;
        }
        /* "no": load_opus returned but is_loaded() stayed False */
    }
    if (!loaded) not_found = 1;

    json_set(out, "attempts", json_array());
    {
        json_t *list = json_obj_get(out, "attempts");
        for (i = 0; i < ncand; i++) json_append(list, json_string(candidates[i] ? candidates[i] : ""));
    }
    json_set(out, "load_failures", json_number((double)failures));
    json_set(out, "not_found_warning", json_bool(not_found ? 1 : 0));
    json_set(out, "returned_early", json_bool(early ? 1 : 0));

    for (i = 0; i < ncand; i++) free(candidates[i]);
    free(candidates);
    return out;
}

