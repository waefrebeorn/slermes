/*
 * health.c — C port of upstream Hermes persistence/core/health.py.
 * StorageState(Enum) + StorageProbe(frozen dataclass) executable surface.
 * PoP: corehealth_state_value @ persistence/core/health.py:StorageState
 * PoP: corehealth_state_from_value @ persistence/core/health.py:StorageState
 * PoP: corehealth_state_str @ persistence/core/health.py:StorageState
 * PoP: corehealth_state_count @ persistence/core/health.py:StorageState
 * PoP: corehealth_make_probe @ persistence/core/health.py:StorageProbe
 */
#include "noc_corehealth.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *value;
} corehealth_member_t;

static const corehealth_member_t kMembers[] = {
    {"OK",      "ok"},
    {"CORRUPT", "corrupt"},
};
static const size_t kMemberCount = sizeof(kMembers) / sizeof(kMembers[0]);

const char *corehealth_state_value(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < kMemberCount; ++i)
        if (!strcmp(name, kMembers[i].name)) return kMembers[i].value;
    return NULL;
}

const char *corehealth_state_from_value(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < kMemberCount; ++i)
        if (!strcmp(value, kMembers[i].value)) return kMembers[i].name;
    return NULL;
}

const char *corehealth_state_str(const char *name) {
    static char buf[96];
    if (!name) return NULL;
    for (size_t i = 0; i < kMemberCount; ++i) {
        if (!strcmp(name, kMembers[i].name)) {
            snprintf(buf, sizeof(buf), "StorageState.%s", kMembers[i].name);
            return buf;
        }
    }
    return NULL;
}

int corehealth_state_count(void) {
    return (int)kMemberCount;
}

corehealth_probe_t corehealth_make_probe(const char *status, const char *detail,
                                         int detail_is_null) {
    corehealth_probe_t p;
    p.status = status ? status : "";
    p.detail = detail;
    p.detail_is_null = (detail_is_null || !detail) ? 1 : 0;
    return p;
}
