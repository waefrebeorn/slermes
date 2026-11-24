#ifndef NOC_W19_OPENAICLIE_7F783C_H
#define NOC_W19_OPENAICLIE_7F783C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_openaiclie_7f783c_realtime_url @ plugins/google_meet/realtime/openai_client.py:REALTIME_URL
 * The module's Realtime endpoint literal. */
const char *w19_openaiclie_7f783c_realtime_url(void);

/* PoP: w19_openaiclie_7f783c_decode_audio @ plugins/google_meet/realtime/openai_client.py:_decode_audio
 * ``base64.b64decode(b64) if b64 else b""`` with a ``(ValueError, TypeError)`` guard.
 * Returns ``{"hex": "<decoded bytes as lowercase hex>"}``; a decode failure is the
 * empty payload, exactly like the live helper's fallback.  Trailing bytes after the
 * final padding group are ignored. */
struct json_t *w19_openaiclie_7f783c_decode_audio(const char *b64);

/* PoP: w19_openaiclie_7f783c_read_queue @ plugins/google_meet/realtime/openai_client.py:RealtimeSpeaker._read_queue
 * Parse the JSONL queue (a leading UTF-8 BOM and blank/malformed lines are skipped,
 * non-object entries are skipped) and give every object without an ``id`` the given
 * uuid string.  ``exists`` is ``queue_path.exists()``; a missing file is ``[]``. */
struct json_t *w19_openaiclie_7f783c_read_queue(const char *text, int exists, const char *uuid_value);

/* PoP: w19_openaiclie_7f783c_rewrite_queue @ plugins/google_meet/realtime/openai_client.py:RealtimeSpeaker._rewrite_queue
 * ``"".join(json.dumps(e) + "\n" for e in remaining)``.  Numbers that are integral
 * serialize as Python ints (the JSON wire form does not distinguish 1 from 1.0).
 * Returns a new string (caller frees). */
char *w19_openaiclie_7f783c_rewrite_queue(const struct json_t *entries);

/* PoP: w19_openaiclie_7f783c_append_processed @ plugins/google_meet/realtime/openai_client.py:RealtimeSpeaker._append_processed
 * The ``{"id": entry.get("id"), "text": entry.get("text", ""), "result": result}``
 * record written as one JSONL line (``json.dumps(record) + "\n"``).  Returns a new
 * string (caller frees). */
char *w19_openaiclie_7f783c_append_processed(const struct json_t *entry, const struct json_t *result);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_OPENAICLIE_7F783C_H */
