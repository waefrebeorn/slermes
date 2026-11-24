/* noc_w19_doctor_60f499.h - public C API for persistence/sqlite/admin/_doctor.py port. */
#ifndef NOC_W19_DOCTOR_60F499_H
#define NOC_W19_DOCTOR_60F499_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_doctor_60f499_header_mode @ persistence/sqlite/admin/_doctor.py:_header_mode
 * Journal mode as the on-disk 20-byte file header reports it: "delete" / "wal" from
 * bytes 18/19, "not-a-database" without the "SQLite format 3\0" magic (or a short
 * header), else "unknown(<18>/<19>)". `header_hex` is the header bytes as hex text
 * (the byte-read seam is injected so no file is opened). */
json_t *w19_doctor_60f499_header_mode(const char *header_hex);

/* PoP: w19_doctor_60f499_refusal @ persistence/sqlite/admin/_doctor.py:_refusal
 * Admission checks independent of holder discovery: a non-store header refuses with
 * the "its file header reads ..." text, WAL on a cross-VM filesystem refuses with the
 * virtiofs/9p text, and every other pair answers JSON null (admitted). */
json_t *w19_doctor_60f499_refusal(const char *target, const char *current, int on_cross_vm_fs);

/* PoP: w19_doctor_60f499_newest_mtime_ns @ persistence/sqlite/admin/_doctor.py:_newest_mtime_ns
 * Max st_mtime_ns across the values (null = could not be stat'ed, ignored), or null when
 * none stat'ed. 64-bit nanoseconds travel as decimal strings; the answer is a decimal
 * string (or null), never a double. */
json_t *w19_doctor_60f499_newest_mtime_ns(json_t *values);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_DOCTOR_60F499_H */
