/*
 * noc_w33_pipeline_540ecf_p2.h - C surface for upstream
 * plugins/teams_pipeline/pipeline.py, shaping/stage half (gate
 * w33_pipeline_540ecf_p2).
 *
 * The pure payload-shaping and stage-decision core of the Teams meeting
 * pipeline: Notion rich-text/property/block shaping, the Linear issue body
 * decision (update vs create vs refuse), the rendered markdown, the summary
 * prompt, the LLM-summary parser and its heuristic fallback, participants and
 * call metrics, the transcript-vs-recording stage decision, the recording file
 * name/audio-path decision, the STT result classification and the sink order.
 * Network, LLM, STT, ffmpeg discovery and tempfile dirs are seams fed as
 * fixture inputs.
 */
#ifndef NOC_W33_PIPELINE_540ECF_P2_H
#define NOC_W33_PIPELINE_540ECF_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Clips the live code applies to shaped payloads. */
#define W33_PIPELINE_540ECF_P2_SUMMARY_CLIP    1900   /* Notion summary property */
#define W33_PIPELINE_540ECF_P2_TRANSCRIPT_CLIP 18000  /* summary prompt transcript */
#define W33_PIPELINE_540ECF_P2_HEURISTIC_CLIP  1200   /* heuristic summary */

/* {"content": "<text>"} -> {"rich_text": [{"text": {"content": ...}}]} */
json_t *w33_pipeline_540ecf_p2_rich_text(const json_t *request);

/* ["a", "b"] -> "- a\n- b" */
json_t *w33_pipeline_540ecf_p2_bullets(const json_t *items);

/* summary payload -> [["Summary", body], ["Key Decisions", body],
 * ["Action Items", body], ["Risks", body]] */
json_t *w33_pipeline_540ecf_p2_sections(const json_t *payload);

/* {"payload": ..., "config": ...} -> the Notion page properties object. */
json_t *w33_pipeline_540ecf_p2_notion_properties(const json_t *request);

/* summary payload -> the Notion body blocks (heading_2 + paragraph per section). */
json_t *w33_pipeline_540ecf_p2_notion_blocks(const json_t *payload);

/* {"payload": ..., "config": ..., "existing_record": ...} ->
 * {mode: "update"|"create"|"error", body: <graphql body>|null, error: <msg>|null} */
json_t *w33_pipeline_540ecf_p2_linear_body(const json_t *request);

/* summary payload -> the Linear/Notion markdown rendering. */
json_t *w33_pipeline_540ecf_p2_render_markdown(const json_t *request);

/* {"meeting_ref": ..., "transcript_text": ..., "artifacts": [...]} -> prompt. */
json_t *w33_pipeline_540ecf_p2_summary_prompt(const json_t *request);

/* {"content": "<model output>"} -> the cleaned summary dict (heuristic when
 * the text is empty or is not a JSON object). */
json_t *w33_pipeline_540ecf_p2_parse_summary(const json_t *request);

/* {"transcript_text": "..."} -> the heuristic summary dict. */
json_t *w33_pipeline_540ecf_p2_heuristic_summary(const json_t *request);

/* meeting_ref -> ["Display Name", ...] (displayName, else identity.user.displayName). */
json_t *w33_pipeline_540ecf_p2_participants(const json_t *meeting_ref);

/* [artifact, ...] -> merged call_record metrics + artifact_count. */
json_t *w33_pipeline_540ecf_p2_call_metrics(const json_t *artifacts);

/* {"meeting_ref", "notion", "linear", "teams_delivery"} ->
 * {title, start_time, end_time, notion_target, linear_target, teams_target}. */
json_t *w33_pipeline_540ecf_p2_payload_targets(const json_t *request);

/* {"config", "meeting_id", "status", "transcript_artifact", "transcript_text",
 *  "recordings"} -> {status, strategy, error_type, error_message} for the
 * transcript-first / recording->STT stage. */
json_t *w33_pipeline_540ecf_p2_transcript_decision(const json_t *request);

/* {"artifact_id", "display_name", "ffmpeg_extract_audio"} ->
 * {path, audio_path, suffix, needs_ffmpeg} (display_name is basename-sanitized;
 * a non-audio suffix needs ffmpeg extraction to .wav). */
json_t *w33_pipeline_540ecf_p2_recording_name(const json_t *request);

/* {"success", "error", "transcript"} -> {outcome, transcript, error_type,
 * error_message}: the STT result classification. */
json_t *w33_pipeline_540ecf_p2_stt_outcome(const json_t *result);

/* {"meeting_id", "notion", "linear", "teams_delivery",
 *  "have_notion_writer", "have_linear_writer", "have_teams_sender"} ->
 * [{name, status, sink_key}, ...] in sink order (Notion, Linear, Teams). */
json_t *w33_pipeline_540ecf_p2_sink_plan(const json_t *request);

#ifdef __cplusplus
}
#endif
#endif
