/*
 * gateway_windows_legacy.c -- real C11 port of
 * hermes_cli/gateway_windows_legacy.py (gate w23_gatewaywin_830338).
 *
 * PoP: w23_gatewaywin_830338_normalize_path @ hermes_cli/gateway_windows.py:_normalize_windows_path
 * PoP: w23_gatewaywin_830338_targets_this_home @ hermes_cli/gateway_windows_legacy.py:_targets_this_home
 * PoP: w23_gatewaywin_830338_artifacts @ hermes_cli/gateway_windows_legacy.py:legacy_launcher_artifacts
 * PoP: w23_gatewaywin_830338_warn @ hermes_cli/gateway_windows_legacy.py:warn_legacy_launchers
 * PoP: w23_gatewaywin_830338_remove @ hermes_cli/gateway_windows_legacy.py:remove_legacy_launchers
 *
 * The whole ownership decision (which bare names belong to this home, the
 * startup/service pair order, the task XML probe, the unlink/delete reporting)
 * is ported here. Only the OS facts are seams: the home/startup dirs, the task
 * name, the path normalizer, the schtasks calls, the access-denied classifier
 * and pathlib's read/exists/unlink over the fixture's virtual file system.
 *
 * `Path / part` is the module's only path construction; the seam renders it as
 * "base-without-trailing-slash" + "/" + part, and every comparison runs
 * through the same normalizer, so the separator never decides anything.
 */
#include "hermes_json.h"
#include "noc_w23_gatewaywin_830338.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* UTF-8 bytes, so this C source stays ASCII: U+26A0 and U+2713. */
#define GWL830_WARN "\xe2\x9a\xa0 "
#define GWL830_OK "\xe2\x9c\x93 "

#define GWL830_BARE W23_GATEWAYWIN_830338_TASK_NAME_DEFAULT
#define GWL830_MAX_ARTIFACTS 5

static char *gwl830_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python str.strip() over the ASCII whitespace config/paths carry. */
static char *gwl830_strip(const char *s) {
    size_t b = 0, e;
    char *out;
    if (!s) return gwl830_dup("");
    e = strlen(s);
    while (b < e && isspace((unsigned char)s[b])) b++;
    while (e > b && isspace((unsigned char)s[e - 1])) e--;
    out = (char *)malloc(e - b + 1);
    if (!out) return NULL;
    memcpy(out, s + b, e - b);
    out[e - b] = '\0';
    return out;
}

/* `_normalize_windows_path`: strip, drop leading quotes, "/" separators, no
 * trailing separator, casefolded. */
static char *gwl830_norm(const char *value) {
    char *stripped = gwl830_strip(value);
    char *out;
    size_t i, j = 0, n, last;
    if (!stripped) return NULL;
    n = strlen(stripped);
    i = 0;
    while (i < n && stripped[i] == '"') i++;
    out = (char *)malloc(n + 1);
    if (!out) { free(stripped); return NULL; }
    for (; i < n; i++) {
        char c = stripped[i];
        out[j++] = (c == '\\') ? '/' : (char)tolower((unsigned char)c);
    }
    out[j] = '\0';
    last = j;
    while (last > 0 && out[last - 1] == '/') last--;
    out[last] = '\0';
    free(stripped);
    return out;
}

char *w23_gatewaywin_830338_normalize_path(const char *value) {
    return gwl830_norm(value);
}

char *w23_gatewaywin_830338_join_path(const char *base, const char *part) {
    size_t nb, np, cut;
    char *out;
    if (!base) base = "";
    if (!part) part = "";
    nb = strlen(base);
    cut = nb;
    while (cut > 0 && base[cut - 1] == '/') cut--;
    if (cut == 0) return gwl830_dup(part);          /* Path("") / part == part */
    np = strlen(part);
    out = (char *)malloc(cut + 1 + np + 1);
    if (!out) return NULL;
    memcpy(out, base, cut);
    out[cut] = '/';
    memcpy(out + cut + 1, part, np);
    out[cut + 1 + np] = '\0';
    return out;
}

static const json_t *gwl830_obj(const json_t *node) {
    return (node && node->type == JSON_OBJECT) ? node : NULL;
}

/* json object lookup by NORMALIZED key (the fixture phrases paths the way the
 * OS would, so "C:\X\y.vbs" and "c:/x/y.vbs" are one file). */
static const json_t *gwl830_lookup_norm(const json_t *obj, const char *path) {
    const json_t *hit = NULL;
    char *want;
    size_t i;
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    want = gwl830_norm(path);
    if (!want) return NULL;
    for (i = 0; i < json_object_size(obj); i++) {
        const char *key = json_object_get_key_at(obj, i);
        char *got = gwl830_norm(key);
        if (got && !strcmp(got, want)) {
            free(got);
            hit = json_object_get_at(obj, i);
            break;
        }
        free(got);
    }
    free(want);
    return hit;
}

static const char *gwl830_lookup_key_norm(const json_t *obj, const char *path, char *buf,
                                          size_t cap) {
    size_t i;
    char *want;
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    want = gwl830_norm(path);
    if (!want) return NULL;
    for (i = 0; i < json_object_size(obj); i++) {
        const char *key = json_object_get_key_at(obj, i);
        char *got = gwl830_norm(key);
        if (got && !strcmp(got, want)) {
            free(got);
            free(want);
            snprintf(buf, cap, "%s", key ? key : "");
            return buf;
        }
        free(got);
    }
    free(want);
    return NULL;
}

static int gwl830_list_has_norm(const json_t *arr, const char *path) {
    size_t i;
    char *want;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    want = gwl830_norm(path);
    if (!want) return 0;
    for (i = 0; i < json_len(arr); i++) {
        const json_t *it = json_get(arr, i);
        char *got;
        int same;
        if (!it || it->type != JSON_STRING) continue;
        got = gwl830_norm(it->str_val);
        same = got && !strcmp(got, want);
        free(got);
        if (same) { free(want); return 1; }
    }
    free(want);
    return 0;
}

/* `path.read_text(encoding="utf-8-sig", errors="replace")`: the BOM is not
 * part of the text. NULL when the file is absent or its read raises OSError. */
static char *gwl830_read_text(const json_t *world, const char *path) {
    const json_t *files = gwl830_obj(json_obj_get(world, "files"));
    const json_t *node = gwl830_lookup_norm(files, path);
    const char *text;
    if (!node || node->type != JSON_STRING) return NULL;
    if (gwl830_list_has_norm(json_obj_get(world, "unreadable"), path)) return NULL;
    text = node->str_val ? node->str_val : "";
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF) {
        text += 3;
    }
    return gwl830_dup(text);
}

static int gwl830_exists(const json_t *world, const char *path) {
    const json_t *files = gwl830_obj(json_obj_get(world, "files"));
    return gwl830_lookup_norm(files, path) != NULL;
}

int w23_gatewaywin_830338_targets_this_home(const char *text, const char *hermes_home) {
    char *service, *marker, *hay;
    int found;
    service = w23_gatewaywin_830338_join_path(hermes_home, "gateway-service");
    marker = gwl830_norm(service);
    hay = gwl830_norm(text);
    found = (hay && marker && marker[0] && strstr(hay, marker) != NULL);
    free(service);
    free(hay);
    free(marker);
    return found ? 1 : 0;
}

typedef struct {
    const char *kind;
    const char *label;
    char *target;
} gwl830_artifact_t;

/* `legacy_launcher_artifacts()`, in the module's own order. */
static size_t gwl830_collect(const json_t *world_in, gwl830_artifact_t out[GWL830_MAX_ARTIFACTS]) {
    const json_t *world = gwl830_obj(world_in);
    const json_t *task;
    const char *startup, *home;
    const json_t *task_name;
    char *service = NULL, *name_vbs = NULL, *name_cmd = NULL;
    char *path = NULL, *text = NULL;
    size_t n = 0;
    if (!world) return 0;
    /* `get_task_name() == _TASK_NAME_DEFAULT` -> this home owns the bare name;
     * an absent name defaults to the bare one, a non-string never equals it. */
    task_name = json_obj_get(world, "task_name");
    if (!task_name) return 0;
    if (task_name->type == JSON_STRING && !strcmp(task_name->str_val, GWL830_BARE)) return 0;
    startup = json_get_str(world, "startup_dir", "");
    home = json_get_str(world, "hermes_home", "");
    service = w23_gatewaywin_830338_join_path(home, "gateway-service");
    name_vbs = (char *)malloc(strlen(GWL830_BARE) + 5);
    name_cmd = (char *)malloc(strlen(GWL830_BARE) + 5);
    if (!service || !name_vbs || !name_cmd) goto done;
    sprintf(name_vbs, "%s.vbs", GWL830_BARE);
    sprintf(name_cmd, "%s.cmd", GWL830_BARE);

    /* Startup items: OURS only when the action points into this home's
     * gateway-service dir. */
    path = w23_gatewaywin_830338_join_path(startup, name_vbs);
    text = path ? gwl830_read_text(world, path) : NULL;
    if (text && w23_gatewaywin_830338_targets_this_home(text, home)) {
        out[n].kind = "file";
        out[n].label = "legacy pre-suffix Windows login item";
        out[n].target = path;
        n++;
        path = NULL;
    }
    free(text);
    text = NULL;
    free(path);
    path = w23_gatewaywin_830338_join_path(startup, name_cmd);
    text = path ? gwl830_read_text(world, path) : NULL;
    if (text && w23_gatewaywin_830338_targets_this_home(text, home)) {
        out[n].kind = "file";
        out[n].label = "legacy pre-suffix Windows login item";
        out[n].target = path;
        n++;
        path = NULL;
    }
    free(text);
    text = NULL;
    free(path);

    /* gateway-service launchers: inside this home by construction. */
    path = w23_gatewaywin_830338_join_path(service, name_vbs);
    if (path && gwl830_exists(world, path)) {
        out[n].kind = "file";
        out[n].label = "legacy pre-suffix task launcher";
        out[n].target = path;
        n++;
        path = NULL;
    }
    free(path);
    path = w23_gatewaywin_830338_join_path(service, name_cmd);
    if (path && gwl830_exists(world, path)) {
        out[n].kind = "file";
        out[n].label = "legacy pre-suffix task script";
        out[n].target = path;
        n++;
        path = NULL;
    }
    free(path);
    path = NULL;

    /* The Scheduled Task itself, when it also points into this home. */
    task = gwl830_obj(json_obj_get(world, "schtasks"));
    if (task && (long long)json_get_num(task, "code", 1) == 0 &&
        w23_gatewaywin_830338_targets_this_home(json_get_str(task, "stdout", ""), home)) {
        out[n].kind = "task";
        out[n].label = "legacy pre-suffix Scheduled Task";
        out[n].target = gwl830_dup(GWL830_BARE);
        n++;
    }

done:
    free(service);
    free(name_vbs);
    free(name_cmd);
    return n;
}

static void gwl830_free_artifacts(gwl830_artifact_t *list, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) free(list[i].target);
}

/* Python repr() of a str for the task-name reporting. */
static char *gwl830_repr(const char *s) {
    char quote = '\'';
    size_t n = 0, cap, i;
    char *out;
    if (!s) s = "";
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    cap = strlen(s) * 2 + 3;
    out = (char *)malloc(cap);
    if (!out) return NULL;
    out[n++] = quote;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (c == '\\' || c == quote) { out[n++] = '\\'; out[n++] = c; }
        else if (c == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
        else if (c == '\t') { out[n++] = '\\'; out[n++] = 't'; }
        else if (c == '\r') { out[n++] = '\\'; out[n++] = 'r'; }
        else out[n++] = c;
    }
    out[n++] = quote;
    out[n] = '\0';
    return out;
}

static void gwl830_line(json_t *lines, const char *text) {
    json_append(lines, json_string(text ? text : ""));
}

json_t *w23_gatewaywin_830338_artifacts(const json_t *world) {
    gwl830_artifact_t list[GWL830_MAX_ARTIFACTS];
    size_t n, i;
    json_t *out = json_array();
    memset(list, 0, sizeof(list));
    n = gwl830_collect(world, list);
    for (i = 0; i < n; i++) {
        json_t *row = json_array();
        json_append(row, json_string(list[i].kind));
        json_append(row, json_string(list[i].label));
        json_append(row, json_string(list[i].target ? list[i].target : ""));
        json_append(out, row);
    }
    gwl830_free_artifacts(list, n);
    return out;
}

json_t *w23_gatewaywin_830338_warn(const json_t *world) {
    gwl830_artifact_t list[GWL830_MAX_ARTIFACTS];
    size_t n, i;
    json_t *lines = json_array();
    json_t *out = json_object();
    memset(list, 0, sizeof(list));
    n = gwl830_collect(world, list);
    for (i = 0; i < n; i++) {
        const char *label = list[i].label;
        const char *target = list[i].target ? list[i].target : "";
        size_t cap = strlen(label) + strlen(target) + 64;
        char *line = (char *)malloc(cap);
        if (!line) continue;
        snprintf(line, cap, GWL830_WARN "%s still installed: %s", label, target);
        gwl830_line(lines, line);
        free(line);
    }
    if (n > 0) {
        gwl830_line(lines, "  These predate per-profile launcher names and also start the gateway at logon.");
        gwl830_line(lines, "  Remove them with: hermes gateway uninstall   (then: hermes gateway install)");
    }
    json_set(out, "result", json_bool(n > 0));
    json_set(out, "lines", lines);
    gwl830_free_artifacts(list, n);
    return out;
}

/* `Path(target).unlink()` over the fixture's store: the file leaves the store,
 * or the world's recorded OSError text is what the caller sees. */
static int gwl830_unlink(const json_t *world, json_t *store, const char *path, const char **err) {
    const json_t *fails = gwl830_obj(json_obj_get(world, "unlink_fail"));
    const json_t *fail = gwl830_lookup_norm(fails, path);
    char keybuf[512];
    const char *key;
    *err = NULL;
    if (fail && fail->type == JSON_STRING) { *err = fail->str_val; return 0; }
    key = gwl830_lookup_key_norm(store, path, keybuf, sizeof(keybuf));
    if (!key) { *err = "file not found"; return 0; }
    json_obj_del(store, key);
    return 1;
}

static int gwl830_cmp_str(const void *a, const void *b) {
    const char *const *x = (const char *const *)a;
    const char *const *y = (const char *const *)b;
    return strcmp(*x, *y);
}

json_t *w23_gatewaywin_830338_remove(const json_t *world_in) {
    const json_t *world = gwl830_obj(world_in);
    const json_t *files = world ? gwl830_obj(json_obj_get(world, "files")) : NULL;
    gwl830_artifact_t list[GWL830_MAX_ARTIFACTS];
    json_t *lines = json_array();
    json_t *out = json_object();
    json_t *store = files ? json_copy(files) : json_object();
    const json_t *task;
    size_t n, i;
    memset(list, 0, sizeof(list));
    n = gwl830_collect(world, list);
    for (i = 0; i < n; i++) {
        const char *label = list[i].label;
        const char *target = list[i].target ? list[i].target : "";
        char *line;
        size_t cap;
        if (!strcmp(list[i].kind, "file")) {
            const char *err = NULL;
            if (gwl830_unlink(world, store, target, &err)) {
                cap = strlen(label) + strlen(target) + 32;
                line = (char *)malloc(cap);
                if (line) {
                    snprintf(line, cap, GWL830_OK "Removed %s: %s", label, target);
                    gwl830_line(lines, line);
                    free(line);
                }
            } else {
                cap = strlen(label) + strlen(target) + strlen(err ? err : "") + 48;
                line = (char *)malloc(cap);
                if (line) {
                    snprintf(line, cap, GWL830_WARN "Could not remove %s %s: %s", label, target,
                             err ? err : "");
                    gwl830_line(lines, line);
                    free(line);
                }
            }
            continue;
        }
        task = gwl830_obj(json_obj_get(world, "schtasks_delete"));
        if (!task) task = gwl830_obj(json_obj_get(world, "schtasks"));
        {
            long long code = task ? (long long)json_get_num(task, "code", 1) : 1;
            const char *err = task ? json_get_str(task, "stderr", "") : "";
            char *quoted = gwl830_repr(target);
            const char *shown = quoted ? quoted : "\"\"";
            if (code == 0) {
                cap = strlen(label) + strlen(target) + 32;
                line = (char *)malloc(cap);
                if (line) {
                    snprintf(line, cap, GWL830_OK "Removed %s %s", label, shown);
                    gwl830_line(lines, line);
                    free(line);
                }
            } else if (json_get_bool(world, "access_denied", false)) {
                cap = strlen(label) + strlen(target) + 96;
                line = (char *)malloc(cap);
                if (line) {
                    snprintf(line, cap,
                             GWL830_WARN "%s %s needs an elevated shell to remove; run as administrator:",
                             label, shown);
                    gwl830_line(lines, line);
                    free(line);
                }
                cap = strlen(target) + 48;
                line = (char *)malloc(cap);
                if (line) {
                    snprintf(line, cap, "    schtasks /Delete /F /TN %s", target);
                    gwl830_line(lines, line);
                    free(line);
                }
            } else {
                char *stripped = gwl830_strip(err);
                cap = strlen(target) + strlen(stripped ? stripped : "") + 96;
                line = (char *)malloc(cap);
                if (line) {
                    snprintf(line, cap, GWL830_WARN "schtasks /Delete %s returned code %lld: %s",
                             shown, code, stripped ? stripped : "");
                    gwl830_line(lines, line);
                    free(line);
                }
                free(stripped);
            }
            free(quoted);
        }
    }
    {
        size_t count = json_object_size(store);
        json_t *remaining = json_array();
        const char **keys = NULL;
        if (count) {
            keys = (const char **)malloc(count * sizeof(char *));
            if (keys) {
                for (i = 0; i < count; i++) keys[i] = json_object_get_key_at(store, i);
                qsort(keys, count, sizeof(char *), gwl830_cmp_str);
                for (i = 0; i < count; i++) json_append(remaining, json_string(keys[i]));
                free(keys);
            }
        }
        json_set(out, "lines", lines);
        json_set(out, "remaining", remaining);
    }
    json_free(store);
    gwl830_free_artifacts(list, n);
    return out;
}
