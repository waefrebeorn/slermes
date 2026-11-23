/*
 * backup_sqlite.c — C home for upstream Hermes hermes_cli/backup_sqlite.py.
 *
 * PoP: w10_backupsqli_6ad966_uses_wal @ hermes_cli/backup_sqlite.py:_uses_wal
 * PoP: w10_backupsqli_6ad966_pages_per_second @ hermes_cli/backup_sqlite.py:_CopyRate.pages_per_second
 * PoP: w10_backupsqli_6ad966_rollback_hold @ hermes_cli/backup_sqlite.py:_stepped_rollback_journal_copy
 *
 * Faithful C11 port of the pure WAL-header check and the copy-rate arithmetic
 * that drives the rollback-journal refusal decision. SQLite I/O, threads and
 * signals are out of scope for this pure gate.
 */
#include "noc_w10_backupsqli_6ad966.h"

#include "hermes_json.h"

#include <stddef.h>

#define W10_WAL_HEADER_BYTES 19
#define W10_MIN_SECONDS 1e-6

static double w10_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

static unsigned char w10_byte_at(const json_t *head, size_t i) {
    const json_t *v = json_get(head, i);
    if (!v || v->type != JSON_NUMBER) return 0;
    return (unsigned char)((long long)v->num_val & 0xFF);
}

struct json_t *w10_backupsqli_6ad966_uses_wal(const struct json_t *head) {
    unsigned char buf[W10_WAL_HEADER_BYTES] = {0};
    size_t n = (head && head->type == JSON_ARRAY) ? json_len(head) : 0;
    if (n > W10_WAL_HEADER_BYTES) n = W10_WAL_HEADER_BYTES; /* file.read(19) */
    for (size_t i = 0; i < n; ++i) buf[i] = w10_byte_at(head, i);
    return json_bool(n == W10_WAL_HEADER_BYTES && buf[18] == 2);
}

static double w10_pps(double pages, double seconds) {
    double s = seconds < W10_MIN_SECONDS ? W10_MIN_SECONDS : seconds;
    return pages / s;
}

struct json_t *w10_backupsqli_6ad966_pages_per_second(const struct json_t *pages,
                                                      const struct json_t *seconds) {
    return json_number(w10_pps(w10_num(pages), w10_num(seconds)));
}

struct json_t *w10_backupsqli_6ad966_rollback_hold(const struct json_t *total,
                                                   const struct json_t *pages,
                                                   const struct json_t *seconds) {
    double pps = w10_pps(w10_num(pages), w10_num(seconds));
    if (pps < 1.0) pps = 1.0;
    return json_number(w10_num(total) / pps);
}
