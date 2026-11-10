/*
 * port_skill_commands.c
 *
 * Pure helpers ported from agent/skill_commands.py (44 defs, previously 0
 * ported) and agent/skill_bundles.py.
 *
 *   sc_slugify_skill_name     <- skill_commands.py:slugify_skill_name
 *   sc_command_snapshot       <- skill_commands.py:command_snapshot
 *   sc_diff_command_snapshots <- skill_commands.py:diff_command_snapshots
 *   sc_resolve_slash_key      <- skill_commands.py:resolve_slash_key
 *   sc_setup_note             <- skill_commands.py:_setup_note
 *   sc_format_missing_skills  <- skill_commands.py:format_missing_skills
 *
 * The scan/cache/plugin half of the module stays in Python - it is filesystem
 * and registry introspection. These are the pure transformations.
 */

#include "port_skill_commands.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* skill_commands.py:_SKILL_INVALID_CHARS = re.compile(r"[^\w-]") */
/* skill_commands.py:_SKILL_MULTI_HYPHEN = re.compile(r"-{2,}")      */
/* skill_utils.py:AMBIGUOUS_SKILL_PREFIX                             */
#define SC_AMBIGUOUS_SKILL_PREFIX "Ambiguous skill name "

/* skill_commands.py:_SETUP_SKIPPED_NOTE */
#define SC_SETUP_SKIPPED_NOTE \
    "Required environment setup was skipped. Continue loading the skill " \
    "and explain any reduced functionality if it matters."

/* ===========================================================================
 *  slugify_skill_name  (skill_commands.py:slugify_skill_name)
 *
 *      cmd = _SKILL_INVALID_CHARS.sub("", name.lower()
 *                                      .replace(" ", "-").replace("_", "-"))
 *      return _SKILL_MULTI_HYPHEN.sub("-", cmd).strip("-")
 *
 *  \w in Python's re is [a-zA-Z0-9_] for str patterns, so [^\w-] removes every
 *  char that is not alphanumeric, underscore or hyphen. Note the replacement
 *  happens BEFORE the invalid-char strip: a space becomes "-" and survives,
 *  while "_" also becomes "-" (so a literal underscore never reaches the
 *  character class). Caller frees.
 * =========================================================================== */

/* PoP: slugify_skill_name @ agent/skill_commands.py:slugify_skill_name */
char *sc_slugify_skill_name(const char *name)
{
    if (!name) return strdup("");

    size_t n = strlen(name);
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;

    /* name.lower().replace(" ", "-").replace("_", "-") */
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c == ' ' || c == '_') s[k++] = '-';
        else s[k++] = (char)tolower(c);
    }
    s[k] = '\0';

    /* _SKILL_INVALID_CHARS.sub("", ...) - drop anything not [\w-].
     * Python's re with a str pattern is UNICODE-aware, so \w matches non-ASCII
     * letters too; a UTF-8 lead or continuation byte (>= 0x80) is therefore kept. */
    size_t w = 0;
    for (size_t i = 0; i < k; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 0x80 || isalnum(c) || c == '_' || c == '-') s[w++] = s[i];
    }
    s[w] = '\0';

    /* _SKILL_MULTI_HYPHEN.sub("-", ...) - collapse runs of 2+ hyphens */
    size_t o = 0;
    for (size_t i = 0; i < w; i++) {
        if (s[i] == '-' && o > 0 && s[o - 1] == '-') continue;
        s[o++] = s[i];
    }
    s[o] = '\0';

    /* .strip("-") */
    size_t start = 0, end = o;
    while (start < end && s[start] == '-') start++;
    while (end > start && s[end - 1] == '-') end--;
    memmove(s, s + start, end - start);
    s[end - start] = '\0';
    return s;
}

/* ===========================================================================
 *  command_snapshot  (skill_commands.py:command_snapshot)
 *
 *      {key.lstrip("/"): (info or {}).get("description") or "" for key, info in cmds.items()}
 *
 *  `cmds` is a flat NULL-terminated [key, description, ...] array; a NULL
 *  description means `(info or {})` had no description, which yields "".
 *  lstrip("/") strips ALL leading slashes, not one. Returns a new flat array.
 * =========================================================================== */

/* PoP: command_snapshot @ agent/skill_commands.py:command_snapshot */
char **sc_command_snapshot(const char *const *cmds, int *out_count)
{
    int n = 0;
    if (cmds) while (cmds[n]) n++;
    int pairs = n / 2;

    char **out = (char **)calloc((size_t)pairs * 2 + 1, sizeof(char *));
    if (!out) { if (out_count) *out_count = 0; return NULL; }

    int k = 0;
    for (int i = 0; i < pairs; i++) {
        const char *key = cmds[i * 2];
        const char *desc = cmds[i * 2 + 1];
        if (key) while (*key == '/') key++;          /* lstrip("/") */
        out[k++] = strdup(key ? key : "");
        out[k++] = strdup((desc && desc[0]) ? desc : "");
    }
    if (out_count) *out_count = k;
    return out;
}

/* ===========================================================================
 *  diff_command_snapshots  (skill_commands.py:diff_command_snapshots)
 *
 *      added:     sorted(set(after) - set(before)) -> {name, description: after[n]}
 *      removed:   sorted(set(before) - set(after)) -> {name, description: before[n]}
 *      unchanged: sorted(set(after) & set(before))
 *      total:     len(after)
 *
 *  The outputs are JSON (the Python returns a dict consumed by callers that
 *  serialise it). `before` / `after` are flat [name, description, ...] arrays
 *  as produced by sc_command_snapshot. Caller frees the returned string.
 * =========================================================================== */

static int sc_cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* PoP: diff_command_snapshots @ agent/skill_commands.py:diff_command_snapshots */
char *sc_diff_command_snapshots(const char *const *before, const char *const *after)
{
    int nb = 0, na = 0;
    if (before) while (before[nb]) nb++;
    if (after)  while (after[na])  na++;

    /* collect the names */
    const char **bn = (const char **)calloc((size_t)nb / 2 + 1, sizeof(char *));
    const char **an = (const char **)calloc((size_t)na / 2 + 1, sizeof(char *));
    if (!bn || !an) { free(bn); free(an); return NULL; }
    for (int i = 0; i < nb / 2; i++) bn[i] = before[i * 2];
    for (int i = 0; i < na / 2; i++) an[i] = after[i * 2];

    const char **added   = (const char **)calloc((size_t)na / 2 + 1, sizeof(char *));
    const char **removed = (const char **)calloc((size_t)nb / 2 + 1, sizeof(char *));
    const char **same    = (const char **)calloc((size_t)(na / 2 + nb / 2) + 1, sizeof(char *));
    if (!added || !removed || !same) { free(bn); free(an); free(added); free(removed); free(same); return NULL; }

    int nadded = 0, nremoved = 0, nsame = 0;
    for (int i = 0; i < na / 2; i++) {
        int in_before = 0;
        for (int j = 0; j < nb / 2; j++) if (strcmp(an[i], bn[j]) == 0) { in_before = 1; break; }
        if (!in_before) added[nadded++] = an[i];
        else            same[nsame++]   = an[i];
    }
    for (int i = 0; i < nb / 2; i++) {
        int in_after = 0;
        for (int j = 0; j < na / 2; j++) if (strcmp(bn[i], an[j]) == 0) { in_after = 1; break; }
        if (!in_after) removed[nremoved++] = bn[i];
    }
    qsort(added, (size_t)nadded, sizeof(char *), sc_cmp_str);
    qsort(removed, (size_t)nremoved, sizeof(char *), sc_cmp_str);
    qsort(same, (size_t)nsame, sizeof(char *), sc_cmp_str);

    size_t cap = 256;
    for (int i = 0; i < nadded; i++)   cap += strlen(added[i]) * 2 + 32;
    for (int i = 0; i < nremoved; i++) cap += strlen(removed[i]) * 2 + 32;
    for (int i = 0; i < nsame; i++)    cap += strlen(same[i]) + 8;
    char *out = (char *)malloc(cap);
    if (!out) { free(bn); free(an); free(added); free(removed); free(same); return NULL; }
    char *p = out;

    p += sprintf(p, "{\"added\":[");
    for (int i = 0; i < nadded; i++) {
        const char *d = "";
        for (int j = 0; j < na / 2; j++) if (strcmp(an[j], added[i]) == 0) { d = after[j * 2 + 1] ? after[j * 2 + 1] : ""; break; }
        p += sprintf(p, "%s{\"name\":\"%s\",\"description\":\"%s\"}", i ? "," : "", added[i], d);
    }
    p += sprintf(p, "],\"removed\":[");
    for (int i = 0; i < nremoved; i++) {
        const char *d = "";
        for (int j = 0; j < nb / 2; j++) if (strcmp(bn[j], removed[i]) == 0) { d = before[j * 2 + 1] ? before[j * 2 + 1] : ""; break; }
        p += sprintf(p, "%s{\"name\":\"%s\",\"description\":\"%s\"}", i ? "," : "", removed[i], d);
    }
    p += sprintf(p, "],\"unchanged\":[");
    for (int i = 0; i < nsame; i++) p += sprintf(p, "%s\"%s\"", i ? "," : "", same[i]);
    p += sprintf(p, "],\"total\":%d}", na / 2);

    free(bn); free(an); free(added); free(removed); free(same);
    return out;
}

/* ===========================================================================
 *  resolve_slash_key  (skill_commands.py:resolve_slash_key)
 *
 *      if not command: return None
 *      exact_key = f"/{command.lower()}"
 *      if exact_key in table: return exact_key
 *      cmd_key = f"/{command.replace('_', '-').lower()}"
 *      return cmd_key if cmd_key in table else None
 *
 *  `table` is a flat NULL-terminated array of keys. Returns a malloc'd key or
 *  NULL. `_` is normalized to `-` only on the second attempt, which is why the
 *  exact form wins when both exist.
 * =========================================================================== */

/* PoP: resolve_slash_key @ agent/skill_commands.py:resolve_slash_key */
char *sc_resolve_slash_key(const char *command, const char *const *table)
{
    if (!command || !command[0]) return NULL;

    size_t n = strlen(command);
    char *lower = (char *)malloc(n + 1);
    if (!lower) return NULL;
    for (size_t i = 0; i < n; i++) lower[i] = (char)tolower((unsigned char)command[i]);
    lower[n] = '\0';

    size_t cap = n + 2;
    char *key = (char *)malloc(cap);
    if (!key) { free(lower); return NULL; }

    snprintf(key, cap, "/%s", lower);
    int found = 0;
    for (int i = 0; table && table[i]; i++) if (strcmp(table[i], key) == 0) { found = 1; break; }
    if (found) { free(lower); return key; }

    for (size_t i = 0; i < n; i++) if (lower[i] == '_') lower[i] = '-';
    snprintf(key, cap, "/%s", lower);
    found = 0;
    for (int i = 0; table && table[i]; i++) if (strcmp(table[i], key) == 0) { found = 1; break; }
    free(lower);
    if (found) return key;
    free(key);
    return NULL;
}

/* ===========================================================================
 *  _setup_note  (skill_commands.py:_setup_note)
 *
 *      if loaded_skill.get("setup_skipped"): return _SETUP_SKIPPED_NOTE
 *      return loaded_skill.get("gateway_setup_hint") or (
 *          loaded_skill.get("setup_note") if loaded_skill.get("setup_needed") else None
 *      ) or None
 *
 *  The `or` chain means an empty gateway_setup_hint falls through to the
 *  setup_note branch. Returns a malloc'd string or NULL.
 * =========================================================================== */

/* PoP: _setup_note @ agent/skill_commands.py:_setup_note */
char *sc_setup_note(int setup_skipped, const char *gateway_setup_hint,
                    int setup_needed, const char *setup_note)
{
    if (setup_skipped) return strdup(SC_SETUP_SKIPPED_NOTE);
    if (gateway_setup_hint && gateway_setup_hint[0]) return strdup(gateway_setup_hint);
    if (setup_needed && setup_note && setup_note[0]) return strdup(setup_note);
    return NULL;
}

/* ===========================================================================
 *  format_missing_skills  (skill_commands.py:format_missing_skills)
 *
 *      ambiguous = [m for m in missing if m.startswith(AMBIGUOUS_SKILL_PREFIX)]
 *      unknown = [m for m in missing if m not in ambiguous]
 *      return "; ".join(ambiguous + ([f"Unknown skill(s): {', '.join(unknown)}"] if unknown else []))
 *
 *  Caller frees.
 * =========================================================================== */

/* PoP: format_missing_skills @ agent/skill_commands.py:format_missing_skills */
char *sc_format_missing_skills(const char *const *missing)
{
    int n = 0;
    if (missing) while (missing[n]) n++;

    size_t cap = 32;
    for (int i = 0; i < n; i++) cap += strlen(missing[i]) * 2 + 8;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';

    size_t plen = strlen(SC_AMBIGUOUS_SKILL_PREFIX);
    int first = 1;
    for (int i = 0; i < n; i++) {
        if (strncmp(missing[i], SC_AMBIGUOUS_SKILL_PREFIX, plen) == 0) {
            if (!first) strcat(out, "; ");
            strcat(out, missing[i]);
            first = 0;
        }
    }
    /* the unknown list, in the ORIGINAL order */
    int any_unknown = 0;
    for (int i = 0; i < n; i++)
        if (strncmp(missing[i], SC_AMBIGUOUS_SKILL_PREFIX, plen) != 0) { any_unknown = 1; break; }
    if (any_unknown) {
        if (!first) strcat(out, "; ");
        strcat(out, "Unknown skill(s): ");
        int f2 = 1;
        for (int i = 0; i < n; i++) {
            if (strncmp(missing[i], SC_AMBIGUOUS_SKILL_PREFIX, plen) == 0) continue;
            if (!f2) strcat(out, ", ");
            strcat(out, missing[i]);
            f2 = 0;
        }
    }
    return out;
}
