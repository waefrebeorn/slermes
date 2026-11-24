#ifndef NOC_W19_CONTEXTCOM_BBABD0_H
#define NOC_W19_CONTEXTCOM_BBABD0_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_contextcom_bbabd0_summary_view @ agent/context_compressor_window.py:summary_view
 * `plan` with the window [start, end) put back to the original tool results: what the
 * summarizer reads. Truncated plans (len(originals) != len(plan)) answer `plan` unchanged.
 * A tool row whose call is `clarify` keeps its pruned form (a correctness filter, not a cut). */
struct json_t *w19_contextcom_bbabd0_summary_view(struct json_t *plan, struct json_t *originals,
                                                 int start, int end);

/* PoP: w19_contextcom_bbabd0_restart_probe_bounds @ agent/context_compressor_window.py:CompactionWindowMixin._restart_handoff_probe_bounds
 * The bounded transcript region that can indicate restart decay: [start, end) as
 * [first_non_system, min(len, first_non_system + protect_first_n + extra)]. */
struct json_t *w19_contextcom_bbabd0_restart_probe_bounds(struct json_t *messages,
                                                         int protect_first_n, int extra);

/* PoP: w19_contextcom_bbabd0_effective_protect_first_n @ agent/context_compressor_window.py:CompactionWindowMixin._effective_protect_first_n
 * protect_first_n, decayed to 0 once compressed (or once a resumed head already carries a
 * handoff summary). A pinned task turn never decays: it answers 1. */
int w19_contextcom_bbabd0_effective_protect_first_n(int pin_task_turn, int compression_count,
                                                    int previous_summary, int protect_first_n,
                                                    struct json_t *messages, int extra);

/* PoP: w19_contextcom_bbabd0_protect_head_size @ agent/context_compressor_window.py:CompactionWindowMixin._protect_head_size
 * The system prompt row (if present) plus the effective protect_first_n extra rows. */
int w19_contextcom_bbabd0_protect_head_size(int pin_task_turn, int compression_count,
                                            int previous_summary, int protect_first_n,
                                            struct json_t *messages, int extra);

/* PoP: w19_contextcom_bbabd0_compress_window @ agent/context_compressor_window.py:CompactionWindowMixin._compress_window
 * (compress_start, compress_end) for the summarizable middle: the protected head (computed
 * here, then aligned forward past orphan tool results) and the tail cut. `tail_cut`,
 * `aligned_backward` and `floor` are the values the live collaborators (the token walk,
 * _align_boundary_backward, tail_cut_progress_floor) answer for this transcript, injected so
 * the bridge rule itself is what is compared. */
struct json_t *w19_contextcom_bbabd0_compress_window(struct json_t *messages, int pin_task_turn,
                                                     int compression_count, int previous_summary,
                                                     int protect_first_n, int extra, int tail_cut,
                                                     int last_user_idx, int aligned_backward,
                                                     int floor);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_CONTEXTCOM_BBABD0_H */
