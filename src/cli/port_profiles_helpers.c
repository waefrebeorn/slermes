/*
 * port_profiles_helpers.c
 *
 * Pure, portable helpers ported from hermes_cli/profiles.py. These are the
 * name normalization / validation / archive-path-safety helpers that touch
 * no filesystem, no network, and no env — only string + path-shaped logic:
 *   - normalize_profile_name       (canonical on-disk id; "default" passes
 *                                   through; lowercase + strip)
 *   - validate_profile_name        (regex [a-z0-9][a-z0-9_-]{0,63} + reserved
 *                                   names; "default" is a valid special alias)
 *   - validate_alias_name          (same regex; used as a bare filename)
 *   - normalize_profile_archive_parts (safe posix path parts; rejects
 *                                   absolute / ".." / empty — mirrors the
 *                                   PurePosixPath/PureWindowsPath checks)
 *
 * The IO-coupled functions (get_profile_dir, create_profile, export/import,
 * wrapper scripts, gateway service registration) are honest REAL_GAPs and are
 * NOT ported here.
 *
 * Module prefix used by the scanner for hermes_cli/profiles.py is "profiles_".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* --- normalize_profile_name ------------------------------------------- */
/* PoP: normalize_profile_name @ hermes_cli/profiles.py:normalize_profile_name */
/*
 * Canonical profile id: strip, reject empty (return -1), "default" (any case)
 * -> "default", else lowercase. On success fill out (malloc'd by caller or
 * static). We return the length written into out (excluding NUL), or -1 for
 * empty. Caller provides out of size >= 64.
 */
int profiles_normalize_profile_name(const char *name, char *out, size_t outsz)
{
    if (!name) { if (out && outsz) out[0] = '\0'; return -1; }
    /* strip leading/trailing whitespace */
    const char *p = name;
    while (*p == ' ' || *p == '\t') p++;
    const char *e = p + strlen(p);
    while (e > p && (e[-1] == ' ' || e[-1] == '\t')) e--;
    size_t n = (size_t)(e - p);
    if (n == 0) { if (out && outsz) out[0] = '\0'; return -1; }

    /* casefold comparison with "default" */
    if (n == 7) {
        char buf[8];
        for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)p[i]);
        buf[n] = '\0';
        if (strcmp(buf, "default") == 0) {
            if (out && outsz) { strncpy(out, "default", outsz - 1); out[outsz - 1] = '\0'; }
            return 7;
        }
    }
    /* lowercase copy (cap at outsz-1) */
    size_t w = 0;
    for (size_t i = 0; i < n && w + 1 < outsz; i++) {
        out[w++] = (char)tolower((unsigned char)p[i]);
    }
    out[w] = '\0';
    return (int)w;
}

/* --- validation regex equivalent -------------------------------------- */
/* Python: ^[a-z0-9][a-z0-9_-]{0,63}$ */
static int profiles_id_valid(const char *s)
{
    size_t n = strlen(s);
    if (n < 1 || n > 64) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        int ok = isalnum((unsigned char)c) || c == '_' || c == '-';
        if (!ok) return 0;
        if (i == 0 && !isalnum((unsigned char)c)) return 0; /* first must be alnum */
    }
    return 1;
}

static int profiles_is_reserved(const char *s)
{
    static const char *RES[] = {
        "hermes", "default", "test", "tmp", "root", "sudo", NULL
    };
    for (int i = 0; RES[i]; i++)
        if (strcasecmp(s, RES[i]) == 0) return 1;
    return 0;
}

/* --- validate_profile_name -------------------------------------------- */
/* PoP: validate_profile_name @ hermes_cli/profiles.py:validate_profile_name */
/* Returns 0 if valid; -1 and fills err otherwise. "default" is always valid. */
int profiles_validate_profile_name(const char *name, char *err, size_t errsz)
{
    if (err) err[0] = '\0';
    if (name && strcasecmp(name, "default") == 0) return 0;
    if (!name || !profiles_id_valid(name)) {
        if (err) snprintf(err, errsz,
            "Invalid profile name '%s'. Must match [a-z0-9][a-z0-9_-]{0,63}",
            name ? name : "");
        return -1;
    }
    if (profiles_is_reserved(name)) {
        if (err) snprintf(err, errsz,
            "Profile name '%s' is reserved -- it collides with either the "
            "Hermes installation itself or a common system binary.  Pick a "
            "different name.", name);
        return -1;
    }
    return 0;
}

/* --- validate_alias_name ---------------------------------------------- */
/* PoP: validate_alias_name @ hermes_cli/profiles.py:validate_alias_name */
/* Same regex as profile id (forbids '/', '.', '..'); no reserved-name check. */
int profiles_validate_alias_name(const char *name, char *err, size_t errsz)
{
    if (err) err[0] = '\0';
    if (!name || !profiles_id_valid(name)) {
        if (err) snprintf(err, errsz,
            "Invalid alias name '%s'. Must match [a-z0-9][a-z0-9_-]{0,63}",
            name ? name : "");
        return -1;
    }
    return 0;
}

/* --- normalize_profile_archive_parts ---------------------------------- */
/* PoP: _normalize_profile_archive_parts @ hermes_cli/profiles.py:_normalize_profile_archive_parts */
/*
 * Split an archive member name into safe posix path parts. Rejects:
 *   - empty / None
 *   - absolute posix path (leading '/') or any component ".."
 *   - windows absolute (drive letter, leading backslash) — detected by a
 *     ':' before any '/' or a leading '\\'
 * Returns the number of parts written into parts[] (max maxparts), or -1 on
 * unsafe input (and fills err). parts entries are malloc'd; caller frees.
 */
int profiles_normalize_profile_archive_parts(const char *member,
                                             char **parts, int maxparts,
                                             char *err, size_t errsz)
{
    if (err) err[0] = '\0';
    if (!member || !*member) {
        if (err) snprintf(err, errsz, "Unsafe archive member path: %s", member ? member : "(null)");
        return -1;
    }
    /* windows drive detection: a ':' with no preceding '/' and no '/' before it */
    int has_drive = 0;
    for (const char *q = member; *q; q++) {
        if (*q == ':') { has_drive = 1; break; }
        if (*q == '/') { has_drive = 0; break; }
    }
    /* backslash => treat as windows separator; presence of leading '\\' or ':' => absolute */
    const char *scan = member;
    int lead_bslash = (scan[0] == '\\');
    if (has_drive || lead_bslash) {
        if (err) snprintf(err, errsz, "Unsafe archive member path: %s", member);
        return -1;
    }
    /* posix absolute */
    if (member[0] == '/') {
        if (err) snprintf(err, errsz, "Unsafe archive member path: %s", member);
        return -1;
    }
    /* split on / and \\ */
    int cnt = 0;
    const char *seg = member;
    while (*seg && cnt < maxparts) {
        /* skip separators */
        while (*seg == '/' || *seg == '\\') seg++;
        if (!*seg) break;
        const char *end = seg;
        while (*end && *end != '/' && *end != '\\') end++;
        size_t len = (size_t)(end - seg);
        if (len == 1 && seg[0] == '.') { seg = end; continue; } /* '.' skip */
        if (len == 2 && seg[0] == '.' && seg[1] == '.') {
            if (err) snprintf(err, errsz, "Unsafe archive member path: %s", member);
            /* free already-allocated */
            for (int i = 0; i < cnt; i++) free(parts[i]);
            return -1;
        }
        parts[cnt] = malloc(len + 1);
        memcpy(parts[cnt], seg, len);
        parts[cnt][len] = '\0';
        cnt++;
        seg = end;
    }
    if (cnt == 0) {
        if (err) snprintf(err, errsz, "Unsafe archive member path: %s", member);
        return -1;
    }
    return cnt;
}
