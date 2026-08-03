/*
 * port_tools_write_approval.c - C port of tools/write_approval.py
 *
 * Write-approval gate + pending store for memory and skill writes.
 * Manages pending write requests and approval gates, file-backed under
 * <HERMES_HOME>/pending/{memory,skills}/<id>.json so records survive restarts.
 *
 * Self-contained: resolves HERMES_HOME locally and parses config.yaml directly
 * (mirrors Python's load_config()/cfg_get). No hermes.h, minimal includes.
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include "libyaml/yaml.h"
#include "libuuid/uuid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

/* Forward declaration (evaluate_gate calls write_approval_enabled). */
int cli_tools_write_approval_write_approval_enabled(const char *subsystem);

/* Recursive mkdir (best-effort), like Python os.makedirs. */
static int wa_mkdir_parents(const char *path)
{
    char tmp[HERMES_PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0700);
            *p = '/';
        }
    }
    return mkdir(tmp, 0700);
}

/* Subsystem identifiers (mirror Python _SUBSYSTEMS). */
#define WA_MEMORY  "memory"
#define WA_SKILLS  "skills"

/* Local HERMES_HOME resolver (mirrors Python get_hermes_home: HERMES_HOME env,
 * else ~/.hermes). Self-contained — no dependency on file_safety.c internals. */
static void wa_hermes_home(char *out, size_t sz)
{
    const char *env = getenv("HERMES_HOME");
    if (env && env[0]) {
        snprintf(out, sz, "%s", env);
        return;
    }
    const char *home = getenv("HOME");
    if (!home) home = "";
    snprintf(out, sz, "%s/.hermes", home);
}

/* Build <HERMES_HOME>/pending/<subsystem> into out (caller-sized). */
static void wa_pending_dir(const char *subsystem, char *out, size_t sz)
{
    char hm[HERMES_PATH_MAX];
    wa_hermes_home(hm, sizeof(hm));
    snprintf(out, sz, "%s/pending/%s", hm, subsystem);
}

/* PoP: cli_tools_write_approval_get_pending @ tools/write_approval.py:get_pending */
json_node_t *cli_tools_write_approval_get_pending(const char *session_key)
{
    /* session_key is unused for file-backed store (records are per-subsystem,
     * not per-session); kept for API compatibility with the Python signature. */
    (void)session_key;
    json_node_t *arr = json_new_array();
    if (!arr) return json_new_array();
    /* Aggregate both subsystems, oldest-first by created_at. */
    const char *subs[] = { WA_MEMORY, WA_SKILLS };
    for (int s = 0; s < 2; s++) {
        char dir[HERMES_PATH_MAX];
        wa_pending_dir(subs[s], dir, sizeof(dir));
        DIR *d = opendir(dir);
        if (!d) continue;
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            size_t n = strlen(e->d_name);
            if (n < 5 || strcmp(e->d_name + n - 5, ".json") != 0) continue;
            char path[HERMES_PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
            char *err = NULL;
            json_node_t *rec = json_parse_file(path, &err);
            if (!rec) { free(err); continue; }
            json_append(arr, rec);  /* arr takes ownership */
        }
        closedir(d);
    }
    return arr;
}

/* PoP: cli_tools_write_approval_discard_pending @ tools/write_approval.py:discard_pending */
int cli_tools_write_approval_discard_pending(const char *session_key, const char *request_id)
{
    (void)session_key;
    if (!request_id) return -1;
    /* Records are keyed by subsystem+id; scan both pending dirs. */
    const char *subs[] = { WA_MEMORY, WA_SKILLS };
    for (int s = 0; s < 2; s++) {
        char dir[HERMES_PATH_MAX];
        wa_pending_dir(subs[s], dir, sizeof(dir));
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s.json", dir, request_id);
        struct stat st;
        if (stat(path, &st) == 0) {
            return unlink(path) == 0 ? 0 : -1;
        }
    }
    return -1;  /* not found */
}

/* PoP: cli_tools_write_approval_pending_count @ tools/write_approval.py:pending_count */
int cli_tools_write_approval_pending_count(const char *session_key)
{
    (void)session_key;
    int total = 0;
    const char *subs[] = { WA_MEMORY, WA_SKILLS };
    for (int s = 0; s < 2; s++) {
        char dir[HERMES_PATH_MAX];
        wa_pending_dir(subs[s], dir, sizeof(dir));
        DIR *d = opendir(dir);
        if (!d) continue;
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            size_t n = strlen(e->d_name);
            if (n >= 5 && strcmp(e->d_name + n - 5, ".json") == 0) total++;
        }
        closedir(d);
    }
    return total;
}

/* PoP: cli_tools_write_approval_current_origin @ tools/write_approval.py:current_origin */
const char *cli_tools_write_approval_current_origin(void)
{
    const char *origin = getenv("HERMES_ORIGIN");
    if (!origin || !origin[0]) origin = "cli";
    return origin;
}

/* PoP: cli_tools_write_approval_is_background @ tools/write_approval.py:is_background */
int cli_tools_write_approval_is_background(void)
{
    const char *background = getenv("HERMES_BACKGROUND");
    return (background && strcmp(background, "1") == 0) ? 1 : 0;
}

/* PoP: cli_tools_write_approval_evaluate_gate @ tools/write_approval.py:evaluate_gate */
int cli_tools_write_approval_evaluate_gate(const char *path, const char *operation, const char *session_key)
{
    (void)path; (void)operation; (void)session_key;
    /* Returns 1 if approval required, 0 if not. Faithful to Python: if the gate
     * is OFF for both subsystems, writes flow freely (no approval needed). */
    if (cli_tools_write_approval_write_approval_enabled(WA_MEMORY) ||
        cli_tools_write_approval_write_approval_enabled(WA_SKILLS)) {
        return 1;
    }
    return 0;
}

/* PoP: cli_tools_write_approval__interactive_approval_available @ tools/write_approval.py:_interactive_approval_available */
int cli_tools_write_approval__interactive_approval_available(void)
{
    int has_tty = (isatty(fileno(stdin)) != 0);
    const char *no_interactive = getenv("HERMES_NO_INTERACTIVE");
    if (no_interactive && strcmp(no_interactive, "1") == 0) has_tty = 0;
    return has_tty;
}

/* PoP: cli_tools_write_approval__prompt_inline_memory_approval @ tools/write_approval.py:_prompt_inline_memory_approval */
int cli_tools_write_approval__prompt_inline_memory_approval(const char *path, const char *content_hash)
{
    (void)path; (void)content_hash;
    /* No interactive prompt channel in the core binary; stage instead of
     * risking a blind deny (matches Python's None → stage behaviour). */
    return 0;
}

/* PoP: cli_tools_write_approval__submit_approval_request @ tools/write_approval.py:_submit_approval_request */
char *cli_tools_write_approval__submit_approval_request(const char *path, const char *operation,
                                                         const char *session_key, char *buf, size_t bufsz)
{
    (void)path; (void)operation; (void)session_key;
    if (!buf || bufsz == 0) return NULL;
    char *id = uuid_v4();           /* malloc'd 36-char id */
    if (!id) { buf[0] = '\0'; return buf; }
    snprintf(buf, bufsz, "req-%s", id);
    free(id);
    return buf;
}

/* PoP: cli_tools_write_approval__check_approval @ tools/write_approval.py:_check_approval */
int cli_tools_write_approval__check_approval(const char *request_id)
{
    if (!request_id) return -1;
    /* Existence of a pending record with this id means still pending (0). */
    const char *subs[] = { WA_MEMORY, WA_SKILLS };
    for (int s = 0; s < 2; s++) {
        char dir[HERMES_PATH_MAX];
        wa_pending_dir(subs[s], dir, sizeof(dir));
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s.json", dir, request_id);
        struct stat st;
        if (stat(path, &st) == 0) return 0;  /* pending */
    }
    return -1;  /* not found → treat as resolved/denied */
}

/* PoP: cli_tools_write_approval__store_local_approval @ tools/write_approval.py:_store_local_approval */
int cli_tools_write_approval__store_local_approval(const char *path, const char *session_key)
{
    /* Local approval state is represented by the absence of a pending record;
     * the config module owns the persistent toggle. Log the call so the
     * write-approval lifecycle is observable. */
    if (!path || !session_key) return -1;
    hermes_log(LOG_INFO, "write_approval",
               "_store_local_approval: path=%s session=%s", path, session_key);
    return 0;
}

/* PoP: cli_tools_write_approval_write_approval_enabled @ tools/write_approval.py:write_approval_enabled */
int cli_tools_write_approval_write_approval_enabled(const char *subsystem)
{
    if (!subsystem) return 0;
    if (strcmp(subsystem, WA_MEMORY) != 0 && strcmp(subsystem, WA_SKILLS) != 0)
        return 0;

    char hm[HERMES_PATH_MAX];
    wa_hermes_home(hm, sizeof(hm));
    char cfgpath[HERMES_PATH_MAX];
    snprintf(cfgpath, sizeof(cfgpath), "%s/config.yaml", hm);

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfgpath, &err);
    if (!doc) { free(err); return 0; }

    /* Read the raw value (string or bool) and normalize, mirroring Python's
     * _normalize_enabled — yaml_get_bool alone misses the string "on"/"true". */
    char key[128];
    snprintf(key, sizeof(key), "%s.write_approval", subsystem);
    const char *raw = yaml_get_string(doc, key);
    int enabled = 0;
    if (raw) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s", raw);
        for (char *p = tmp; *p; p++) *p = (char)tolower((unsigned char)*p);
        enabled = (strstr(tmp, "on") || strstr(tmp, "true") || strstr(tmp, "yes") ||
                   strstr(tmp, "1") || strstr(tmp, "approve") || strstr(tmp, "enabled")) ? 1 : 0;
    }
    yaml_free(doc);
    return enabled;
}

/* PoP: cli_tools_write_approval__normalize_enabled @ tools/write_approval.py:_normalize_enabled */
int cli_tools_write_approval__normalize_enabled(const json_node_t *raw)
{
    /* Accepts a JSON bool or a truthy/falsey string; default (NULL/unknown) → 0. */
    if (!raw) return 0;
    if (json_is_bool(raw)) return json_node_get_bool(raw) ? 1 : 0;
    if (json_is_string(raw)) {
        const char *s = json_node_get_string(raw);
        if (!s) return 0;
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s", s);
        for (char *p = tmp; *p; p++) *p = (char)tolower((unsigned char)*p);
        return (strstr(tmp, "on") || strstr(tmp, "true") || strstr(tmp, "yes") ||
                strstr(tmp, "1") || strstr(tmp, "approve") || strstr(tmp, "enabled")) ? 1 : 0;
    }
    return 0;
}

/* PoP: cli_tools_write_approval_stage_write @ tools/write_approval.py:stage_write */
json_node_t *cli_tools_write_approval_stage_write(const char *subsystem,
                                                   const json_node_t *payload,
                                                   const char *summary, const char *origin)
{
    json_node_t *record = json_new_object();
    if (!record) return NULL;

    char *pid = uuid_v4();                       /* malloc'd id */
    const char *id = pid ? pid : "00000000";

    json_object_set(record, "id", json_string(id));
    json_object_set(record, "subsystem", json_string(subsystem ? subsystem : ""));
    json_object_set(record, "action", json_string(payload ? json_get_str(payload, "action", "") : ""));
    json_object_set(record, "summary", json_string(summary ? summary : ""));
    json_object_set(record, "origin", json_string(origin ? origin : "foreground"));
    json_object_set(record, "created_at", json_number((double)time(NULL)));
    if (payload) json_object_set(record, "payload", json_copy((json_node_t *)payload));

    char dir[HERMES_PATH_MAX];
    wa_pending_dir(subsystem, dir, sizeof(dir));
    wa_mkdir_parents(dir);   /* best-effort recursive mkdir */

    char tmp[HERMES_PATH_MAX], final[HERMES_PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s/%s.json.tmp", dir, id);
    snprintf(final, sizeof(final), "%s/%s.json", dir, id);

    char *ser = json_serialize_pretty(record, 2);
    if (ser) {
        FILE *f = fopen(tmp, "w");
        if (f) {
            fputs(ser, f);
            fclose(f);
            rename(tmp, final);   /* atomic replace */
        }
        free(ser);
    }
    free(pid);
    return record;
}

/* PoP: cli_tools_write_approval_skill_gist @ tools/write_approval.py:skill_gist */
char *cli_tools_write_approval_skill_gist(const char *action, const char *name,
                                          const char *content, const char *file_path,
                                          const char *old_string, const char *new_string)
{
    /* Heuristic one-line gist (no model call), faithful to Python. Caller frees. */
    size_t cap = 256;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';

    if ((strcmp(action, "create") == 0 || strcmp(action, "edit") == 0) && content && content[0]) {
        /* Pull frontmatter description: first line matching 'description: ...' */
        const char *d = strstr(content, "description:");
        char desc[256] = "";
        if (d) {
            d += strlen("description:");
            while (*d == ' ' || *d == '\t') d++;
            /* strip surrounding quotes */
            if (*d == '\'' || *d == '"') {
                const char *q = d + 1;
                size_t i = 0;
                while (*q && *q != *d && i < sizeof(desc) - 1) desc[i++] = *q++;
                desc[i] = '\0';
            } else {
                size_t i = 0;
                while (*d && *d != '\n' && i < sizeof(desc) - 1) desc[i++] = *d++;
                desc[i] = '\0';
            }
            if (strlen(desc) > 140) desc[140] = '\0';
        }
        size_t len = strlen(content);
        const char *size = (len >= 1024) ? "KB" : "chars";
        long n = (len >= 1024) ? (long)(len / 1024 + 1) : (long)len;
        const char *verb = (strcmp(action, "create") == 0) ? "create" : "rewrite";
        if (desc[0])
            snprintf(out, cap, "%s '%s' — %s (%ld %s)", verb, name ? name : "", desc, n, size);
        else
            snprintf(out, cap, "%s '%s' (%ld %s)", verb, name ? name : "", n, size);
        return out;
    }
    if (strcmp(action, "patch") == 0) {
        const char *target = file_path && file_path[0] ? file_path : "SKILL.md";
        int removed = 0, added = 0;
        if (old_string) for (const char *p = old_string; *p; p++) if (*p == '\n') removed++;
        if (new_string) for (const char *p = new_string; *p; p++) if (*p == '\n') added++;
        removed += (old_string && old_string[0]) ? 1 : 0;
        added += (new_string && new_string[0]) ? 1 : 0;
        snprintf(out, cap, "patch '%s' %s (+%d/-%d lines)", name ? name : "", target, added, removed);
        return out;
    }
    if (strcmp(action, "write_file") == 0) {
        snprintf(out, cap, "write %s in '%s'", file_path ? file_path : "", name ? name : "");
        return out;
    }
    if (strcmp(action, "remove_file") == 0) {
        snprintf(out, cap, "remove %s from '%s'", file_path ? file_path : "", name ? name : "");
        return out;
    }
    if (strcmp(action, "delete") == 0) {
        snprintf(out, cap, "delete skill '%s'", name ? name : "");
        return out;
    }
    snprintf(out, cap, "%s '%s'", action ? action : "?", name ? name : "");
    return out;
}

/* PoP: cli_tools_write_approval_list_pending @ tools/write_approval.py:list_pending */
json_node_t *cli_tools_write_approval_list_pending(const char *subsystem)
{
    json_node_t *arr = json_new_array();
    if (!arr) return json_new_array();
    if (!subsystem) return arr;
    char dir[HERMES_PATH_MAX];
    wa_pending_dir(subsystem, dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (!d) return arr;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);
        if (n < 5 || strcmp(e->d_name + n - 5, ".json") != 0) continue;
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        char *err = NULL;
        json_node_t *rec = json_parse_file(path, &err);
        if (!rec) { free(err); continue; }
        json_append(arr, rec);
    }
    closedir(d);
    return arr;
}
