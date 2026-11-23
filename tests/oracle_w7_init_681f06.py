#!/usr/bin/env python3
"""Differential oracle for persistence/__init__.py (w7_init_681f06)."""
import importlib.util
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, pkg=False, **attrs):
    m = types.ModuleType(name)
    if pkg:
        m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _attrs(name, pkg=False, names=()):
    _mod(name, pkg=pkg, **{n: n for n in names})


# The live __init__.py is a pure re-export surface: stub every submodule it
# imports from so exec exercises the real __all__ declaration without drivers.
_mod("persistence", pkg=True)
_mod("persistence.core", pkg=True)
_mod("persistence.gateway", pkg=True)
_mod("persistence.housekeeping", pkg=True)
_attrs("persistence.core.artifacts", names=["SessionArtifacts"])
_attrs("persistence.core.capabilities", names=["PersistenceCapabilities"])
_attrs("persistence.core.context", names=["ProfileScope"])
_attrs("persistence.core.conventions", names=["RowVisibility", "ensure_utc"])
_attrs("persistence.core.aio", names=["AwaitableRepository", "AwaitableStore"])
_attrs("persistence.core.errors", names=[
    "AuthorityRefusal", "CapabilityUnsupported", "CompressionBusy", "CompressionClosed",
    "CompressionInProgress", "CompressionPublishRefused", "Conflict", "CooldownRestoreRefused",
    "CorruptStoreError", "DeclarativeDrift", "DeletedWalGeneration", "DiskFull", "ExportTooLarge",
    "FenceStale", "HeldRowRewritten", "MUTATION_GUARD_REFUSALS", "MalformedSchema", "NotFound",
    "PERSISTENCE_ERROR_CAUSES", "PermanentStoreError", "PersistenceError", "ProofRejected",
    "PruneRowStale", "PruneRowUnresolved", "RetiredSessionError", "RewindRefusalReason",
    "RewindRefused", "STORE_FAILURES", "SearchIndexCorrupt", "SessionLedgered", "SessionRowMissing",
    "SessionWriteGuarded", "StaleFenceKind", "StoreContention", "StoreQuarantined", "StoreReplaced",
    "StoreUnavailable", "TransientStoreError", "TurnLeaseLost", "UnknownExecution", "busy_refusal",
])
_attrs("persistence.core.fences", names=["AuthorityEpoch", "ExecutionGeneration", "SessionRevision"])
_attrs("persistence.core.health", names=["StorageHealth", "StorageProbe", "StorageState"])
_attrs("persistence.core.identity", names=[
    "ClaimToken", "ContinuationToken", "DelegationId", "JsonValue", "MessageId", "Page",
    "PageRequest", "SessionId",
])
_attrs("persistence.gateway.routing", names=["TopicAlreadyLinked"])
_attrs("persistence.housekeeping.profile_identity", names=["ProfileIdentityCollision"])

SRC = Q + "/persistence/__init__.py"
spec = importlib.util.spec_from_file_location("persistence", SRC, submodule_search_locations=[])
live = importlib.util.module_from_spec(spec)
sys.modules["persistence"] = live
spec.loader.exec_module(live)

import json  # noqa: E402  (after submodule stubs)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "all_names":
            py = live.__all__
        elif func == "all_count":
            py = len(live.__all__)
        elif func == "all_contains":
            py = r.get("name") in live.__all__
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_init_681f06: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
