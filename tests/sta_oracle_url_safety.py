"""sta_oracle_url_safety.py — oracle for tools/url_safety.py query/redirect
helpers. Replays each harness line through the LIVE Python module and asserts
equality with the C output (0 mismatches = pass)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
import tools.url_safety as U

def py_redir(is_redirect, cur, loc, nxt):
    class N:
        url = nxt
    class R:
        pass
    r = R()
    r.is_redirect = is_redirect
    r.url = cur
    r.headers = {"location": loc} if loc else {}
    r.next_request = N() if nxt else None
    return U.redirect_target_from_response(r)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    rec = json.loads(line)
    fn = rec["fn"]; inp = rec["in"]
    if fn == "qp":
        exp = U.sensitive_query_param_name(inp)
        got = rec["out"]
    elif fn == "has":
        exp = bool(U.has_sensitive_query_params(inp))
        got = (rec["out"] == "true")
    elif fn == "redir":
        exp = py_redir(rec.get("is_redirect") == "true", inp,
                       rec.get("loc") or None, rec.get("nxt") or None)
        got = rec["out"]
    else:
        continue
    if exp != got:
        mism += 1
        print(f"MISMATCH fn={fn} in={inp!r} PY={exp!r} C={got!r}")
    n += 1
print(f"URL_SAFETY oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
