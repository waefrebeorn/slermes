#ifndef NOC_W15_GITCOMMIT_F305B1_H
#define NOC_W15_GITCOMMIT_F305B1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_cli/git_commit_graph.py (gate w15_gitcommit_f305b1).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w15_gitcommit_f305b1_commit_graph_stale @ hermes_cli/git_commit_graph.py:commit_graph_stale
 * `files` maps relative path -> mtime (number); only present paths exist.
 * `now` injects time.time(). True when no graph or newest stamp is > 24h old. */
struct json_t *w15_gitcommit_f305b1_commit_graph_stale(const struct json_t *files,
                                                       double now);

/* PoP: w15_gitcommit_f305b1_graph_on_disk @ hermes_cli/git_commit_graph.py:_graph_on_disk
 * `chain_text` is the chain file's text (NULL when it cannot be read, which
 * selects the monolithic-graph fallback). True for a complete split chain or
 * a monolithic commit-graph. */
struct json_t *w15_gitcommit_f305b1_graph_on_disk(const struct json_t *files,
                                                  const char *chain_text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_GITCOMMIT_F305B1_H */
