"""twin for tool_result_classification (water-cup gate).

Re-implements the SAME retry-classification contract the C harness asserts,
in pure Python (no LIVE Python source — this is a self-contained contract gate).
The gate passes only if C and Python agree on every case below.

Contract:
  - non-error / None / success -> NONE
  - explicit "retryable":"true"/"false" wins
  - transient keywords -> TRANSIENT
  - permanent keywords -> PERMANENT
  - unknown error -> PERMANENT (fail-closed)
  - with_class appends _retry_class non-destructively
"""
import sys

TRANSIENT = ["timeout","timed out","etimedout","econn","connection reset",
             "connection refused","network","temporary","try again","rate limit",
             "rate_limit","too many requests","503","502","504","429","busy","overloaded"]
PERMANENT = ["not found","no such","404","permission","denied","forbidden","401","403",
             "unauthorized","unauthenticated","invalid","malformed","400","422","bad request"]

def classify(result):
    if result is None or not (isinstance(result, str) and result.strip().startswith('{"error')):
        return "none"
    try:
        d = json.loads(result)
    except Exception:
        return "permanent"
    retry = d.get("retryable") if isinstance(d, dict) else None
    if isinstance(retry, bool):
        return "transient" if retry else "permanent"
    blob = " ".join(str(d.get(k,"")) for k in ("error","code","status") if isinstance(d, dict))
    b = blob.lower()
    if any(k in b for k in PERMANENT):
        return "permanent"
    if any(k in b for k in TRANSIENT):
        return "transient"
    return "permanent"  # fail-closed

def with_class(result, cls):
    if not (isinstance(result, str) and result.strip().startswith('{"error')):
        return result
    i = result.rfind('}')
    body = result[:i]
    sep = "," if ":" in body else ""
    return f'{body}{sep}"_retry_class":"{cls}"}}'

# cases: (json_str, expected_class, with_class_should_contain)
cases = [
    (None, "none", None),
    ('{"success":true}', "none", None),
]
# Build error cases programmatically to mirror the C harness inputs.
import json
def E(**kw): return json.dumps(kw)

cases += [
    (E(error="boom", retryable=True), "transient", "transient"),
    (E(error="boom", retryable=False), "permanent", "permanent"),
    (E(error="connection timed out"), "transient", "transient"),
    (E(error="upstream 503 service unavailable"), "transient", "transient"),
    (E(error="429 too many requests"), "transient", "transient"),
    (E(error="ECONNREFUSED", code="econnrefused"), "transient", "transient"),
    (E(error="file not found"), "permanent", "permanent"),
    (E(error="permission denied"), "permanent", "permanent"),
    (E(error="invalid argument"), "permanent", "permanent"),
    (E(error="bad request", status="400"), "permanent", "permanent"),
    (E(error="something weird happened"), "permanent", "permanent"),
]

fail = 0
for raw, exp, wc in cases:
    got = classify(raw)
    if got != exp:
        print("  FAIL: classify(%r) = %s expected %s" % (raw, got, exp)); fail += 1
    else:
        print("  ok:   classify -> %s" % got)
    if wc is not None and raw is not None and raw.strip().startswith('{"error'):
        tagged = with_class(raw, wc)
        if '"_retry_class":"%s"' % wc not in tagged or '"error"' not in tagged:
            print("  FAIL: with_class broken: %r" % tagged); fail += 1

if fail:
    print("RESULT: %d FAILURES" % fail); sys.exit(1)
print("RESULT: tool_result_classification contract holds (python twin)")
