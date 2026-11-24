/*
 * noc_w33_pipeline_540ecf_p1.h - C surface for upstream
 * plugins/teams_pipeline/pipeline.py, ingress half (gate
 * w33_pipeline_540ecf_p1).
 *
 * The pure notification/admission core of the Teams meeting pipeline: config
 * normalization, the Graph resource-path parser, the transcript-vs-artifact id
 * predicate, the notification -> (meeting_id, organizer, artifact ids) decision,
 * the dedupe/receipt key, the lifecycle gate that decides whether a delivery is
 * actually run, and the dedupe lookup over stored jobs.  Every I/O seam (the
 * store, the Graph client, the receipt recorder) stays with the caller.
 */
#ifndef NOC_W33_PIPELINE_540ECF_P1_H
#define NOC_W33_PIPELINE_540ECF_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* payload: a config dict (or null) -> the normalized TeamsPipelineConfig:
 * transcript_preferred, transcript_required, transcription_fallback,
 * stt_model, ffmpeg_extract_audio, transcript_min_chars, tmp_dir,
 * notion, linear, teams_delivery. */
json_t *w33_pipeline_540ecf_p1_config(const json_t *payload);

/* payload: a Graph payload -> organizer user id, or null.  meetingOrganizer /
 * organizer (user or identity.user) wins over organizerUserId. */
json_t *w33_pipeline_540ecf_p1_organizer_user_id(const json_t *payload);

/* request: {"resource": "<graph resource or @odata.id>"} ->
 * {organizer_user_id, meeting_id, transcript_id, recording_id} (nulls kept). */
json_t *w33_pipeline_540ecf_p1_parse_resource(const json_t *request);

/* request: {"value": "<id>", "odata_type": "<@odata.type>"} -> bool:
 * is this id a callTranscript artifact rather than an onlineMeeting? */
json_t *w33_pipeline_540ecf_p1_transcript_like(const json_t *request);

/* request: {"notification": {...}, "resource_data": {...}} -> bool: does
 * resourceData.id name an artifact instead of the meeting? */
json_t *w33_pipeline_540ecf_p1_is_artifact(const json_t *request);

/* notification: a Graph change notification ->
 * {meeting_id, organizer_user_id, extra_metadata{transcript_id?, recording_id?}}. */
json_t *w33_pipeline_540ecf_p1_meeting_ids(const json_t *notification);

/* notification -> the durable receipt/dedupe key ("id:<id>" or "sha256:<hex>"). */
json_t *w33_pipeline_540ecf_p1_receipt_key(const json_t *notification);

/* job: {"status": "<lifecycle state>"} -> "run" or "skip".  Terminal and
 * in-flight (non-"received") deliveries are returned as-is upstream. */
json_t *w33_pipeline_540ecf_p1_run_gate(const json_t *job);

/* request: {"jobs": [job payloads], "dedupe_key": "<key>"} -> the job_id of
 * the first stored job whose dedupe_key matches, or null.  (The live method
 * returns the re-validated job model; its observable identity is job_id.) */
json_t *w33_pipeline_540ecf_p1_find_dedupe(const json_t *request);

#ifdef __cplusplus
}
#endif
#endif
