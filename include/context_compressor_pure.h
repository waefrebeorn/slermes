/*
 * context_compressor_pure.h — Pure, self-contained helpers from
 * agent/context_compressor.py that operate on json_t message arrays + strings.
 *
 * These are the "static helper" surface of the Python module that the
 * compression orchestration (llm_client.c / context_engine.c /
 * port_agent_context_compressor.c) calls. They carry NO agent handle, NO
 * async/IO, so they live in their own translation unit with minimal includes
 * (opaque json_t* + libjson + libredact + liberrorclassifier).
 *
 * Faithful ports of:
 *   _is_summary_access_or_quota_error
 *   _collect_ghosted_skill_names
 *   _skill_view_call_sites
 *   _collect_protected_skill_names
 *   _redact_compaction_text
 *   _serialized_length_for_budget
 *   _image_part_label
 *   _str_arg
 *   _summarize_tool_result_unguarded
 *   resolve_model_threshold
 */

#ifndef CONTEXT_COMPRESSOR_PURE_H
#define CONTEXT_COMPRESSOR_PURE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "hermes_json.h"  /* defines json_t (opaque struct) */

/* ── Stateful tracker structs (mirror ContextCompressor instance fields) ── */
/* Compression-guard state: cooldown + anti-thrashing counters. */
typedef struct {
    int   consecutive_timeout_failures;
    double summary_failure_cooldown_until;  /* monotonic seconds */
    int   ineffective_compression_count;
    int   fallback_compression_streak;
    char *last_summary_error;              /* strdup'd, owned */
} cc_compression_guards_t;

/* Request pre-flight projection: (rough, real) anchor pair. */
typedef struct {
    long pending_request_rough_tokens;
    long last_real_prompt_tokens;
    long last_rough_tokens_when_real_prompt_fit;
    long last_compression_rough_tokens;
} cc_request_tracker_t;

/* Compressor budget state (mirrors ContextCompressor's cached budget fields).
 * All *_resolved_* / *_budget_* fields are "unset sentinel" = -1 until lazily
 * resolved, exactly like the Python properties. */
typedef struct {
    /* immutable config (set once at construction) */
    const char *model;
    const char *base_url;
    const char *api_key;
    const char *provider;
    int  config_context_length;      /* 0 == None */
    double base_threshold_percent;   /* _base_threshold_percent (per-model resolved) */
    double summary_target_ratio;
    long  max_tokens;                /* 0 == None */
    long  threshold_tokens_cap;      /* -1 == None */
    /* lazily-resolved cache */
    long resolved_context_length;    /* -1 == unset */
    long threshold_tokens;           /* -1 == unset */
    long tail_token_budget;          /* -1 == unset */
    long max_summary_tokens;         /* -1 == unset */
    double threshold_percent;        /* effective (floored) percent */
} cc_compressor_budget_t;

/* Re-declare the small helpers owned by context.c we reuse (no agent dep). */
char *context_compressor__skill_pruned_marker(const char *skill_name);
int   context_compressor__extract_pruned_skill_names(const char *text,
                                                      char **out_names,
                                                      int *out_count,
                                                      int limit);
char *context_compressor_content_text(const json_t *content);

/* ── _is_summary_access_or_quota_error ─────────────────────────────────── */
/* True for non-retryable summary auth / permission / permanent-quota errors.
 * Built on the ported liberrorclassifier + status-code inspection, mirroring
 * Python's classify_api_error() + status_code + marker text scan. The caller
 * passes the status code, error body text, and an already-classified reason
 * (so this stays free of any exception/provider plumbing). */
bool cc_is_summary_access_or_quota_error(int status_code,
                                         const char *error_text,
                                         int classified_reason);

/* ── _skill_view_call_sites ───────────────────────────────────────────── */
/* Yield (message_index, skill_name) for every skill_view tool call.
 * Returns the count filled into sites[] (each entry is {index, name}); the
 * caller frees every name via free(). */
typedef struct { int index; char *name; } cc_skill_view_site_t;
int cc_skill_view_call_sites(const json_t *messages,
                             cc_skill_view_site_t *sites, int limit);

/* ── _collect_ghosted_skill_names ─────────────────────────────────────── */
/* Skill names whose instructions are about to be lost in compaction: either a
 * [SKILL_PRUNED: ...] marker already in a message, or a RAW skill_view body in
 * a tool message keyed by a captured call id. Fills out_names[] (caller frees
 * each). Returns count. */
int cc_collect_ghosted_skill_names(const json_t *turns,
                                    char **out_names, int limit);

/* ── _collect_protected_skill_names ───────────────────────────────────── */
/* Lower-cased set of skill names whose skill_view bodies must survive the
 * Phase-1 demotion. Fills out_names[] (caller frees each). Returns count. */
int cc_collect_protected_skill_names(const json_t *messages,
                                     int prune_boundary,
                                     char **out_names, int limit);

/* ── _redact_compaction_text ──────────────────────────────────────────── */
/* Redact text crossing a compaction summary boundary. Force mode: overrides
 * the security.redact_secrets=false opt-out (a summary is a persistence
 * boundary). Caller frees. */
char *cc_redact_compaction_text(const char *text);

/* ── _serialized_length_for_budget ────────────────────────────────────── */
/* Stable char-length for non-content replay/metadata fields. */
int cc_serialized_length_for_budget(const json_t *value);

/* ── _image_part_label ────────────────────────────────────────────────── */
/* Render a multimodal image part as a short text label. Caller frees. */
char *cc_image_part_label(const json_t *part);

/* ── _str_arg ──────────────────────────────────────────────────────────── */
/* Safely get a string tool arg, coercing non-str values to str. Caller frees. */
char *cc_str_arg(const json_t *args, const char *key, const char *def);

/* ── _summarize_tool_result_unguarded ─────────────────────────────────── */
/* Build the 1-line tool summary (unguarded). Caller frees. */
char *cc_summarize_tool_result_unguarded(const char *tool_name,
                                         const char *tool_args,
                                         const char *tool_content);

/* ── resolve_model_threshold ──────────────────────────────────────────── */
/* Resolve effective compression threshold for a model. Longest substring
 * key wins. */
double cc_resolve_model_threshold(const char *model,
                                  const char *const *threshold_keys,
                                  const double *threshold_vals,
                                  int threshold_count,
                                  double default_threshold);

/* Serialize message list text for the LM summarizer (compact form).
 * Reuses redaction, truncation, tool-call flattening, image labels.
 * Caller frees. */
char *cc_serialize_for_summary(const char *messages_json);

/* ── batch 2: string/summary/image constants + helpers ────────────────── */

/* Runtime constant arrays (mirror Python module-level literals). */
extern const char *cc_historical_summary_prefixes[];
extern const size_t cc_num_historical_prefixes;
extern const char *cc_image_part_types[];
extern const size_t cc_num_image_part_types;

/* Best-effort integer coercion for telemetry fields. Returns 1 + sets *out on
 * success, 0 otherwise (*out left 0). */
int  cc_safe_int(const json_t *value, int *out);

/* Canonical prune marker for skill_name (emit + survival-check share it). */
char *cc_skill_pruned_marker(const char *skill_name);
/* Skill names referenced by prune markers in text, in order (deduped). */
int  cc_extract_pruned_skill_names(const char *text, char **out_names,
                                  int *out_count, int limit);
/* Deterministic restore of prune markers the summarizer dropped: builds the
 * marker for each skill missing from *summary*, appends under "## Pruned
 * Skills", redacts, and sets *out (caller frees). Caller frees skill_names[]. */
int  cc_reinject_pruned_skill_markers(const char *summary,
                                     const char **skill_names,
                                     int skill_count, char **out);

/* Flatten message content to a single string for substring checks. Caller frees. */
char *cc_content_text_for_contains(const json_t *content);

/* Effective char-length of a message's content for token budgeting (images
 * count as CC_IMAGE_CHAR_EQUIVALENT each). */
int  cc_content_length_for_budget(const json_t *raw_content);

/* Multimodal image-part predicates. */
int  cc_is_image_part(const json_t *part);
int  cc_content_has_images(const json_t *content);
/* Return a NEW content array with image parts replaced by placeholder text, or
 * NULL when input is not a list / has no images (caller keeps original). */
json_t *cc_strip_images_from_content(const json_t *content);

/* Summary-prefix normalization (byte-pinned to Python's prefixes). */
int  cc_starts_with_summary_prefix(const char *text);
char *cc_strip_summary_prefix(const char *summary);   /* caller frees */
char *cc_with_summary_prefix(const char *summary);   /* caller frees */

/* ── _template_visible_role ──────────────────────────────────────────── */
/* Role as counted by strict chat-template alternation checks. Returns NULL for
 * tool messages and assistant messages carrying tool_calls (exempt from
 * alternation). Returns a borrowed pointer into message. */
const char *_template_visible_role(const json_t *message);

/* ── _reasoning_details_text_chars ───────────────────────────────────── */
/* Textual thinking chars inside a reasoning_details envelope (thinking/text/
 * summary fields only; envelope blobs are skipped). */
long _reasoning_details_text_chars(const json_t *value);

/* ── _rolling_summary_from_marker ────────────────────────────────────── */
/* Recover rolling-summary text from a summary marker's content (caller frees). */
char *_rolling_summary_from_marker(const char *content);

/* ── _render_micro_marker_content ────────────────────────────────────── */
/* Assemble the marker content wrapper around summary_text (caller frees). */
char *_render_micro_marker_content(const char *summary_text);

/* ── _merge_adjacent_user_turns ───────────────────────────────────────── */
/* Build a new list merging consecutive plain-text user turns (\n\n-joined),
 * skipping tool/summary messages; drops api_content on merged entries.
 * Returns the new count; *out_merged is the result array (caller frees via
 * json_free). */
int _merge_adjacent_user_turns(json_t *result, json_t **out_merged);

/* ── batch 3: summary classification + user-turn predicates + threshold math ─ */

const char *cc_get_tool_call_id(const json_t *tool_call);
const char *cc_get_tool_call_id_by_tc(const json_t *tc);

int  cc_has_compressed_summary_metadata(const json_t *message);
int  cc_is_context_summary_content(const json_t *content);
/* "standalone" / "merged" / NULL (caller frees non-NULL) */
char *cc_classify_summary_content(const json_t *content);
int  cc_is_context_summary_message(const json_t *message);

int  cc_is_blank_user_turn(const json_t *message);
int  cc_is_synthetic_compression_user_turn(const json_t *message);
int  cc_is_actionable_user_turn(const json_t *message);
int  cc_transcript_has_real_user_turn(const json_t *messages);
/* malloc'd int[] of removable indices, *out_count set; NULL when none */
int *cc_blank_echo_indices_after(const json_t *messages, int user_idx, int *out_count);

int  cc_find_context_summaries(const json_t *messages, int start, int end,
                               int *out_idx, char **out_bodies, int limit);
json_t *cc_strip_context_summary_handoff_message(const json_t *message); /* caller frees */

long  cc_coerce_threshold_tokens_cap(const json_t *value);   /* -1 == None */
double cc_effective_threshold_percent(long context_length, double threshold_percent);
long  cc_compute_threshold_tokens(long context_length, double threshold_percent,
                                  long max_tokens);
long  cc_apply_threshold_tokens_cap(long threshold_tokens, long threshold_tokens_cap,
                                    long context_length);

void cc_restart_handoff_probe_bounds(const json_t *messages, int protect_first_n,
                                     int *out_start, int *out_end);
int  cc_effective_protect_first_n(int compression_count, int has_previous_summary,
                                 int protect_first_n);

/* ── batch 4: micro-compaction marker helpers ─────────────────────────── */
/* Assemble the marker content wrapper around summary_text. Caller frees. */
char *cc_render_micro_marker_content(const char *summary_text);
/* Recover the rolling-summary text from a summary marker's content. Caller frees. */
char *cc_rolling_summary_from_marker(const char *content);
/* Index just past the summary marker in result (scan from the end), else fallback. */
int  cc_cursor_after_splice(const json_t *result, int fallback);
/* New message array with consecutive plain-text real user turns merged. Caller frees. */
json_t *cc_merge_adjacent_user_turns(const json_t *result);

/* ── batch 5: standalone handoff + user-turn predicates (pure) ─────────── */
/* True when a summary-bearing row still carries a live user ask after handoff stripped. */
bool cc_handoff_carries_live_user_content(const json_t *message);
/* True when the next model call would be driven ONLY by a handoff. */
bool cc_reference_handoff_would_drive_next_model_call(const json_t *messages);
/* True for human-authored user turns (not compaction scaffolding). */
bool cc_is_user_originated_turn(const json_t *message);
/* Textual thinking chars inside a reasoning_details envelope (envelope blobs skipped). */
int  cc_reasoning_details_text_chars(const json_t *value);

/* Copy a message for compaction assembly without persistence markers. Caller frees. */
json_t *cc_fresh_compaction_message_copy(const json_t *msg);

/* PoP: _serialize_for_summary @ agent/context_compressor.py:_serialize_for_summary */
char *cc_serialize_for_summary(const char *messages_json); /* caller frees; impl in port_context_compressor_ports.c */
/* Strip persistence markers from assembled messages; returns count stripped. */
int  cc_strip_persistence_markers(json_t *messages);

/* Role as counted by strict chat-template alternation checks (NULL if skipped). */
const char *cc_template_visible_role(const json_t *message);
/* True when a clarify user_response is runtime sentinel prose (timeout/no-user). */
bool cc_is_clarify_non_response_sentinel(const json_t *response);
/* Index of the newest assistant message, or -1. */
int  cc_last_assistant_index(const json_t *messages);
/* Strip stale per-turn replay items from assistant messages before the last
 * user turn. Mutates in place; returns count of messages pruned. */
int  cc_prune_stale_reasoning_replay(json_t *messages);

/* ── batch 6: stateful compression-guard + preflight trackers ───────────── */
/* Record a consecutive timeout failure using the shared cooldown ladder. */
void cc_record_timeout_failure(cc_compression_guards_t *g, const char *error);
/* Human-readable block reason ("cooldown:N"/"ineffective"/NULL) from guards. Caller frees. */
char *cc_compression_block_reason(const cc_compression_guards_t *g);
/* True when a guard is active (cooldown or anti-thrashing). */
bool cc_automatic_compression_blocked(const cc_compression_guards_t *g);
/* (should_compress, reason) tuple. Returns whether over threshold. */
bool cc_should_compress_info(const cc_compression_guards_t *g, long prompt_tokens,
                             long threshold_tokens, bool *out_should, char **out_reason);
/* Record the rough estimate of the request about to be sent. */
void cc_note_request_rough_estimate(cc_request_tracker_t *t, long rough_tokens);
/* NOTE: cc_should_defer_preflight_to_real_usage is owned by
 * port_context_compressor_ports.c (canonical PoP-tagged impl) — not here. */

/* ── batch 8: ContextCompressor budget property accessors (pure math) ──── */
/* Lazy-resolve + cache the model's context length (small-context floor applied).
 * Returns the resolved window. */
long cc_budget_resolve_context_length(cc_compressor_budget_t *b);
/* context_length getter/setter */
long cc_budget_context_length_get(cc_compressor_budget_t *b);
void cc_budget_context_length_set(cc_compressor_budget_t *b, long value);
/* threshold_tokens getter/setter (lazy compute + absolute cap) */
long cc_budget_threshold_tokens_get(cc_compressor_budget_t *b);
void cc_budget_threshold_tokens_set(cc_compressor_budget_t *b, long value);
/* tail_token_budget getter/setter (threshold_tokens * summary_target_ratio) */
long cc_budget_tail_token_budget_get(cc_compressor_budget_t *b);
void cc_budget_tail_token_budget_set(cc_compressor_budget_t *b, long value);
/* max_summary_tokens getter/setter (min(0.05*context_length, ceiling)) */
long cc_budget_max_summary_tokens_get(cc_compressor_budget_t *b);
void cc_budget_max_summary_tokens_set(cc_compressor_budget_t *b, long value);

#ifdef __cplusplus
}
#endif

#endif /* CONTEXT_COMPRESSOR_PURE_H */
