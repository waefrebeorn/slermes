/*
 * port_profiles_helpers.c
 *
 * Pure, portable helper functions ported from hermes_cli/profiles.py.
 * Ported helpers: normalize_profile_name, validate_profile_name,
 * validate_alias_name, check_alias_collision (pure checks only — the PATH
 * "which" probe in check_alias_collision is OS-coupled and left as honest NA:
 * when the pure checks pass we return NULL, meaning "no pure collision found").
 *
 * Filesystem-coupled helpers (get_profile_dir, profile_exists, find_alias_for_profile,
 * build_alias_map, read/write_profile_meta, profiles_to_serve, seed_profile_skills)
 * are intentionally NOT ported — they hit the profile tree / spawn subprocesses.
 *
 * Module prefix used by the scanner for hermes_cli/profiles.py is "profiles_".
 *
 * C name <- python name (profiles_ prefix):
 *   profiles_normalize_profile_name, profiles_validate_profile_name,
 *   profiles_validate_alias_name, profiles_check_alias_collision
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>
#include "hermes_json.h"

/* Profile id regex: ^[a-z0-9][a-z0-9_-]{0,63}$ (anchored, lowercase only) */
static int profile_id_matches(const char *s)
{
    size_t n = strlen(s);
    if (n < 1 || n > 64) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        int ok = islower((unsigned char)c) || isdigit((unsigned char)c) ||
                 (i > 0 && (c == '-' || c == '_'));
        if (!ok) return 0;
    }
    return 1;
}

static int in_json_array(const char *arr_json, const char *val)
{
    if (!arr_json || !arr_json[0] || !val) return 0;
    json_t *a = json_parse(arr_json, NULL);
    if (!a || a->type != JSON_ARRAY) { if (a) json_free(a); return 0; }
    int found = 0;
    for (size_t i = 0; i < json_array_size(a); i++) {
        json_t *e = json_array_get(a, i);
        if (e && e->type == JSON_STRING && strcmp(json_string_value(e), val) == 0) { found = 1; break; }
    }
    json_free(a);
    return found;
}

/* ---------------------------------------------------------------------- */
/* PoP: normalize_profile_name @ hermes_cli/profiles.py:normalize_profile_name */
char *profiles_normalize_profile_name(const char *name)
{
    if (!name) return strdup("");
    /* strip */
    while (*name && isspace((unsigned char)*name)) name++;
    size_t L = strlen(name);
    while (L > 0 && isspace((unsigned char)name[L-1])) L--;
    if (L == 0) return strdup("");
    char *tmp = malloc(L + 1);
    memcpy(tmp, name, L); tmp[L] = '\0';
    /* casefold == "default" (ASCII, case-insensitive) */
    char low[256]; size_t k = 0;
    for (size_t i = 0; i < L && k < sizeof(low)-1; i++) low[k++] = (char)tolower((unsigned char)tmp[i]);
    low[k] = '\0';
    char *out;
    if (strcasecmp(low, "default") == 0) out = strdup("default");
    else {
        for (size_t i = 0; i < L; i++) tmp[i] = (char)tolower((unsigned char)tmp[i]);
        out = tmp;
    }
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: validate_profile_name @ hermes_cli/profiles.py:validate_profile_name
 * Returns NULL if valid, else malloc'd error message. "default" is a pass-through. */
char *profiles_validate_profile_name(const char *name)
{
    if (name && strcmp(name, "default") == 0) return NULL;
    if (!profile_id_matches(name)) {
        return strdup("Invalid profile name. Must match [a-z0-9][a-z0-9_-]{0,63}");
    }
    static const char *reserved[] = {"hermes","default","test","tmp","root","sudo",NULL};
    for (int i = 0; reserved[i]; i++)
        if (strcmp(name, reserved[i]) == 0)
            return strdup("is reserved — it collides with either the Hermes installation itself or a common system binary. Pick a different name.");
    return NULL;
}

/* ---------------------------------------------------------------------- */
/* PoP: validate_alias_name @ hermes_cli/profiles.py:validate_alias_name
 * Returns NULL if valid, else malloc'd error message. */
char *profiles_validate_alias_name(const char *name)
{
    if (!profile_id_matches(name)) {
        return strdup("Invalid alias name. Must match [a-z0-9][a-z0-9_-]{0,63}");
    }
    return NULL;
}

/* ---------------------------------------------------------------------- */
/* PoP: check_alias_collision @ hermes_cli/profiles.py:check_alias_collision
 * reserved_json / subcommands_json: JSON arrays of strings.
 * Returns malloc'd collision message, or NULL if no PURE collision found.
 * (The PATH "which" probe is OS-coupled and intentionally omitted — pure
 * checks only, matching the documented NA boundary.) */
char *profiles_check_alias_collision(const char *name, const char *reserved_json, const char *subcommands_json)
{
    char *canon = profiles_normalize_profile_name(name);
    char *err = profiles_validate_alias_name(canon);
    if (err) { char *out = err; free(canon); return out; }
    static const char *reserved[] = {"hermes","default","test","tmp","root","sudo",NULL};
    for (int i = 0; reserved[i]; i++) {
        if (strcmp(canon, reserved[i]) == 0) {
            char *out = malloc(strlen(canon) + 32);
            sprintf(out, "'%s' is a reserved name", canon);
            free(canon); return out;
        }
    }
    if (in_json_array(subcommands_json, canon)) {
        char *out = malloc(strlen(canon) + 48);
        sprintf(out, "'%s' conflicts with a hermes subcommand", canon);
        free(canon); return out;
    }
    free(canon);
    return NULL; /* pure checks pass; PATH probe is OS-coupled (NA) */
}
