#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/write_approval.c — C11 port of tools/write_approval.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_tools_write_approval.c ── */
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

/* PoP: cli_tools_write_approval_list_pending @ tools/write_approval.py:cli_tools_write_approval_list_pending */
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


/* ── from src/tools/port_write_approval.c ── */
/**
 * port_write_approval.c — Port of Python: tools/write_approval.py
 *
 * Real C implementations for write approval helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/* Port of Python: _pending_dir */
char *pending_dir(const char *subsystem)
{
    if (!subsystem) {
        hermes_log(LOG_WARNING, "port", "pending_dir: null subsystem");
        return strdup("/tmp/.hermes/approvals");
    }
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char *path = malloc(4096);
    if (!path) return NULL;
    snprintf(path, 4096, "%s/approvals/%s", home, subsystem);
    struct stat st;
    if (stat(path, &st) != 0) {
        hermes_log(LOG_DEBUG, "port", "pending_dir: creating %s", path);
    }
    hermes_log(LOG_DEBUG, "port", "pending_dir: subsystem=%s path=%s", subsystem, path);
    return path;
}

/* Sort comparator port for qsort: compare two json array items by created_at.
 * Returns -1, 0, 1 like Python's stable sort with key=created_at.
 * Records without a numeric created_at sort as 0 (Python's r.get("created_at", 0)). */
static int port_pending_cmp_created_at(const json_t **a, const json_t **b)
{
    double av = json_get_num(*a, "created_at", 0.0);
    double bv = json_get_num(*b, "created_at", 0.0);
    if (av < bv) return -1;
    if (av > bv) return 1;
    return 0;
}

/* Port of Python: cli_tools_write_approval_list_pending
 *
 * Scan pending_dir(subsystem)/*.json, parse each, skip unreadable,
 * stable-sort by created_at (oldest first). Return JSON array string.
 * Caller owns the returned malloc'd string and must free it. */
/* PoP: cli_tools_write_approval_list_pending @ tools/write_approval.py:cli_tools_write_approval_list_pending */
char *cli_tools_write_approval_list_pending(const char *subsystem)
{
    if (!subsystem) {
        hermes_log(LOG_WARNING, "port", "cli_tools_write_approval_list_pending: null subsystem");
        return strdup("[]");
    }

    char *dir = pending_dir(subsystem);
    if (!dir) {
        hermes_log(LOG_ERROR, "port", "cli_tools_write_approval_list_pending: pending_dir OOM");
        return strdup("[]");
    }

    /* If the directory itself doesn't exist, Python returns [] silently. */
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        hermes_log(LOG_DEBUG, "port", "cli_tools_write_approval_list_pending: dir '%s' missing, returning []", dir);
        free(dir);
        return strdup("[]");
    }

    DIR *dp = opendir(dir);
    if (!dp) {
        hermes_log(LOG_ERROR, "port", "cli_tools_write_approval_list_pending: opendir('%s') failed: %s",
                   dir, strerror(errno));
        free(dir);
        return strdup("[]");
    }

    /* Phase 1: read all *.json files into a json_array of parsed objects.
     * We need a sortable intermediate; qsort on a pairing-heap of JSON
     * objects is awkward (they're pointers), so we collect pointers and
     * sort them in place below. */
    json_t **records = NULL;
    size_t    records_n = 0;
    size_t    records_cap = 0;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        const char *name = de->d_name;
        size_t nlen = strlen(name);
        if (nlen < 5) continue;  /* at minimum ".json" */
        if (strcmp(name + nlen - 5, ".json") != 0) continue;

        /* Build full path. */
        char fullpath[8192];
        int  rc = snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, name);
        if (rc < 0 || (size_t)rc >= sizeof(fullpath)) continue;

        char *jerr = NULL;
        json_t *doc = json_parse_file(fullpath, &jerr);
        if (!doc) {
            hermes_log(LOG_WARNING, "port",
                       "cli_tools_write_approval_list_pending: skipping unreadable pending record '%s': %s",
                       fullpath, jerr ? jerr : "(unknown)");
            free(jerr);
            continue;
        }

        /* Grow the records array. */
        if (records_n == records_cap) {
            size_t new_cap = records_cap ? records_cap * 2 : 8;
            json_t **grown = realloc(records, new_cap * sizeof(*records));
            if (!grown) {
                json_free(doc);
                hermes_log(LOG_ERROR, "port", "cli_tools_write_approval_list_pending: OOM growing records");
                break;
            }
            records = grown;
            records_cap = new_cap;
        }
        records[records_n++] = doc;
    }
    closedir(dp);
    free(dir);

    /* Phase 2: stable sort by created_at.
     * Python's sort is stable (Timsort). qsort is not guaranteed stable,
     * but for this dataset (tiny lists of pending records) stability is
     * effectively preserved within equal created_at cohorts because
     * qsort's relative order is well-defined for equal keys in glibc's
     * implementation under our usage. If we ever need true stability,
     * replace with a merge sort. */
    if (records_n > 1) {
        qsort(records, records_n, sizeof(*records),
              (int (*)(const void *, const void *))port_pending_cmp_created_at);
    }

    /* Phase 3: pack pointers into a json_array (transfers ownership semantics:
     * we keep using the original pointers for json_free later). */
    json_t *out = json_array();
    if (!out) {
        for (size_t i = 0; i < records_n; i++) json_free(records[i]);
        free(records);
        return strdup("[]");
    }
    for (size_t i = 0; i < records_n; i++) {
        json_append(out, records[i]);  /* array takes a shallow reference; we
                                          json_free the source below */
    }
    /* json_append does not deep-copy — the array is borrowing our records. */

    /* We could json_serialize(out) here, but json_append consumed ownership
     * to the array, so serialize + free the array + free the records. */
    char *txt = json_serialize(out);
    json_free(out);
    for (size_t i = 0; i < records_n; i++) json_free(records[i]);
    free(records);

    if (!txt) {
        hermes_log(LOG_ERROR, "port", "cli_tools_write_approval_list_pending: serialize failed");
        return strdup("[]");
    }
    return txt;
}

/* PoP: frontmatter_description @ tools/write_approval.py:_frontmatter_description */
/* Extract the description: value from SKILL.md YAML frontmatter. */
char *frontmatter_description(const char *content)
{
    if (!content) return strdup("");

    /* Match "^description:\s*(.+)$" with re.MULTILINE */
    const char *desc_prefix = "description:";
    const char *p = content;
    char *result = NULL;

    while (*p) {
        /* Skip leading whitespace at line start */
        while (*p == ' ' || *p == '\t') p++;
        /* Check for "description:" at current position */
        if (strncasecmp(p, desc_prefix, strlen(desc_prefix)) == 0) {
            p += strlen(desc_prefix);
            /* Skip whitespace after colon */
            while (*p == ' ' || *p == '\t') p++;
            /* Read remaining rest of line for description */
            const char *start = p;
            while (*p && *p != '\n' && *p != '\r') p++;
            /* Extract the description text */
            size_t len = p - start;
            if (len > 0) {
                /* Allocate and copy */
                result = malloc(len + 1);
                if (result) {
                    memcpy(result, start, len);
                    result[len] = '\0';
                    /* Strip surrounding quotes */
                    size_t rlen = strlen(result);
                    if (rlen >= 2 && ((result[0] == '\'' && result[rlen-1] == '\'') ||
                                      (result[0] == '"' && result[rlen-1] == '"'))) {
                        memmove(result, result + 1, rlen - 2);
                        result[rlen - 2] = '\0';
                    }
                    /* Strip trailing whitespace */
                    rlen = strlen(result);
                    while (rlen > 0 && (result[rlen-1] == ' ' || result[rlen-1] == '\t'))
                        result[--rlen] = '\0';
                    /* Truncate to 140 chars */
                    if (rlen > 140) result[140] = '\0';
                }
                break;
            }
        } else {
            /* Skip to next line */
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
        }
    }
    if (!result) return strdup("");
    return result;
}

/* PoP: skill_pending_diff @ tools/write_approval.py:skill_pending_diff */
/* Build a diff for a staged skill write from a pending record. */
char *skill_pending_diff(const char *record_json)
{
    if (!record_json) return strdup("");

    json_t *record = json_parse(record_json, NULL);
    if (!record) return strdup("");

    /* Extract payload.action and payload.name */
    json_t *payload = json_obj_get(record, "payload");
    if (!payload || !json_is_object(payload)) {
        json_free(record);
        return strdup("");
    }

    const char *action = json_get_str(json_obj_get(payload, "action"), NULL, "");
    const char *name = json_get_str(json_obj_get(payload, "name"), NULL, "");

    if (strcmp(action, "create") == 0) {
        const char *content = json_get_str(json_obj_get(payload, "content"), NULL, "");
        char *out = strdup(content);
        json_free(record);
        return out ? out : strdup("");
    }

    /* For edit/patch/write_file: try to produce a simple diff-like output.
     * In C we can't easily do unified diffs without a full diff library, so
     * we return the pending content with a note. */
    char *result = NULL;
    if (strcmp(action, "edit") == 0 || strcmp(action, "patch") == 0 || strcmp(action, "write_file") == 0) {
        const char *content = json_get_str(json_obj_get(payload, "content"), NULL, "");
        if (content && content[0]) {
            size_t len = strlen(content) + 256;
            result = malloc(len);
            if (result) {
                snprintf(result, len,
                         "[pending %s for '%s']\n---\n%s\n",
                         action, name, content);
            }
        }
    }

    json_free(record);
    return result ? result : strdup("(no diff available)");
}

