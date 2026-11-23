/*
 * __init__.c — C port of upstream Hermes persistence/__init__.py.
 *
 * PoP: w7_init_681f06_all_names @ persistence/__init__.py:__all__
 * PoP: w7_init_681f06_all_count @ persistence/__init__.py:len(__all__)
 * PoP: w7_init_681f06_all_contains @ persistence/__init__.py:name in __all__
 *
 * persistence/__init__.py is a pure re-export surface; the testable contract is
 * its ordered __all__ list and membership/count queries over it.
 */
#include "noc_w7_init_681f06.h"
#include "hermes_json.h"
#include <string.h>

static const char *const ALL_NAMES[] = {
    "MUTATION_GUARD_REFUSALS",
    "PERSISTENCE_ERROR_CAUSES",
    "STORE_FAILURES",
    "AuthorityEpoch",
    "AuthorityRefusal",
    "AwaitableRepository",
    "AwaitableStore",
    "CapabilityUnsupported",
    "ClaimToken",
    "CompressionBusy",
    "CompressionClosed",
    "CompressionInProgress",
    "CompressionPublishRefused",
    "Conflict",
    "ContinuationToken",
    "CooldownRestoreRefused",
    "CorruptStoreError",
    "DeclarativeDrift",
    "DelegationId",
    "DeletedWalGeneration",
    "DiskFull",
    "ExecutionGeneration",
    "ExportTooLarge",
    "FenceStale",
    "HeldRowRewritten",
    "JsonValue",
    "MalformedSchema",
    "MessageId",
    "NotFound",
    "Page",
    "PageRequest",
    "PermanentStoreError",
    "PersistenceCapabilities",
    "PersistenceError",
    "ProfileIdentityCollision",
    "ProfileScope",
    "ProofRejected",
    "PruneRowStale",
    "PruneRowUnresolved",
    "RetiredSessionError",
    "RewindRefusalReason",
    "RewindRefused",
    "RowVisibility",
    "SearchIndexCorrupt",
    "SessionArtifacts",
    "SessionId",
    "SessionLedgered",
    "SessionRevision",
    "SessionRowMissing",
    "SessionWriteGuarded",
    "StaleFenceKind",
    "StorageHealth",
    "StorageProbe",
    "StorageState",
    "StoreContention",
    "StoreQuarantined",
    "StoreReplaced",
    "StoreUnavailable",
    "TopicAlreadyLinked",
    "TransientStoreError",
    "TurnLeaseLost",
    "UnknownExecution",
    "busy_refusal",
    "ensure_utc",
};
#define N_ALL_NAMES ((int)(sizeof(ALL_NAMES) / sizeof(ALL_NAMES[0])))

struct json_t *w7_init_681f06_all_names(void) {
    json_t *arr = json_array();
    for (int i = 0; i < N_ALL_NAMES; i++) json_append(arr, json_string(ALL_NAMES[i]));
    return arr;
}

int w7_init_681f06_all_count(void) { return N_ALL_NAMES; }

int w7_init_681f06_all_contains(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < N_ALL_NAMES; i++)
        if (!strcmp(ALL_NAMES[i], name)) return 1;
    return 0;
}
