"""twin for tool_dispatch_retry (water-cup gate).

Re-implements the agent loop's retry POLICY in Python (the consumer of
tool_classify_error): on a TRANSIENT error, re-dispatch up to TOOL_RETRY_MAX
times; on PERMANENT/unknown/success, do NOT retry. Mirrors harness.c (which uses
the REAL C classifier). Gate passes only if C and Python agree on the dispatch
counts below.
"""
import sys

TOOL_RETRY_MAX = 3

def classify(result):
    if not (isinstance(result, str) and result.strip().startswith('{"error')):
        return "none"
    import json
    try:
        d = json.loads(result)
    except Exception:
        return "permanent"
    retry = d.get("retryable") if isinstance(d, dict) else None
    if isinstance(retry, bool):
        return "transient" if retry else "permanent"
    blob = " ".join(str(d.get(k,"")) for k in ("error","code","status"))
    b = blob.lower()
    PERM = ["not found","permission","denied","forbidden","401","403","unauthorized","invalid","400","422"]
    TRAN = ["timeout","econn","network","rate limit","503","502","504","429","busy"]
    if any(k in b for k in PERM): return "permanent"
    if any(k in b for k in TRAN): return "transient"
    return "permanent"

def dispatch_with_retry(script, idx):
    """script: list of result strings; idx: mutable [0]. Returns (calls, final)."""
    calls = 0
    result = script[idx[0]]; idx[0]+=1; calls+=1
    attempt = 1
    while attempt < TOOL_RETRY_MAX:
        if classify(result) != "transient": break
        result = script[idx[0]]; idx[0]+=1; calls+=1
        attempt+=1
    return calls, result

cases = [
    (["{\"error\":\"timeout\",\"_retry_class\":\"transient\"}",
      "{\"error\":\"timeout\",\"_retry_class\":\"transient\"}",
      "{\"error\":\"timeout\",\"_retry_class\":\"transient\"}"], TOOL_RETRY_MAX, "transient"),
    (["{\"error\":\"not found\",\"_retry_class\":\"permanent\"}"], 1, "permanent"),
    (["{\"success\":true}"], 1, "success"),
    (["{\"error\":\"timeout\",\"_retry_class\":\"transient\"}","{\"success\":true}"], 2, "success"),
    (["{\"error\":\"something weird\"}"], 1, "permanent"),
]

fail = 0
for script, exp_calls, exp_final_kind in cases:
    idx = [0]
    calls, final = dispatch_with_retry(script, idx)
    if calls != exp_calls:
        print("  FAIL: calls=%d expected %d for %r" % (calls, exp_calls, script)); fail+=1
    else:
        print("  ok:   calls=%d (%s)" % (calls, exp_final_kind))

if fail:
    print("RESULT: %d FAILURES" % fail); sys.exit(1)
print("RESULT: tool_dispatch_retry policy correct (python twin)")
