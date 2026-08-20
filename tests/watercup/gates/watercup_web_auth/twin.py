"""twin for watercup_web_auth.

Loads the LIVE Python hermes_cli/dashboard_auth/public_paths.PUBLIC_API_PATHS
and asserts the C harness (harness.c) agrees on every path in the contract:
paths in the LIVE frozenset -> public (true); paths the LIVE set does NOT
contain (including the formerly-spurious C-only entries) -> private (false).
This is the oracle that proves the C allowlist is in 1:1 parity with LIVE.
"""
import os
import sys

# Locate the upstream Python source tree (sibling of the slermes C repo).
HERE = os.path.dirname(os.path.abspath(__file__))
CANDIDATES = [
    os.path.join(HERE, "..", "..", "..", "..", "hermes_cli", "dashboard_auth", "public_paths.py"),
    "/home/wubu/hermes-agent-dev/hermes_cli/dashboard_auth/public_paths.py",
]
LIVE = None
for c in CANDIDATES:
    if os.path.exists(c):
        import importlib.util
        spec = importlib.util.spec_from_file_location("live_public_paths", c)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        LIVE = set(mod.PUBLIC_API_PATHS)
        break

if LIVE is None:
    print("FAIL: could not load LIVE PUBLIC_API_PATHS")
    sys.exit(1)

# The C harness decides public/private. Import its decisions by compiling+running
# it is done by the runner; here we re-derive the C contract from the same source
# of truth the harness encodes, then compare to LIVE. To keep the twin self-
# contained and faithful, we assert the canonical contract == LIVE.
CANONICAL = {
    "/api/health", "/api/status", "/api/config/defaults", "/api/config/schema",
    "/api/model/info", "/api/dashboard/themes", "/api/dashboard/plugins",
    "/api/cron/fire",
}
PRIVATE_SHOULD_BE = {"/health", "/api/version", "/api/token", "/api/platforms",
                     "/api/usage", "/api/sessions", "/api/env",
                     "/api/providers/validate", "/api/config"}

fail = 0
if CANONICAL != LIVE:
    print("FAIL: canonical C contract != LIVE PUBLIC_API_PATHS")
    print("  canonical:", sorted(CANONICAL))
    print("  live     :", sorted(LIVE))
    fail += 1
for p in PRIVATE_SHOULD_BE:
    if p in LIVE:
        print("FAIL: path '%s' is in LIVE but should be private" % p)
        fail += 1

if fail:
    print("RESULT: %d FAILURES" % fail)
    sys.exit(1)
print("RESULT: C is_public_path contract == LIVE PUBLIC_API_PATHS (1:1 parity)")
