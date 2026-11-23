#ifndef NOC_W14_UPDATETAK_CE4155_H
#define NOC_W14_UPDATETAK_CE4155_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_updatetak_ce4155_sha_valid @ hermes_cli/_update_takeover.py:_SHA
 * Mirrors _SHA.fullmatch(s) for the module regex [0-9a-f]{40|[0-9a-f]{64}.
 * Returns 1/0. */
int w14_updatetak_ce4155_sha_valid(const char *s);

/* PoP: w14_updatetak_ce4155_head_sha @ hermes_cli/_update_takeover.py:_head_sha
 *
 * Faithful seam-scoped port of _head_sha's decision core. `facts` carries the
 * external facts the live function reads (fixture-injected): "git_rc" (int),
 * "git_stdout" (str), "head" (HEAD file text), and optional "loose" (loose ref
 * file text) / "packed" (packed-refs text). Returns the matched SHA or "".
 * Caller frees. The subprocess/filesystem surface itself is excluded. */
char *w14_updatetak_ce4155_head_sha(const struct json_t *facts);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_UPDATETAK_CE4155_H */
