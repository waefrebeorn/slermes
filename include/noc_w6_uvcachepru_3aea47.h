#ifndef NOC_W6_UVCACHEPRU_3AEA47_H
#define NOC_W6_UVCACHEPRU_3AEA47_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_uvcachepru_3aea47_normalized @ pm/uv_cache_prune.py:_normalized
 * PEP 503 normalization: re.sub(r"[-_.]+", "-", name).lower().
 * Runs of [-_.] collapse to a single '-', then ASCII-lowercased.
 * NULL name -> JSON null. Returns a new json string (caller frees). */
struct json_t *w6_uvcachepru_3aea47_normalized(const char *name);

/* PoP: w6_uvcachepru_3aea47_dist_name @ pm/uv_cache_prune.py:prune_uv_cache_to_lock.dist_name
 * `markers` is a JSON array of *.dist-info marker filenames (the live
 * bucket.glob("*.dist-info") results injected by the fixture). Applies the
 * live regex ([A-Za-z0-9_.]+?)-\d[^-]*\.dist-info to each marker in order and
 * returns the first normalized group(1), or JSON null when none matches. */
struct json_t *w6_uvcachepru_3aea47_dist_name(const struct json_t *markers);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_UVCACHEPRU_3AEA47_H */
