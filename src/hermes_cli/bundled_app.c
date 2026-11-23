/*
 * bundled_app.c — C port of upstream Hermes hermes_cli/bundled_app.py.
 *
 * PoP: w8_bundledapp_aeb15b_only @ hermes_cli/bundled_app.py:_only
 * PoP: w8_bundledapp_aeb15b_launcher @ hermes_cli/bundled_app.py:_resolve_launcher
 * PoP: w8_bundledapp_aeb15b_layout @ hermes_cli/bundled_app.py:resolve_bundle_layout
 *
 * Faithful port of the pure layout/launcher decision: path parent/name/suffix
 * arithmetic, the agent-payload containment check, the macOS .app offset
 * detection, the three platform launcher-naming rules, and _only's
 * "single candidate" rule. Filesystem listing/is_dir/executability are
 * injected through the fixture (the oracle stubs _files, Path and os.access).
 */
#include "noc_w8_bundledapp_aeb15b.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void split_path(const char *p, char segs[][256], size_t *n, size_t max) {
    *n = 0;
    if (!p) return;
    const char *q = p;
    while (*q && *n < max) {
        while (*q == '/') q++;
        if (!*q) break;
        const char *start = q;
        while (*q && *q != '/') q++;
        size_t len = (size_t)(q - start);
        if (len >= 256) len = 255;
        memcpy(segs[*n], start, len);
        segs[*n][len] = '\0';
        (*n)++;
    }
}

static void parent_path(const char *p, char *out, size_t n) {
    char segs[32][256];
    size_t cnt = 0;
    split_path(p, segs, &cnt, 32);
    if (cnt <= 1) { snprintf(out, n, "/"); return; }
    out[0] = '\0';
    for (size_t i = 0; i + 1 < cnt; i++) {
        strncat(out, "/", n - strlen(out) - 1);
        strncat(out, segs[i], n - strlen(out) - 1);
    }
}

static const char *name_ptr(const char *p) {
    if (!p) return "";
    const char *last = strrchr(p, '/');
    return last ? last + 1 : p;
}

static void suffix_of(const char *name, char *out, size_t n) {
    out[0] = '\0';
    const char *dot = strrchr(name, '.');
    if (dot && dot > name && dot[1]) snprintf(out, n, "%s", dot);
}

static int is_linux_non_launcher(const char *name) {
    return !strcmp(name, "AppRun") || !strcmp(name, "chrome-sandbox") ||
           !strcmp(name, "chrome_crashpad_handler");
}

struct json_t *w8_bundledapp_aeb15b_only(const struct json_t *candidates) {
    if (candidates && candidates->type == JSON_ARRAY && json_len(candidates) == 1)
        return json_copy(json_get(candidates, 0));
    return json_null();
}

struct json_t *w8_bundledapp_aeb15b_launcher(const struct json_t *files, const char *platform) {
    struct json_t *candidates = json_array();
    size_t n = (files && files->type == JSON_ARRAY) ? json_len(files) : 0;
    for (size_t i = 0; i < n; i++) {
        struct json_t *f = json_get(files, i);
        if (!f || f->type != JSON_OBJECT) continue;
        const char *path = json_get_str(f, "path", "");
        const char *name = name_ptr(path);
        int exec = json_get_bool(f, "exec", false) ? 1 : 0;
        int keep = 0;
        if (!strcmp(platform, "darwin")) {
            keep = 1;
        } else if (!strcmp(platform, "win32")) {
            char suf[32]; suffix_of(name, suf, sizeof(suf));
            keep = !strcasecmp(suf, ".exe") && strncasecmp(name, "uninstall", 9) != 0;
        } else {
            keep = !is_linux_non_launcher(name) && strstr(name, ".so") == NULL && exec;
        }
        if (keep) json_append(candidates, json_string(path));
    }
    struct json_t *out = w8_bundledapp_aeb15b_only(candidates);
    json_free(candidates);
    return out;
}

static json_t *layout_fail(const char *code) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string(code));
    return o;
}

struct json_t *w8_bundledapp_aeb15b_layout(const char *project_root, const char *platform,
                                           const struct json_t *files, int app_root_is_dir) {
    char payload[1024], resources[1024], app_root[1024], app_parent[1024];
    parent_path(project_root, payload, sizeof(payload));
    if (strcmp(name_ptr(payload), "agent-payload"))
        return layout_fail("not_bundled");

    parent_path(payload, resources, sizeof(resources));
    parent_path(resources, app_root, sizeof(app_root));

    char res_name[256], ar_name[256], ar_parent[1024], ar_parent_suffix[64];
    snprintf(res_name, sizeof(res_name), "%s", name_ptr(resources));
    snprintf(ar_name, sizeof(ar_name), "%s", name_ptr(app_root));
    parent_path(app_root, ar_parent, sizeof(ar_parent));
    suffix_of(name_ptr(ar_parent), ar_parent_suffix, sizeof(ar_parent_suffix));
    if (!strcmp(res_name, "Resources") && !strcmp(ar_name, "Contents") &&
        !strcmp(ar_parent_suffix, ".app")) {
        snprintf(app_root, sizeof(app_root), "%s", ar_parent);
    }

    if (!app_root_is_dir) return layout_fail("no_app_dir");

    json_t *launcher = w8_bundledapp_aeb15b_launcher(files, platform);
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "app_root", json_string(app_root));
    json_set(o, "resources", json_string(resources));
    json_set(o, "payload", json_string(payload));
    json_set(o, "launcher", launcher);
    return o;
}
