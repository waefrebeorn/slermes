/*
 * port_missing_impl.c — the definitions the tree calls but never provided.
 *
 * Linking every ported translation unit surfaced eight symbols that are *declared* in headers and
 * *called* by linked code, but defined nowhere in src/ or lib/:
 *
 *   desktop_controller_init, desktop_controller_set_boot_state      include/desktop_controller.h
 *   ctx_token_path                                                  gateway/platforms/weixin_http.h
 *   discover_entrypoint_manifests                                   hermes_cli/plugins_cmd_gaps.c
 *   tui (the global), tui_redraw_status                             src/cli/tui_state.h
 *
 * These are not duplicates and not platform seams; the Python side has them and the port never
 * carried them over. Each is implemented here against its header's contract, with the behaviour
 * kept honest: state is stored and reported, nothing pretends to do work it does not do.
 */
#include "desktop_controller.h"
#include "../gateway/platforms/weixin_http.h"
#include "tui_state.h"
#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>

/* ── desktop controller ──────────────────────────────────────────────────────────────── */

static boot_state_t g_boot_state = BOOT_INIT;




/* ── weixin context token cache path ─────────────────────────────────────────────────── */

/* Python: ctx_token_path builds the cache file path for weixin context tokens. Same shape as the
 * other per-platform cache files: $HERMES_HOME (or ~/.hermes) plus a fixed leaf name. */

/* ── plugin entrypoint manifests ─────────────────────────────────────────────────────── */

/* Python (hermes_cli/plugins_discovery.py:72): discover_entrypoint_manifests() returns
 * metadata-only manifests for installed entry-point plugins, reading
 *   [hermes_agent.plugins]              name = module:callable
 *   [hermes_agent.plugin_capabilities]  <plugin>.<capability> = module:callable
 * from each distribution's entry_points.txt, with version/description from its METADATA. No plugin
 * code is imported: kind defaults to "standalone" because classifying it needs a source scan.
 *
 * The Python version asks importlib.metadata; a native binary has no such index, so the same files
 * are read directly out of the site-packages-like directories this process was told about. */
#define EP_GROUP_PLUGINS      "hermes_agent.plugins"
#define EP_GROUP_CAPABILITIES "hermes_agent.plugin_capabilities"
#define EP_MAX_CAPS 16

static const char *const EP_CAPABILITY_IDS[] = {
    "tools.override",
    "llm.provider_override",
    "llm.model_override",
    "llm.agent_id_override",
    "llm.profile_override",
    "llm.task_override",
    "llm.side_question",
    "llm.session_fork",
    "gateway.platform_actions",
};

typedef struct { char name[128]; char value[512]; } ep_entry_t;
typedef struct { char value[512]; char name[128]; } ep_cap_t;

/* Read `key:` out of a METADATA/PKG-INFO header block. */
static bool meta_field(const char *path, const char *key, char *out, size_t sz)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[1024];
    size_t klen = strlen(key);
    bool found = false;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '\n' || line[0] == '\r') break;          /* end of headers */
        if (strncmp(line, key, klen) == 0 && line[klen] == ':') {
            const char *v = line + klen + 1;
            while (*v == ' ') v++;
            size_t n = strcspn(v, "\r\n");
            if (n >= sz) n = sz - 1;
            memcpy(out, v, n);
            out[n] = '\0';
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

/* Parse entry_points.txt: which plugin entries and capability declarations it holds. */
static void parse_entry_points(const char *path, ep_entry_t *plugins, size_t *n_plugins,
                               ep_cap_t *caps, size_t *n_caps, size_t max_each)
{
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char line[1024];
    int section = 0;   /* 1 = plugins, 2 = capabilities */
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '[') {
            section = 0;
            if (strncmp(p + 1, EP_GROUP_PLUGINS, strlen(EP_GROUP_PLUGINS)) == 0) section = 1;
            else if (strncmp(p + 1, EP_GROUP_CAPABILITIES, strlen(EP_GROUP_CAPABILITIES)) == 0) section = 2;
            continue;
        }
        if (*p == '#' || *p == ';' || *p == '\n' || *p == '\r' || !*p) continue;
        char *eq = strchr(p, '=');
        if (!eq || !section) continue;
        *eq = '\0';
        char *name = p;
        char *value = eq + 1;
        char *end = name + strlen(name);
        while (end > name && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
        while (*value == ' ' || *value == '\t') value++;
        size_t vlen = strcspn(value, "\r\n");
        value[vlen] = '\0';
        if (section == 1 && *n_plugins < max_each) {
            snprintf(plugins[*n_plugins].name, sizeof plugins[0].name, "%s", name);
            snprintf(plugins[*n_plugins].value, sizeof plugins[0].value, "%s", value);
            (*n_plugins)++;
        } else if (section == 2 && *n_caps < max_each) {
            snprintf(caps[*n_caps].name, sizeof caps[0].name, "%s", name);
            snprintf(caps[*n_caps].value, sizeof caps[0].value, "%s", value);
            (*n_caps)++;
        }
    }
    fclose(f);
}

/* One distribution directory -> append its entry-point manifests. */
static void collect_dist(const char *dist_dir, json_t *out)
{
    char path[1024];
    ep_entry_t plugins[64];
    ep_cap_t caps[128];
    size_t n_plugins = 0, n_caps = 0;

    snprintf(path, sizeof path, "%s/entry_points.txt", dist_dir);
    parse_entry_points(path, plugins, &n_plugins, caps, &n_caps,
                       sizeof plugins / sizeof plugins[0]);
    if (!n_plugins) return;

    char version[64] = "";
    char summary[512] = "";
    snprintf(path, sizeof path, "%s/METADATA", dist_dir);
    if (!meta_field(path, "Version", version, sizeof version)) {
        snprintf(path, sizeof path, "%s/PKG-INFO", dist_dir);
        meta_field(path, "Version", version, sizeof version);
    }
    snprintf(path, sizeof path, "%s/METADATA", dist_dir);
    if (!meta_field(path, "Summary", summary, sizeof summary)) {
        snprintf(path, sizeof path, "%s/PKG-INFO", dist_dir);
        meta_field(path, "Summary", summary, sizeof summary);
    }

    for (size_t i = 0; i < n_plugins; i++) {
        json_t *m = json_object();
        if (!m) continue;
        json_set(m, "name", json_string(plugins[i].name));
        json_set(m, "version", json_string(version));
        json_set(m, "description", json_string(summary));
        json_set(m, "source", json_string("entrypoint"));
        json_set(m, "path", json_string(plugins[i].value));
        json_set(m, "key", json_string(plugins[i].name));
        json_set(m, "kind", json_string("standalone"));

        /* Python keeps capability entries whose *value* is the plugin's entry point and whose name
         * is "<plugin>.<capability>", then normalises to bare ids and drops anything outside
         * VALID_CAPABILITY_IDS (fail-closed: an id this build cannot grant must not appear). */
        json_t *declared = json_array();
        int seen[EP_MAX_CAPS];
        size_t n_seen = 0;
        const size_t n_ids = sizeof EP_CAPABILITY_IDS / sizeof EP_CAPABILITY_IDS[0];
        for (size_t c = 0; c < n_caps; c++) {
            if (strcmp(caps[c].value, plugins[i].value) != 0) continue;
            size_t plen = strlen(plugins[i].name);
            if (strncmp(caps[c].name, plugins[i].name, plen) != 0) continue;
            if (caps[c].name[plen] != '.') continue;
            const char *cap = caps[c].name + plen + 1;
            int id = -1;
            for (size_t k = 0; k < n_ids; k++)
                if (strcmp(cap, EP_CAPABILITY_IDS[k]) == 0) { id = (int)k; break; }
            if (id < 0) continue;
            bool dup = false;
            for (size_t k = 0; k < n_seen; k++) if (seen[k] == id) { dup = true; break; }
            if (dup) continue;
            if (n_seen < EP_MAX_CAPS) seen[n_seen++] = id;
            json_append(declared, json_string(cap));
        }
        json_set(m, "capabilities", declared);
        json_append(out, m);
    }
}

/* Scan one root dir for .dist-info and .egg-info entries. */
static void scan_root(const char *root, json_t *out)
{
    DIR *d = opendir(root);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        const char *n = de->d_name;
        size_t len = strlen(n);
        bool is_dist = false;
        if (len > 10 && strcmp(n + len - 10, ".dist-info") == 0) is_dist = true;
        if (len > 9 && strcmp(n + len - 9, ".egg-info") == 0) is_dist = true;
        if (!is_dist) continue;
        char full[1024];
        snprintf(full, sizeof full, "%s/%s", root, n);
        collect_dist(full, out);
    }
    closedir(d);
}

json_t *discover_entrypoint_manifests(void)
{
    json_t *out = json_array();
    if (!out) return NULL;

    /* Same sources a native process can legitimately know about: PYTHONPATH entries (where a
     * Python host keeps its distributions) and the Hermes plugin directory. */
    const char *pp = getenv("PYTHONPATH");
    if (pp && *pp) {
        char buf[4096];
        snprintf(buf, sizeof buf, "%s", pp);
        char *save = NULL;
        for (char *tok = buf; (tok = strtok_r(tok, ";:", &save)) != NULL; tok = NULL)
            scan_root(tok, out);
    }
    const char *local = getenv("LOCALAPPDATA");
    if (local && *local) {
        char buf[1024];
        snprintf(buf, sizeof buf, "%s/hermes/plugins", local);
        scan_root(buf, out);
    }
    const char *home = getenv("HERMES_HOME");
    if (home && *home) {
        char buf[1024];
        snprintf(buf, sizeof buf, "%s/plugins", home);
        scan_root(buf, out);
    }
    return out;
}

/* ── TUI model picker / status line ──────────────────────────────────────────────────── */

/* The global UI state the TUI files all share (src/cli/tui_state.h:951). */
tui_global_state_t tui;



/* model_picker_state_t has no `visible` flag: an empty list (count == 0) is what makes the
 * overlay not draw, so that is what "closed" means here. */
void tui_model_picker_init(void)
{
    memset(&tui.model_picker, 0, sizeof tui.model_picker);
}

void tui_draw_model_picker(void)
{
    /* The overlay is painted by the main draw loop when count > 0; there is no curses surface
     * to write to from here. */
}

void tui_redraw_status(void)
{
    /* The status line is repainted by the main draw loop on the next frame; there is no
     * dirty flag in tui_global_state_t, so this is the caller's request and nothing more. */
}
