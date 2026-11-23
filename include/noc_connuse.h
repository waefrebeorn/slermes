#ifndef NOC_CONNUSE_H
#define NOC_CONNUSE_H
/* C port of tools/connectors/data_use.py (gate: connuse).
 * _sink ContextVar modelled as an active flag + a set of recorded strings.
 * collect_data_use() -> begin()/end(); record_data_use() -> record().
 * Outside an active collect, record() is a no-op. */
#include <stddef.h>

void connuse_clear(void);   /* free the set and deactivate (harness hygiene) */
void connuse_begin(void);   /* collect_data_use: fresh empty sink, active */
void connuse_end(void);     /* context manager finally: deactivate */
void connuse_record(const char *item);   /* sink.update([item]) if active */
int  connuse_contains(const char *item);
size_t connuse_count(void);
const char *connuse_at(size_t i);        /* i-th recorded item */
void connuse_sort(void);                 /* sort items (Python sorted(set)) */

#endif /* NOC_CONNUSE_H */
