/*
 * profiles_initialization.c - C11 port of upstream
 * hermes_cli/profiles_initialization.py (gate w23_profilesin_594c72).
 *
 * PoP: w23_profilesin_594c72_staging @ hermes_cli/profiles_initialization.py:clone_staging_dir
 * PoP: w23_profilesin_594c72_discard @ hermes_cli/profiles_initialization.py:discard_unpublished_vault_key
 * PoP: w23_profilesin_594c72_prepare_import @ hermes_cli/profiles_initialization.py:prepare_imported_tree
 * PoP: w23_profilesin_594c72_finish_layout @ hermes_cli/profiles_initialization.py:finish_profile_layout
 *
 * Everything here runs on the staging tree before the publish rename, so the
 * filesystem, the pid, the keychain and the profiles module's seeders are all
 * seams: the caller says what the tree looks like and records what the port
 * decides to do to it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_profilesin_594c72.h"

/* Seeds this module forwards from hermes_cli.profiles:
 *   _PLACEHOLDER_ENV      - header of a fresh profile's empty .env
 *   NO_BUNDLED_SKILLS_MARKER - opt-out marker file name
 * Both are the upstream literals; a change there must fail this gate. */
static const char PI_PLACEHOLDER_ENV[] =
    "# Per-profile secrets for this Hermes profile.\n"
    "# API keys and tokens set here override the shell environment.\n"
    "# Behavioral settings belong in config.yaml, not here.\n";
static const char PI_NO_SKILLS_MARKER[] = ".no-bundled-skills";

/* The marker's body is written inline by finish_profile_layout. */
static const char PI_NO_SKILLS_BODY[] =
    "This profile opted out of bundled-skill seeding (`hermes profile create --no-skills`).\n"
    "Delete this file to re-enable sync on the next `hermes update`.\n";

/* PM_RUNTIME_ROOT_DIRS from hermes_cli.home_data_layout: foreign runtime state
 * that must not survive an import. */
static const char *const PI_RUNTIME_ROOTS[] = {"installs", "tools", "cache"};

/* ------------------------------------------------------------ path helpers */

#define PI_MAXSEG 32
#define PI_SEGLEN 192

typedef struct {
    char seg[PI_MAXSEG][PI_SEGLEN];
    size_t n;
} pi_segs;

static void pi_split(const char *s, pi_segs *out, char *drive, int *has_drive, int *absolute) {
    size_t len = strlen(s), i = 0, bl = 0;
    char buf[PI_SEGLEN];
    out->n = 0;
    *has_drive = 0;
    *absolute = 0;
    drive[0] = '\0';
    drive[1] = '\0';
    if (len >= 2 && s[1] == ':') {
        *has_drive = 1;
        drive[0] = s[0];
        i = 2;
    }
    if (i < len && (s[i] == '\\' || s[i] == '/')) *absolute = 1;
    for (; i <= len; i++) {
        char c = (i < len) ? s[i] : '\0';
        if (c == '\\' || c == '/' || c == '\0') {
            buf[bl] = '\0';
            if (bl > 0 && !(bl == 1 && buf[0] == '.')) {
                if (out->n < PI_MAXSEG) {
                    memcpy(out->seg[out->n], buf, bl + 1);
                    out->n++;
                }
            }
            bl = 0;
            if (c == '\0') break;
            continue;
        }
        if (bl + 1 < PI_SEGLEN) buf[bl++] = c;
    }
}

static void pi_join_segs(char *out, size_t cap, const char *drive, int has_drive,
                         int absolute, const pi_segs *segs) {
    size_t used = 0, i;
    out[0] = '\0';
    if (has_drive && used + 2 < cap) {
        out[used++] = drive[0];
        out[used++] = ':';
        out[used] = '\0';
    }
    if (absolute && used + 1 < cap) {
        out[used++] = '\\';
        out[used] = '\0';
    }
    for (i = 0; i < segs->n; i++) {
        size_t sl = strlen(segs->seg[i]);
        if (used && out[used - 1] != '\\' && used + 1 < cap) out[used++] = '\\';
        if (used + sl < cap) {
            memcpy(out + used, segs->seg[i], sl);
            used += sl;
        }
        out[used] = '\0';
    }
}

/* str(Path(dir)) */
static void pi_path_str(const char *dir, char *out, size_t cap) {
    pi_segs segs;
    char drive[2];
    int has_drive, absolute;
    pi_split(dir ? dir : "", &segs, drive, &has_drive, &absolute);
    pi_join_segs(out, cap, drive, has_drive, absolute, &segs);
}

/* str(Path(dir) / name) */
static void pi_child(const char *dir, const char *name, char *out, size_t cap) {
    char base[1024];
    pi_path_str(dir, base, sizeof(base));
    if (!base[0]) snprintf(out, cap, "%s", name);
    else if (base[strlen(base) - 1] == '\\') snprintf(out, cap, "%s%s", base, name);
    else snprintf(out, cap, "%s\\%s", base, name);
}

/* str(Path(dir).parent) */
static void pi_parent(const char *dir, char *out, size_t cap) {
    char base[1024];
    char *slash;
    pi_path_str(dir, base, sizeof(base));
    slash = strrchr(base, '\\');
    if (slash) {
        if (slash == base) {
            snprintf(out, cap, "%s", "\\");
            return;
        }
        if (strlen(base) >= 3 && base[1] == ':' && slash == base + 2) {
            /* a drive root is its own parent */
            snprintf(out, cap, "%s", base);
            return;
        }
        *slash = '\0';
        snprintf(out, cap, "%s", base);
        return;
    }
    if (strlen(base) == 2 && base[1] == ':') {
        snprintf(out, cap, "%c:\\", base[0]);
        return;
    }
    snprintf(out, cap, "%s", ".");
}

/* str(Path(dir).name) */
static void pi_name(const char *dir, char *out, size_t cap) {
    char base[1024];
    char *slash;
    pi_path_str(dir, base, sizeof(base));
    slash = strrchr(base, '\\');
    snprintf(out, cap, "%s", slash ? slash + 1 : base);
}

/* --------------------------------------------------------------- staging */

/* clone_staging_dir: name the staging tree beside the final dir, discarding a
 * leftover from a crashed create first. */
json_t *w23_profilesin_594c72_staging(const json_t *req) {
    const char *dir = json_get_str(req, "dir", "");
    const char *leftover = json_get_str(req, "leftover", "none");
    long long pid = (long long)json_get_num(req, "pid", 0.0);
    char parent[1024], name[512], path[2048];
    json_t *out = json_object();

    pi_parent(dir, parent, sizeof(parent));
    pi_name(dir, name, sizeof(name));
    if (!parent[0] || (parent[0] == '.' && !parent[1]))
        snprintf(path, sizeof(path), ".%s.staging-%lld", name, pid);
    else if (parent[strlen(parent) - 1] == '\\')
        snprintf(path, sizeof(path), "%s.%s.staging-%lld", parent, name, pid);
    else
        snprintf(path, sizeof(path), "%s\\.%s.staging-%lld", parent, name, pid);

    json_set(out, "path", json_string(path));
    /* is_symlink/is_file -> unlink, is_dir -> rmtree, nothing otherwise */
    json_set(out, "removed", json_bool(leftover && *leftover && strcmp(leftover, "none")));
    return out;
}

/* --------------------------------------------------------------- discard */

/* discard_unpublished_vault_key: delete exactly the reference the clone's own
 * keychain entry handed back, never one inferred from the source profile. */
json_t *w23_profilesin_594c72_discard(const json_t *req) {
    const json_t *ref = req ? json_obj_get(req, "ref") : NULL;
    json_t *out = json_object();
    int present = ref && ref->type != JSON_NULL;
    json_set(out, "deleted", json_bool(present));
    json_set(out, "ref", present ? json_copy((json_t *)ref) : json_null());
    return out;
}

/* -------------------------------------------------------- imported tree */

static int pi_is_runtime_root(const char *name) {
    size_t i;
    if (!name) return 0;
    for (i = 0; i < sizeof(PI_RUNTIME_ROOTS) / sizeof(PI_RUNTIME_ROOTS[0]); i++)
        if (!strcmp(name, PI_RUNTIME_ROOTS[i])) return 1;
    return 0;
}

static int pi_str_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* prepare_imported_tree: remove foreign runtime roots (file-shaped ones too),
 * strip world access from config.yaml, make .env owner-only. */
json_t *w23_profilesin_594c72_prepare_import(const json_t *req) {
    const char *dir = json_get_str(req, "dir", "");
    const json_t *children = req ? json_obj_get(req, "children") : NULL;
    const char *env_kind = json_get_str(req, "env", "absent");
    const char *config_kind = json_get_str(req, "config", "absent");
    const char *kept[PI_MAXSEG];
    const char *removed[PI_MAXSEG];
    size_t nk = 0, nr = 0, i, k, n = 0;
    char path[2048];
    json_t *kept_out = json_array();
    json_t *removed_out = json_array();
    json_t *out = json_object();

    if (children && children->type == JSON_ARRAY) {
        n = json_len(children);
        if (n > PI_MAXSEG) n = PI_MAXSEG;
    }
    for (i = 0; i < n; i++) {
        json_t *child = json_get(children, i);
        const char *name = child ? json_get_str(child, "name", "") : "";
        if (pi_is_runtime_root(name)) removed[nr++] = name;
        else kept[nk++] = name;
    }
    /* the tree's own files survive the sweep: the credentials file and the
     * config the import extracted */
    {
        const char *always[] = {NULL, NULL};
        if (env_kind && strcmp(env_kind, "absent")) always[0] = ".env";
        if (config_kind && !strcmp(config_kind, "file")) always[1] = "config.yaml";
        for (k = 0; k < 2; k++) {
            size_t j;
            int seen = 0;
            if (!always[k]) continue;
            for (j = 0; j < nk; j++)
                if (!strcmp(kept[j], always[k])) { seen = 1; break; }
            if (!seen && nk < PI_MAXSEG) kept[nk++] = always[k];
        }
    }
    qsort(kept, nk, sizeof(kept[0]), pi_str_cmp);
    qsort(removed, nr, sizeof(removed[0]), pi_str_cmp);
    for (i = 0; i < nk; i++) json_append(kept_out, json_string(kept[i]));
    for (i = 0; i < nr; i++) json_append(removed_out, json_string(removed[i]));

    json_set(out, "kept", kept_out);
    json_set(out, "removed", removed_out);
    /* drop_other_access is called unconditionally, existing file or not */
    pi_child(dir, "config.yaml", path, sizeof(path));
    json_set(out, "config_drop", json_string(path));
    /* .env: only a regular, non-symlinked file is tightened to 0o600 */
    json_set(out, "env_chmod",
             (env_kind && !strcmp(env_kind, "file")) ? json_number(384.0) : json_null());
    return out;
}

/* ------------------------------------------------------------- finish layout */

/* Python str.strip() over ASCII whitespace. */
static void pi_strip(const char *s, char *out, size_t cap) {
    size_t start = 0, end;
    if (!s) { out[0] = '\0'; return; }
    end = strlen(s);
    while (start < end && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' ||
                           s[start] == '\r' || s[start] == '\v' || s[start] == '\f'))
        start++;
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\n' ||
                           s[end - 1] == '\r' || s[end - 1] == '\v' || s[end - 1] == '\f'))
        end--;
    if (end - start >= cap) end = start + cap - 1;
    memcpy(out, s + start, end - start);
    out[end - start] = '\0';
}

static json_t *pi_action(const char *kind, const char *path) {
    json_t *a = json_object();
    json_set(a, "kind", json_string(kind));
    if (path) json_set(a, "path", json_string(path));
    return a;
}

/* finish_profile_layout: the seed order a fresh profile relies on. */
json_t *w23_profilesin_594c72_finish_layout(const json_t *req) {
    const char *dir = json_get_str(req, "dir", "");
    const json_t *soul_v = req ? json_obj_get(req, "soul") : NULL;
    const char *desc = json_get_str(req, "description", NULL);
    int no_skills = json_get_bool(req, "no_skills", 0) ? 1 : 0;
    int clone_all = json_get_bool(req, "clone_all", 0) ? 1 : 0;
    json_t *actions = json_array();
    json_t *out = json_object();
    json_t *a;
    char path[2048];
    char trimmed[1024];

    /* 1. an explicit SOUL body is written before any seeding */
    if (soul_v && soul_v->type == JSON_STRING) {
        pi_child(dir, "SOUL.md", path, sizeof(path));
        a = pi_action("write_soul", path);
        json_set(a, "text", json_string(soul_v->str_val ? soul_v->str_val : ""));
        json_append(actions, a);
    }
    /* 2. an empty .env so the profile owns a credentials file from day one */
    pi_child(dir, ".env", path, sizeof(path));
    a = pi_action("seed_env", path);
    json_set(a, "text", json_string(PI_PLACEHOLDER_ENV));
    json_set(a, "mode", json_number(384.0));
    json_append(actions, a);
    /* 3. a clone's copy2 carried the source config mode, world bits included */
    pi_child(dir, "config.yaml", path, sizeof(path));
    json_append(actions, pi_action("drop_access", path));
    /* 4. default SOUL.md, best-effort (an import failure is suppressed) */
    {
        int soul_error = json_get_bool(req, "default_soul_error", 0) ? 1 : 0;
        if (!soul_error) {
            pi_child(dir, "SOUL.md", path, sizeof(path));
            a = pi_action("seed_soul", path);
            json_set(a, "text", json_string("<DEFAULT_SOUL_MD>"));
            json_append(actions, a);
        }
    }
    /* 5. the bundled-skill opt-out marker */
    if (no_skills) {
        pi_child(dir, PI_NO_SKILLS_MARKER, path, sizeof(path));
        a = pi_action("seed_marker", path);
        json_set(a, "text", json_string(PI_NO_SKILLS_BODY));
        json_append(actions, a);
    }
    /* 6. config-only clones are migrated now; --clone-all stays byte-for-byte */
    if (!clone_all) {
        char base[1024];
        pi_path_str(dir, base, sizeof(base));
        json_append(actions, pi_action("migrate", base));
    }
    /* 7. the description lands last so a partial create strands nothing */
    pi_strip(desc, trimmed, sizeof(trimmed));
    if (desc && desc[0] && trimmed[0]) {
        char base[1024];
        a = pi_action("meta", NULL);
        pi_path_str(dir, base, sizeof(base));
        json_set(a, "path", json_string(base));
        json_set(a, "description", json_string(trimmed));
        json_set(a, "auto", json_bool(0));
        json_append(actions, a);
    }

    json_set(out, "actions", actions);
    return out;
}
