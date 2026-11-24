/*
 * cua_backend_capture.c - C11 port of upstream
 * tools/computer_use/cua_backend_capture.py (gate w23_cuabackend_bf7126).
 *
 * PoP: w23_cuabackend_bf7126_self_window @ tools/computer_use/cua_backend_capture.py:_is_cua_driver_self_window
 * PoP: w23_cuabackend_bf7126_desktop_window @ tools/computer_use/cua_backend_capture.py:_is_desktop_window
 * PoP: w23_cuabackend_bf7126_select_target @ tools/computer_use/cua_backend_capture.py:_select_capture_target
 * PoP: w23_cuabackend_bf7126_sorted_windows @ tools/computer_use/cua_backend_capture.py:_sorted_windows
 * PoP: w23_cuabackend_bf7126_png_metrics @ tools/computer_use/cua_backend_capture.py:_png_metrics
 * PoP: w23_cuabackend_bf7126_match_windows @ tools/computer_use/cua_backend_capture.py:_CaptureMixin._match_windows_for_app
 * PoP: w23_cuabackend_bf7126_list_apps @ tools/computer_use/cua_backend_capture.py:_CaptureMixin.list_apps
 * PoP: w23_cuabackend_bf7126_is_full_screen @ tools/computer_use/cua_backend_capture.py:_FULL_SCREEN_SENTINELS
 * PoP: w23_cuabackend_bf7126_is_desktop_shell @ tools/computer_use/cua_backend_capture.py:_DESKTOP_SHELL_SENTINELS
 * PoP: w23_cuabackend_bf7126_no_app_match_message @ tools/computer_use/cua_backend_capture.py:_NO_APP_MATCH_MSG
 * PoP: w23_cuabackend_bf7126_no_desktop_message @ tools/computer_use/cua_backend_capture.py:_NO_DESKTOP_WINDOW_MSG
 * PoP: w23_cuabackend_bf7126_constants @ tools/computer_use/cua_backend_capture.py:_DESKTOP_WINDOW_NAMES
 *
 * `list_windows` / `list_apps` payloads and the xprop active-window probe are
 * the seam: the caller supplies what the driver reported and this file owns
 * discovery, target selection, payload normalisation and app= resolution.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w23_cuabackend_bf7126.h"

#define CU_EMDASH "\xe2\x80\x94" /* U+2014 EM DASH */
#define CU_KANJI1 "\xe8\xa8\x88" /* U+8A08 */
#define CU_KANJI2 "\xe7\xae\x97" /* U+7B97 */
#define CU_KANJI3 "\xe6\xa9\x9f" /* U+6A5F */

static const char *CU_FULL_SCREEN_SENTINELS[] = {"all", "full screen", "fullscreen", "screen", NULL};
static const char *CU_DESKTOP_SHELL_SENTINELS[] = {"desktop", NULL};
static const char *CU_DESKTOP_WINDOW_NAMES[] = {"progman", "workerw", "program manager", "shell_traywnd",
                                                "taskbar", "finder", "desktop", "dock", NULL};
static const char *CU_DESKTOP_BACKDROP_NAMES[] = {"progman", "workerw", "program manager", "finder",
                                                  "desktop", NULL};
static const char *CU_NON_APP_TITLE_PREFIXES[] = {"@!", "Desktop", "gnome-shell", "GNOME Shell", NULL};

#define CU_NO_APP_MATCH_MSG                                                                          \
    "<no on-screen window matched app={app!r}; call list_apps to see available app names or bundle " \
    "IDs (macOS reports localized names, e.g. '" CU_KANJI1 CU_KANJI2 CU_KANJI3 "' instead of "       \
    "'Calculator'; some Linux/Qt apps only resolve via list_apps metadata)>"

#define CU_NO_DESKTOP_WINDOW_MSG                                                                      \
    "<no desktop/shell window found for app={app!r}; cua-driver captures one window at a time and " \
    "exposes no whole-virtual-desktop or per-monitor capture. Call list_apps / "                    \
    "capture(app='<AppName>') to target a specific window instead. On Windows the taskbar is "      \
    "'Shell_TrayWnd' and the desktop is 'Progman'.>"

#define CU_NO_DESKTOP_IMAGE_MSG                                                                        \
    "<get_desktop_state returned no image; the driver may predate the desktop capture lane " CU_EMDASH \
    " try capture(app='<AppName>') for a specific window>"

#define CU_FULL_SCREEN_NOTE                                                                        \
    "full-screen capture has no interactable elements; to act on what you see, call "              \
    "capture(app='<AppName>') for that app's clickable element list, or capture(app='desktop') "   \
    "for the desktop shell (wallpaper icons / taskbar) with elements"

/* ------------------------------------------------------------------ */
/* growable byte buffer                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    char *p;
    size_t n, cap;
} sb_t;

static void sb_init(sb_t *b) {
    b->cap = 128;
    b->n = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void sb_grow(sb_t *b, size_t extra) {
    size_t want;
    char *q;
    if (!b->p) return;
    want = b->n + extra + 1;
    if (want <= b->cap) return;
    while (b->cap < want) b->cap *= 2;
    q = (char *)realloc(b->p, b->cap);
    if (!q) return;
    b->p = q;
}

static void sb_putn(sb_t *b, const char *s, size_t n) {
    if (!b->p || !s) return;
    sb_grow(b, n);
    if (!b->p) return;
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_puts(sb_t *b, const char *s) {
    if (s) sb_putn(b, s, strlen(s));
}

static void sb_putc(sb_t *b, char c) { sb_putn(b, &c, 1); }

static char *sb_take(sb_t *b) {
    char *out = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return out;
}

static char *cu_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* ------------------------------------------------------------------ */
/* Python str() / repr() coercions                                     */
/* ------------------------------------------------------------------ */

static void cu_py_repr_into(sb_t *b, const json_t *v);

static void cu_fmt_num(sb_t *b, double d) {
    char buf[64];
    if (d == (double)(long long)d) {
        snprintf(buf, sizeof(buf), "%lld", (long long)d);
    } else {
        int p;
        buf[0] = '\0';
        for (p = 12; p <= 17; p++) {
            snprintf(buf, sizeof(buf), "%.*g", p, d);
            if (strtod(buf, NULL) == d) break;
        }
        if (!buf[0]) snprintf(buf, sizeof(buf), "%g", d);
    }
    sb_puts(b, buf);
}

static void cu_repr_str_into(sb_t *b, const char *s) {
    sb_putc(b, '\'');
    if (s) {
        for (; *s; s++) {
            unsigned char c = (unsigned char)*s;
            if (c == '\'') sb_puts(b, "\\'");
            else if (c == '\\') sb_puts(b, "\\\\");
            else if (c == '\n') sb_puts(b, "\\n");
            else if (c == '\r') sb_puts(b, "\\r");
            else if (c == '\t') sb_puts(b, "\\t");
            else sb_putc(b, (char)c);
        }
    }
    sb_putc(b, '\'');
}

static void cu_py_repr_into(sb_t *b, const json_t *v) {
    size_t i, n;
    if (!v) {
        sb_puts(b, "None");
        return;
    }
    switch (v->type) {
    case JSON_NULL: sb_puts(b, "None"); break;
    case JSON_BOOL: sb_puts(b, v->bool_val ? "True" : "False"); break;
    case JSON_NUMBER: cu_fmt_num(b, v->num_val); break;
    case JSON_STRING: cu_repr_str_into(b, v->str_val); break;
    case JSON_ARRAY:
        sb_putc(b, '[');
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) sb_puts(b, ", ");
            cu_py_repr_into(b, json_get(v, i));
        }
        sb_putc(b, ']');
        break;
    case JSON_OBJECT:
        sb_putc(b, '{');
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            if (i) sb_puts(b, ", ");
            cu_repr_str_into(b, json_object_get_key_at(v, i));
            sb_puts(b, ": ");
            cu_py_repr_into(b, json_object_get_at(v, i));
        }
        sb_putc(b, '}');
        break;
    default: sb_puts(b, "None"); break;
    }
}

static void cu_py_str_into(sb_t *b, const json_t *v) {
    if (v && v->type == JSON_STRING) {
        sb_puts(b, v->str_val ? v->str_val : "");
        return;
    }
    if (v && v->type == JSON_NUMBER) {
        cu_fmt_num(b, v->num_val);
        return;
    }
    cu_py_repr_into(b, v);
}

static int cu_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY: return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    default: return 0;
    }
}

/* str(w.get(key, default)) */
static char *cu_str_field(const json_t *w, const char *key, const char *dflt) {
    const json_t *v = w && w->type == JSON_OBJECT ? json_obj_get(w, key) : NULL;
    sb_t b;
    sb_init(&b);
    if (!v) sb_puts(&b, dflt);
    else cu_py_str_into(&b, v);
    return sb_take(&b);
}

static char *cu_lower(const char *s) {
    char *out = cu_dup(s);
    char *p;
    if (!out) return NULL;
    for (p = out; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    }
    return out;
}

static int cu_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip() */
static char *cu_strip(const char *s) {
    size_t n, start = 0;
    char *out;
    if (!s) return cu_dup("");
    n = strlen(s);
    while (start < n && cu_is_space(s[start])) start++;
    while (n > start && cu_is_space(s[n - 1])) n--;
    out = (char *)malloc(n - start + 1);
    if (!out) return NULL;
    memcpy(out, s + start, n - start);
    out[n - start] = '\0';
    return out;
}

/* app.strip().lower() */
static char *cu_norm_app(const char *s) {
    char *stripped = cu_strip(s);
    char *lower = cu_lower(stripped);
    free(stripped);
    return lower;
}

/* re.sub(r"[\s_-]+", "", text) */
static char *cu_drop_separators(const char *s) {
    sb_t b;
    sb_init(&b);
    if (!s) return sb_take(&b);
    for (; *s; s++) {
        if (cu_is_space(*s) || *s == '_' || *s == '-') continue;
        sb_putc(&b, *s);
    }
    return sb_take(&b);
}

/* _positive_int: int/int-string > 0, bools and other types rejected. */
static int cu_positive_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL || v->type == JSON_NULL) return 0;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d != (double)(long long)d) return 0; /* a float is not an int in Python */
        if (d <= 0.0) return 0;
        *out = (long long)d;
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        char *end = NULL;
        long long parsed;
        if (!s || !s[0]) return 0;
        parsed = strtoll(s, &end, 10);
        if (!end) return 0;
        while (*end && cu_is_space(*end)) end++;
        if (*end) return 0;
        if (parsed <= 0) return 0;
        *out = parsed;
        return 1;
    }
    return 0;
}

static double cu_num_field(const json_t *w, const char *key, double dflt) {
    const json_t *v = w && w->type == JSON_OBJECT ? json_obj_get(w, key) : NULL;
    if (!v || v->type != JSON_NUMBER) return dflt;
    return v->num_val;
}

static int cu_bool_field(const json_t *w, const char *key) {
    const json_t *v = w && w->type == JSON_OBJECT ? json_obj_get(w, key) : NULL;
    return cu_truthy(v);
}

/* ------------------------------------------------------------------ */
/* window predicates                                                   */
/* ------------------------------------------------------------------ */

int w23_cuabackend_bf7126_self_window(const json_t *w) {
    char *app_name = cu_str_field(w, "app_name", "");
    char *stripped = cu_strip(app_name);
    char *lower = cu_lower(stripped);
    char *flat = cu_drop_separators(lower);
    int same = flat && strcmp(flat, "cuadriver") == 0;
    free(app_name);
    free(stripped);
    free(lower);
    free(flat);
    return same ? 1 : 0;
}

static int cu_title_starts(const char *title, const char *prefix) {
    size_t i;
    if (!title || !prefix) return 0;
    for (i = 0; prefix[i]; i++) {
        char a = title[i];
        char b = prefix[i];
        if (!a) return 0;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static int cu_is_real_app_window(const json_t *w) {
    char *title = cu_str_field(w, "title", "");
    int real = 1;
    int i;
    for (i = 0; CU_NON_APP_TITLE_PREFIXES[i]; i++) {
        if (cu_title_starts(title, CU_NON_APP_TITLE_PREFIXES[i])) {
            real = 0;
            break;
        }
    }
    free(title);
    return real;
}

/* name in f"{app_name} {title}".lower() for the given catalogue */
static int cu_matches_names(const json_t *w, const char **names) {
    char *app_name = cu_str_field(w, "app_name", "");
    char *title = cu_str_field(w, "title", "");
    sb_t joined;
    char *lower;
    int hit = 0, i;
    sb_init(&joined);
    sb_puts(&joined, app_name);
    sb_putc(&joined, ' ');
    sb_puts(&joined, title);
    lower = cu_lower(joined.p);
    free(sb_take(&joined));
    free(app_name);
    free(title);
    for (i = 0; lower && names[i]; i++) {
        if (strstr(lower, names[i])) {
            hit = 1;
            break;
        }
    }
    free(lower);
    return hit;
}

int w23_cuabackend_bf7126_desktop_window(const json_t *w, int backdrop) {
    return cu_matches_names(w, backdrop ? CU_DESKTOP_BACKDROP_NAMES : CU_DESKTOP_WINDOW_NAMES);
}

static int cu_in_set(const json_t *app, const char **set) {
    char *name = NULL;
    int hit = 0, i;
    if (!cu_truthy(app)) return 0;
    {
        sb_t b;
        sb_init(&b);
        cu_py_str_into(&b, app);
        name = cu_norm_app(b.p);
        free(sb_take(&b));
    }
    for (i = 0; name && set[i]; i++) {
        if (strcmp(name, set[i]) == 0) {
            hit = 1;
            break;
        }
    }
    free(name);
    return hit;
}

int w23_cuabackend_bf7126_is_full_screen(const json_t *app) { return cu_in_set(app, CU_FULL_SCREEN_SENTINELS); }

int w23_cuabackend_bf7126_is_desktop_shell(const json_t *app) {
    return cu_in_set(app, CU_DESKTOP_SHELL_SENTINELS);
}

/* ------------------------------------------------------------------ */
/* target selection                                                    */
/* ------------------------------------------------------------------ */

json_t *w23_cuabackend_bf7126_select_target(const json_t *req) {
    const json_t *windows = req && req->type == JSON_OBJECT ? json_obj_get(req, "windows") : NULL;
    const json_t *active = req && req->type == JSON_OBJECT ? json_obj_get(req, "active_id") : NULL;
    const char *platform = req && req->type == JSON_OBJECT ? json_get_str(req, "platform", "") : "";
    int app_requested = req && req->type == JSON_OBJECT ? cu_truthy(json_obj_get(req, "app_requested")) : 0;
    int exact_target = req && req->type == JSON_OBJECT ? cu_truthy(json_obj_get(req, "exact_target")) : 0;
    size_t n, i, npool = 0;
    size_t *pool;
    json_t *chosen;

    if (!windows || windows->type != JSON_ARRAY || json_len(windows) == 0) return json_null();
    n = json_len(windows);
    pool = (size_t *)malloc(n * sizeof(size_t));
    if (!pool) return json_null();
    for (i = 0; i < n; i++) {
        if (!cu_bool_field(json_get(windows, i), "off_screen")) pool[npool++] = i;
    }
    if (!exact_target) {
        size_t kept = 0;
        for (i = 0; i < npool; i++) {
            if (!w23_cuabackend_bf7126_self_window(json_get(windows, pool[i]))) pool[kept++] = pool[i];
        }
        if (kept) npool = kept;
    }
    if (!exact_target && !app_requested && strcmp(platform, "linux") == 0) {
        size_t kept = 0;
        for (i = 0; i < npool; i++) {
            if (cu_is_real_app_window(json_get(windows, pool[i]))) pool[kept++] = pool[i];
        }
        if (kept) npool = kept;
        if (npool) {
            int informative = 0;
            double first_z = cu_num_field(json_get(windows, pool[0]), "z_index", 0.0);
            for (i = 1; i < npool && !informative; i++) {
                if (cu_num_field(json_get(windows, pool[i]), "z_index", 0.0) != first_z) informative = 1;
            }
            if (!informative && active && active->type == JSON_NUMBER) {
                for (i = 0; i < npool; i++) {
                    const json_t *w = json_get(windows, pool[i]);
                    if (cu_num_field(w, "window_id", -1.0) == active->num_val) {
                        chosen = json_copy(w);
                        free(pool);
                        return chosen;
                    }
                }
            }
        }
    }
    chosen = json_copy(json_get(windows, npool ? pool[0] : 0));
    free(pool);
    return chosen;
}

/* ------------------------------------------------------------------ */
/* list_windows payload normalisation                                  */
/* ------------------------------------------------------------------ */

/* _windows_from_tool_result */
static const json_t *cu_windows_from_payload(const json_t *out) {
    const json_t *structured, *data;
    static const char *keys[2] = {"windows", "_legacy_windows"};
    int k;
    if (!out || out->type != JSON_OBJECT) return NULL;
    structured = json_obj_get(out, "structuredContent");
    if (structured && structured->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(structured, "windows");
        if (v && v->type == JSON_ARRAY && json_len(v)) return v;
    }
    data = json_obj_get(out, "data");
    if (data && data->type == JSON_OBJECT) {
        for (k = 0; k < 2; k++) {
            const json_t *v = json_obj_get(data, keys[k]);
            if (v && v->type == JSON_ARRAY && json_len(v)) return v;
        }
    }
    for (k = 0; k < 2; k++) {
        const json_t *v = json_obj_get(out, keys[k]);
        if (v && v->type == JSON_ARRAY && json_len(v)) return v;
    }
    return NULL;
}

/* _ingest_windows: one normalised row, or NULL when the row is unusable. */
static json_t *cu_ingest_one(const json_t *w) {
    long long pid, window_id;
    json_t *row;
    if (!w || w->type != JSON_OBJECT) return NULL;
    if (!cu_positive_int(json_obj_get(w, "pid"), &pid)) return NULL;
    if (!cu_positive_int(json_obj_get(w, "window_id"), &window_id)) return NULL;
    row = json_object();
    {
        char *app_name = cu_str_field(w, "app_name", "");
        char *title = cu_str_field(w, "title", "");
        const json_t *screen = json_obj_get(w, "is_on_screen");
        json_set(row, "app_name", json_string(app_name));
        json_set(row, "pid", json_number((double)pid));
        json_set(row, "window_id", json_number((double)window_id));
        json_set(row, "off_screen", json_bool(screen && screen->type == JSON_BOOL && !screen->bool_val));
        json_set(row, "title", json_string(title));
        free(app_name);
        free(title);
    }
    {
        const json_t *z = json_obj_get(w, "z_index");
        double vz = (z && z->type == JSON_NUMBER) ? z->num_val : 0.0;
        json_set(row, "z_index", json_number(vz));
    }
    return row;
}

json_t *w23_cuabackend_bf7126_sorted_windows(const json_t *out) {
    const json_t *raw = cu_windows_from_payload(out);
    json_t *rows = json_array();
    json_t *result = json_array();
    size_t n, i, j;

    if (!raw) return result;
    n = json_len(raw);
    for (i = 0; i < n; i++) {
        json_t *row = cu_ingest_one(json_get(raw, i));
        if (row) json_append(rows, row);
    }
    {
        size_t count = json_len(rows);
        json_t **order = (json_t **)malloc((count ? count : 1) * sizeof(json_t *));
        if (!order) {
            json_free(rows);
            return result;
        }
        for (i = 0; i < count; i++) order[i] = json_get(rows, i);
        for (i = 1; i < count; i++) {
            json_t *held = order[i];
            double hz = cu_num_field(held, "z_index", 0.0);
            j = i;
            while (j > 0 && cu_num_field(order[j - 1], "z_index", 0.0) < hz) {
                order[j] = order[j - 1];
                j--;
            }
            order[j] = held;
        }
        for (i = 0; i < count; i++) json_append(result, json_copy(order[i]));
        free(order);
    }
    json_free(rows);
    return result;
}

/* ------------------------------------------------------------------ */
/* base64 + PNG/JPEG header sniff                                      */
/* ------------------------------------------------------------------ */

static int cu_b64_val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* base64.b64decode(s, validate=False). */
static int cu_b64_decode(const char *s, unsigned char **out, size_t *out_len) {
    size_t n, i, k = 0, full, rem, oi = 0;
    int pad = 0;
    unsigned char *sextets, *decoded;

    *out = NULL;
    *out_len = 0;
    if (!s) return 0;
    n = strlen(s);
    sextets = (unsigned char *)malloc(n + 4);
    if (!sextets) return 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        int val = cu_b64_val(c);
        if (c == '=') {
            pad++;
            continue;
        }
        if (val < 0) continue;
        sextets[k++] = (unsigned char)val;
    }
    if (k % 4 == 1 || (k % 4 != 0 && pad == 0)) {
        free(sextets);
        return 0;
    }
    full = k / 4;
    rem = k % 4;
    decoded = (unsigned char *)malloc(full * 3 + 3);
    if (!decoded) {
        free(sextets);
        return 0;
    }
    for (i = 0; i < full; i++) {
        unsigned v = ((unsigned)sextets[i * 4] << 18) | ((unsigned)sextets[i * 4 + 1] << 12) |
                     ((unsigned)sextets[i * 4 + 2] << 6) | (unsigned)sextets[i * 4 + 3];
        decoded[oi++] = (unsigned char)((v >> 16) & 0xFF);
        decoded[oi++] = (unsigned char)((v >> 8) & 0xFF);
        decoded[oi++] = (unsigned char)(v & 0xFF);
    }
    if (rem == 2) {
        unsigned v = ((unsigned)sextets[full * 4] << 18) | ((unsigned)sextets[full * 4 + 1] << 12);
        decoded[oi++] = (unsigned char)((v >> 16) & 0xFF);
    } else if (rem == 3) {
        unsigned v = ((unsigned)sextets[full * 4] << 18) | ((unsigned)sextets[full * 4 + 1] << 12) |
                     ((unsigned)sextets[full * 4 + 2] << 6);
        decoded[oi++] = (unsigned char)((v >> 16) & 0xFF);
        decoded[oi++] = (unsigned char)((v >> 8) & 0xFF);
    }
    free(sextets);
    *out = decoded;
    *out_len = oi;
    return 1;
}

static int cu_jpeg_sof(unsigned marker) {
    switch (marker) {
    case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC5: case 0xC6: case 0xC7:
    case 0xC9: case 0xCA: case 0xCB: case 0xCD: case 0xCE: case 0xCF:
        return 1;
    default: return 0;
    }
}

/* image_dimensions_from_bytes: PNG IHDR / first JPEG SOF marker. */
static void cu_image_dims(const unsigned char *raw, size_t len, long long *w, long long *h) {
    *w = 0;
    *h = 0;
    if (len >= 24 && memcmp(raw, "\x89PNG\r\n\x1a\n", 8) == 0) {
        *w = ((long long)raw[16] << 24) | ((long long)raw[17] << 16) | ((long long)raw[18] << 8) | raw[19];
        *h = ((long long)raw[20] << 24) | ((long long)raw[21] << 16) | ((long long)raw[22] << 8) | raw[23];
        return;
    }
    if (len > 4 && raw[0] == 0xFF && raw[1] == 0xD8) {
        size_t i = 2;
        while (i + 9 < len) {
            unsigned marker;
            size_t seg;
            if (raw[i] != 0xFF) {
                i++;
                continue;
            }
            marker = raw[i + 1];
            i += 2;
            while (marker == 0xFF && i < len) {
                marker = raw[i];
                i++;
            }
            if (marker == 0xD8 || marker == 0xD9) continue;
            if (marker == 0xDA || i + 2 > len) break;
            seg = ((size_t)raw[i] << 8) | raw[i + 1];
            if (seg < 2 || i + seg > len) break;
            if (cu_jpeg_sof(marker) && seg >= 7) {
                *w = ((long long)raw[i + 5] << 8) | raw[i + 6];
                *h = ((long long)raw[i + 3] << 8) | raw[i + 4];
                return;
            }
            i += seg;
        }
    }
}

json_t *w23_cuabackend_bf7126_png_metrics(const json_t *req) {
    const char *b64 = req && req->type == JSON_OBJECT ? json_get_str(req, "png_b64", "") : "";
    long long width = (long long)cu_num_field(req, "width", 0.0);
    long long height = (long long)cu_num_field(req, "height", 0.0);
    unsigned char *raw = NULL;
    size_t raw_len = 0;
    json_t *out = json_array();

    if (!cu_b64_decode(b64, &raw, &raw_len)) {
        json_append(out, json_number((double)((long long)strlen(b64) * 3 / 4)));
        json_append(out, json_number((double)width));
        json_append(out, json_number((double)height));
        return out;
    }
    {
        long long w = 0, h = 0;
        cu_image_dims(raw, raw_len, &w, &h);
        json_append(out, json_number((double)raw_len));
        if (w > 0 && h > 0) {
            json_append(out, json_number((double)w));
            json_append(out, json_number((double)h));
        } else {
            json_append(out, json_number((double)width));
            json_append(out, json_number((double)height));
        }
    }
    free(raw);
    return out;
}

/* ------------------------------------------------------------------ */
/* app= resolution                                                     */
/* ------------------------------------------------------------------ */

/* _name(w) = str(w.get("app_name", "")).lower() */
static char *cu_window_name(const json_t *w) {
    char *name = cu_str_field(w, "app_name", "");
    char *lower = cu_lower(name);
    free(name);
    return lower;
}

static int cu_pid_of(const json_t *w, long long *pid) {
    return cu_positive_int(w && w->type == JSON_OBJECT ? json_obj_get(w, "pid") : NULL, pid);
}

static int cu_in_pids(const long long *pids, size_t n, long long value) {
    size_t i;
    for (i = 0; i < n; i++)
        if (pids[i] == value) return 1;
    return 0;
}

static void cu_push_pid(long long **pids, size_t *n, size_t *cap, long long pid) {
    if (*n + 2 > *cap) {
        long long *nq;
        *cap = *cap ? *cap * 2 : 8;
        nq = (long long *)realloc(*pids, *cap * sizeof(long long));
        if (!nq) return;
        *pids = nq;
    }
    (*pids)[(*n)++] = pid;
}

json_t *w23_cuabackend_bf7126_match_windows(const json_t *req) {
    const json_t *windows = req && req->type == JSON_OBJECT ? json_obj_get(req, "windows") : NULL;
    const json_t *apps = req && req->type == JSON_OBJECT ? json_obj_get(req, "apps") : NULL;
    const char *app_raw = req && req->type == JSON_OBJECT ? json_get_str(req, "app", "") : "";
    char *app_lower;
    json_t *result = json_array();
    size_t n, i;
    long long *exact_pids = NULL, *partial_pids = NULL;
    size_t exact_n = 0, partial_n = 0, exact_cap = 0, partial_cap = 0;

    if (!windows || windows->type != JSON_ARRAY) return result;
    n = json_len(windows);
    app_lower = cu_norm_app(app_raw);

    if (!app_lower || !app_lower[0]) {
        free(app_lower);
        return result;
    }
    /* tier 0: exact app_name match */
    for (i = 0; i < n; i++) {
        char *name = cu_window_name(json_get(windows, i));
        char *stripped = cu_strip(name);
        if (strcmp(app_lower, stripped) == 0) json_append(result, json_copy(json_get(windows, i)));
        free(name);
        free(stripped);
    }
    if (json_len(result)) {
        free(app_lower);
        return result;
    }

    /* list_apps alias scan (the enumeration seam) */
    if (apps && apps->type == JSON_ARRAY) {
        size_t an = json_len(apps), k;
        for (k = 0; k < an; k++) {
            const json_t *raw_app = json_get(apps, k);
            const json_t *running;
            long long pid;
            int exact = 0, partial = 0;
            char *aliases[5];
            int n_aliases = 0, a;
            static const char *keys[5] = {"bundle_id", "bundleId", "name", "app_name", "display_name"};
            if (!raw_app || raw_app->type != JSON_OBJECT) continue;
            if (!cu_positive_int(json_obj_get(raw_app, "pid"), &pid)) continue;
            running = json_obj_get(raw_app, "running");
            if (running && running->type == JSON_BOOL && !running->bool_val) continue;
            for (a = 0; a < 5; a++) {
                const json_t *v = json_obj_get(raw_app, keys[a]);
                char *stripped, *lower;
                if (!v || v->type != JSON_STRING) continue;
                stripped = cu_strip(v->str_val);
                if (!stripped || !stripped[0]) {
                    free(stripped);
                    continue;
                }
                lower = cu_lower(stripped);
                free(stripped);
                if (lower && n_aliases < 5) aliases[n_aliases++] = lower;
            }
            for (a = 0; a < n_aliases; a++) {
                if (strcmp(app_lower, aliases[a]) == 0) exact = 1;
                else if (strstr(aliases[a], app_lower)) partial = 1;
            }
            for (a = 0; a < n_aliases; a++) free(aliases[a]);
            if (exact) cu_push_pid(&exact_pids, &exact_n, &exact_cap, pid);
            else if (partial) cu_push_pid(&partial_pids, &partial_n, &partial_cap, pid);
        }
    }

    /* tier 1: pid in exact_pids */
    for (i = 0; i < n; i++) {
        long long pid;
        if (cu_pid_of(json_get(windows, i), &pid) && cu_in_pids(exact_pids, exact_n, pid))
            json_append(result, json_copy(json_get(windows, i)));
    }
    /* tier 2: app_lower in app_name */
    if (!json_len(result)) {
        for (i = 0; i < n; i++) {
            char *name = cu_window_name(json_get(windows, i));
            if (strstr(name, app_lower)) json_append(result, json_copy(json_get(windows, i)));
            free(name);
        }
    }
    /* tier 3: pid in partial_pids */
    if (!json_len(result)) {
        for (i = 0; i < n; i++) {
            long long pid;
            if (cu_pid_of(json_get(windows, i), &pid) && cu_in_pids(partial_pids, partial_n, pid))
                json_append(result, json_copy(json_get(windows, i)));
        }
    }
    /* tier 4: nameless row whose title carries the app */
    if (!json_len(result)) {
        for (i = 0; i < n; i++) {
            const json_t *w = json_get(windows, i);
            char *name = cu_window_name(w);
            char *stripped = cu_strip(name);
            if (!stripped[0]) {
                char *title = cu_str_field(w, "title", "");
                char *lower = cu_lower(title);
                if (strstr(lower, app_lower)) json_append(result, json_copy(w));
                free(title);
                free(lower);
            }
            free(name);
            free(stripped);
        }
    }
    free(exact_pids);
    free(partial_pids);
    free(app_lower);
    return result;
}

/* _apps_from_windows */
static json_t *cu_apps_from_windows(const json_t *raw) {
    json_t *apps = json_array();
    size_t i, n;
    if (!raw || raw->type != JSON_ARRAY) return apps;
    n = json_len(raw);
    for (i = 0; i < n; i++) {
        json_t *row = cu_ingest_one(json_get(raw, i));
        const char *name;
        long long pid;
        size_t k;
        int seen = 0;
        if (!row) continue;
        name = json_get_str(row, "app_name", "");
        pid = (long long)json_get_num(row, "pid", 0);
        if (name && name[0]) {
            for (k = 0; k < json_len(apps); k++) {
                const json_t *prev = json_get(apps, k);
                if (prev && strcmp(json_get_str(prev, "name", ""), name) == 0 &&
                    (long long)json_get_num(prev, "pid", 0) == pid) {
                    seen = 1;
                    break;
                }
            }
            if (!seen) {
                json_t *entry = json_object();
                json_set(entry, "name", json_string(name));
                json_set(entry, "pid", json_number((double)pid));
                json_append(apps, entry);
            }
        }
        json_free(row);
    }
    return apps;
}

/* "(name) (pid N)" text fallback, one app per line. */
static void cu_apps_from_text(const char *data, json_t *out) {
    const char *line = data;
    if (!data) return;
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) : strlen(line);
        char *buf = (char *)malloc(len + 1);
        if (buf) {
            const char *cursor;
            memcpy(buf, line, len);
            buf[len] = '\0';
            cursor = buf;
            while ((cursor = strstr(cursor, "(pid")) != NULL) {
                const char *ws = cursor;
                const char *digits;
                const char *close;
                while (ws > buf && cu_is_space(ws[-1])) ws--;
                if (ws == buf) {
                    cursor += 4;
                    continue;
                }
                digits = cursor + 4;
                while (*digits && cu_is_space(*digits)) digits++;
                close = digits;
                while (*close >= '0' && *close <= '9') close++;
                if (close != digits && *close == ')') {
                    size_t name_end = (size_t)(ws - buf);
                    char *name_src = (char *)malloc(name_end + 1);
                    if (name_src) {
                        char *name;
                        size_t dl = (size_t)(close - digits);
                        memcpy(name_src, buf, name_end);
                        name_src[name_end] = '\0';
                        name = cu_strip(name_src);
                        free(name_src);
                        if (dl > 0 && dl < 32) {
                            char num[32];
                            json_t *entry = json_object();
                            memcpy(num, digits, dl);
                            num[dl] = '\0';
                            json_set(entry, "name", json_string(name ? name : ""));
                            json_set(entry, "pid", json_number((double)strtoll(num, NULL, 10)));
                            json_append(out, entry);
                        }
                        free(name);
                    }
                    break;
                }
                cursor += 4;
            }
            free(buf);
        }
        if (!end) break;
        line = end + 1;
    }
}

json_t *w23_cuabackend_bf7126_list_apps(const json_t *payload) {
    const json_t *structured =
        payload && payload->type == JSON_OBJECT ? json_obj_get(payload, "structuredContent") : NULL;
    const json_t *data = payload && payload->type == JSON_OBJECT ? json_obj_get(payload, "data") : NULL;
    const json_t *cand;
    json_t *out;

    if (structured && structured->type == JSON_OBJECT) {
        cand = json_obj_get(structured, "apps");
        if (cand && cand->type == JSON_ARRAY && json_len(cand)) return json_copy(cand);
    }
    if (data && data->type == JSON_ARRAY && json_len(data)) return json_copy(data);
    if (data && data->type == JSON_OBJECT) {
        cand = json_obj_get(data, "apps");
        if (cand && cand->type == JSON_ARRAY && json_len(cand)) return json_copy(cand);
    }
    if (payload && payload->type == JSON_OBJECT) {
        cand = json_obj_get(payload, "apps");
        if (cand && cand->type == JSON_ARRAY && json_len(cand)) return json_copy(cand);
    }
    out = cu_apps_from_windows(cu_windows_from_payload(payload));
    if (json_len(out)) return out;
    json_free(out);
    out = json_array();
    if (data && data->type == JSON_STRING) cu_apps_from_text(data->str_val, out);
    return out;
}

/* ------------------------------------------------------------------ */
/* refusal prose + catalogues                                          */
/* ------------------------------------------------------------------ */

static char *cu_format_app(const char *tmpl, const json_t *app) {
    char *filled = cu_dup(tmpl);
    char *slot;
    if (!filled) return NULL;
    slot = strstr(filled, "{app!r}");
    if (!slot) return filled;
    {
        sb_t b;
        sb_init(&b);
        sb_putn(&b, filled, (size_t)(slot - filled));
        cu_repr_str_into(&b, app && app->type == JSON_STRING ? app->str_val : NULL);
        sb_puts(&b, slot + 7);
        free(filled);
        return sb_take(&b);
    }
}

char *w23_cuabackend_bf7126_no_app_match_message(const json_t *app) {
    return cu_format_app(CU_NO_APP_MATCH_MSG, app);
}

char *w23_cuabackend_bf7126_no_desktop_message(const json_t *app) {
    return cu_format_app(CU_NO_DESKTOP_WINDOW_MSG, app);
}

static void cu_put_list(json_t *o, const char *key, const char **items) {
    json_t *arr = json_array();
    int i;
    for (i = 0; items[i]; i++) json_append(arr, json_string(items[i]));
    json_set(o, key, arr);
}

json_t *w23_cuabackend_bf7126_constants(void) {
    json_t *o = json_object();
    cu_put_list(o, "full_screen_sentinels", CU_FULL_SCREEN_SENTINELS);
    cu_put_list(o, "desktop_shell_sentinels", CU_DESKTOP_SHELL_SENTINELS);
    cu_put_list(o, "desktop_window_names", CU_DESKTOP_WINDOW_NAMES);
    cu_put_list(o, "desktop_backdrop_names", CU_DESKTOP_BACKDROP_NAMES);
    cu_put_list(o, "non_app_title_prefixes", CU_NON_APP_TITLE_PREFIXES);
    json_set(o, "no_app_match_msg", json_string(CU_NO_APP_MATCH_MSG));
    json_set(o, "no_desktop_window_msg", json_string(CU_NO_DESKTOP_WINDOW_MSG));
    json_set(o, "no_desktop_image_msg", json_string(CU_NO_DESKTOP_IMAGE_MSG));
    json_set(o, "full_screen_note", json_string(CU_FULL_SCREEN_NOTE));
    return o;
}
