#!/usr/bin/env python3
"""Differential oracle for upstream Hermes tools/approval_prompt.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
NAME = "w8_prompt_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/approval_prompt.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "consent":
        return live._consent(r.get("choice"), r.get("unresolved", "decline"))
    if f == "attempt":
        return live._attempt(r.get("name", ""), r.get("choice"), r.get("failure"), r.get("fallback"))
    if f == "unanswered":
        u = live.Unanswered(r.get("cause", ""))
        return {"choice": str(u), "cause": u.cause}
    if f == "cli_decision":
        choice = r.get("choice") or ""
        once_only = bool(r.get("once_only", False))
        allow_permanent = bool(r.get("allow_permanent", False))
        if once_only:
            m = {}
            for x in r.get("once_inputs", []): m[x] = "once"
            for x in r.get("deny_inputs", []): m[x] = "deny"
            return m.get(choice, "deny")
        d = live._CLI_CHOICE_ALIASES.get(choice, "deny")
        if d == "always" and not allow_permanent:
            d = "session"
        return d
    if f == "i18n_key":
        return live._CLI_CHOICE_I18N.get(r.get("decision"))
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE prompt: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)