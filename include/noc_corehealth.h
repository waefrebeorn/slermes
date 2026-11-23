#ifndef NOC_COREHEALTH_H
#define NOC_COREHEALTH_H
#include <stddef.h>

/* One readiness probe, mirroring StorageProbe(status, detail).
 * detail points to borrowed storage; detail_is_null==1 means Python None. */
typedef struct {
    const char *status;
    const char *detail;
    int detail_is_null;
} corehealth_probe_t;

const char *corehealth_state_value(const char *name);
const char *corehealth_state_from_value(const char *value);
const char *corehealth_state_str(const char *name);
int corehealth_state_count(void);
corehealth_probe_t corehealth_make_probe(const char *status, const char *detail,
                                         int detail_is_null);
#endif
