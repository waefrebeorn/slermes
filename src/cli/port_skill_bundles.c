/*
 * port_skill_bundles.c
 *
 * Pure helpers ported from agent/skill_bundles.py (14 defs, previously 0
 * ported). It is the backend behind hermes_cli/bundles.py.
 *
 *   sb_bundle_key          <- skill_bundles.py:get_bundle / bundle_path_for's key
 *   sb_bundle_path_for     <- skill_bundles.py:bundle_path_for
 *   sb_bundles_sig         <- skill_bundles.py:_bundles_sig
 *   sb_list_bundles_order  <- skill_bundles.py:list_bundles
 *   sb_resolve_command_key <- skill_bundles.py:resolve_bundle_command_key
 *
 * The scan / cache / YAML load half stays in Python - it is filesystem and
 * registry introspection. `_bundles_sig` takes the file CONTENTS as parameters
 * so the C side does not touch the disk; the Python reads the bytes and hands
 * them over, which is exactly the part the digest is computed from.
 */

#include "port_skill_bundles.h"

#include <stdlib.h>
#include <string.h>

#include "../lib/libhash/hash.h"
#include "port_skill_commands.h"

/* ===========================================================================
 *  get_bundle / bundle_path_for's key  (skill_bundles.py:get_bundle)
 *
 *      get_skill_bundles().get(f"/{_slugify(name)}")
 *
 *  The key is "/" + the slugified name. Caller frees.
 * =========================================================================== */

/* PoP: get_bundle (key form) @ agent/skill_bundles.py:get_bundle */
char *sb_bundle_key(const char *name)
{
    if (!name) return NULL;   /* the Python raises on None.lower() */
    char *slug = sc_slugify_skill_name(name);
    if (!slug) return NULL;
    size_t n = strlen(slug);
    char *key = (char *)malloc(n + 2);
    if (!key) { free(slug); return NULL; }
    key[0] = '/';
    memcpy(key + 1, slug, n + 1);
    free(slug);
    return key;
}

/* ===========================================================================
 *  bundle_path_for  (skill_bundles.py:bundle_path_for)
 *
 *      slug = _slugify(name)
 *      if not slug: raise ValueError(f"Bundle name {name!r} normalizes to an empty slug")
 *      return _bundles_dir() / f"{slug}.yaml"
 *
 *  `dir` stands in for _bundles_dir(). `err` receives 1 for the ValueError
 *  case. Caller frees.
 * =========================================================================== */

/* PoP: bundle_path_for @ agent/skill_bundles.py:bundle_path_for */
char *sb_bundle_path_for(const char *name, const char *dir, int *err)
{
    if (err) *err = 0;
    char *slug = sc_slugify_skill_name(name);
    if (!slug) { if (err) *err = 1; return NULL; }
    if (!slug[0]) {
        free(slug);
        if (err) *err = 1;
        return NULL;
    }
    const char *base = (dir && dir[0]) ? dir : ".";
    size_t n = strlen(base) + strlen(slug) + 8;
    char *out = (char *)malloc(n);
    if (!out) { free(slug); return NULL; }
    const char *sep = (base[strlen(base) - 1] == '/' || base[strlen(base) - 1] == '\\') ? "" : "/";
    snprintf(out, n, "%s%s%s.yaml", base, sep, slug);
    free(slug);
    return out;
}

/* ===========================================================================
 *  _bundles_sig  (skill_bundles.py:_bundles_sig)
 *
 *      entries = []
 *      for f in files:
 *          try: entries.append((str(f), hashlib.sha256(f.read_bytes()).hexdigest()))
 *          except OSError: continue
 *      return tuple(sorted(entries))
 *
 *  `paths` and `contents` are parallel NULL-terminated arrays; a NULL content
 *  is the OSError case and is skipped. Returns two parallel arrays
 *  (*out_paths, *out_digests) of *out_count entries, sorted by path then
 *  digest (Python's tuple sort). Caller frees both arrays and their strings.
 * =========================================================================== */

static int sb_cmp_entry(const void *a, const void *b)
{
    /* qsort passes a pointer to the ELEMENT, and each element is a
     * const char ** - so the cast must DEREFERENCE. Casting the slot's address
     * straight to const char *const * makes ea[0] read the pointer value itself
     * as a string (garbage), and the sort silently does nothing. */
    const char *const *ea = *(const char *const *const *)a;
    const char *const *eb = *(const char *const *const *)b;
    int c = strcmp(ea[0], eb[0]);
    if (c) return c;
    return strcmp(ea[1], eb[1]);
}

/* PoP: _bundles_sig @ agent/skill_bundles.py:_bundles_sig */
void sb_bundles_sig(const char *const *paths, const char *const *contents,
                    char ***out_paths, char ***out_digests, int *out_count)
{
    int n = 0;
    if (paths) while (paths[n]) n++;

    const char ***tmp = (const char ***)calloc((size_t)n + 1, sizeof(char **));
    if (!tmp) { *out_paths = NULL; *out_digests = NULL; *out_count = 0; return; }

    int k = 0;
    for (int i = 0; i < n; i++) {
        if (!contents || !contents[i]) continue;      /* OSError -> skipped */
        char *digest = hash_sha256_hex((const unsigned char *)contents[i], strlen(contents[i]));
        if (!digest) continue;
        tmp[k] = (const char **)calloc(2, sizeof(char *));
        if (!tmp[k]) { free(digest); continue; }
        tmp[k][0] = strdup(paths[i]);
        tmp[k][1] = digest;
        k++;
    }
    qsort(tmp, (size_t)k, sizeof(char **), sb_cmp_entry);

    char **op = (char **)calloc((size_t)k + 1, sizeof(char *));
    char **od = (char **)calloc((size_t)k + 1, sizeof(char *));
    if (!op || !od) {
        free(op); free(od);
        for (int i = 0; i < k; i++) { free((void *)tmp[i][0]); free((void *)tmp[i][1]); free(tmp[i]); }
        free(tmp);
        *out_paths = NULL; *out_digests = NULL; *out_count = 0;
        return;
    }
    for (int i = 0; i < k; i++) {
        op[i] = (char *)tmp[i][0];
        od[i] = (char *)tmp[i][1];
        free(tmp[i]);
    }
    free(tmp);
    *out_paths = op;
    *out_digests = od;
    *out_count = k;
}

/* ===========================================================================
 *  list_bundles  (skill_bundles.py:list_bundles)
 *
 *      return sorted(get_skill_bundles().values(), key=lambda b: b["slug"])
 *
 *  `slugs` is a parallel array to `bundles`; the returned ORDER array holds
 *  the indices sorted by slug. Caller frees the order array.
 * =========================================================================== */

/* PoP: list_bundles @ agent/skill_bundles.py:list_bundles */
int *sb_list_bundles_order(const char *const *slugs, int n)
{
    int *order = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
    if (!order) return NULL;
    for (int i = 0; i < n; i++) order[i] = i;

    /* insertion sort on the slug keys - stable, and n is a handful */
    for (int i = 1; i < n; i++) {
        int key = order[i];
        const char *ks = slugs[key] ? slugs[key] : "";
        int j = i - 1;
        while (j >= 0 && strcmp(slugs[order[j]] ? slugs[order[j]] : "", ks) > 0) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }
    return order;
}

/* ===========================================================================
 *  resolve_bundle_command_key  (skill_bundles.py:resolve_bundle_command_key)
 *
 *      return resolve_slash_key(command, get_skill_bundles())
 *
 *  `table` is the flat bundle-key array. Caller frees.
 * =========================================================================== */

/* PoP: resolve_bundle_command_key @ agent/skill_bundles.py:resolve_bundle_command_key */
char *sb_resolve_command_key(const char *command, const char *const *table)
{
    return sc_resolve_slash_key(command, table);
}
