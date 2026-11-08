#!/usr/bin/env python3
"""Oracle for agent/redact.py redaction (port src/tools/browser_redact.c
browser_redact_sensitive_text / browser_redact_cdp_url). Direct-import the live
module (stdlib-only + agent.file_safety, imports clean in the gate interpreter).
The live fn bails early when _redact_enabled() is False (no config in the gate)
-- force redaction on so the emitted fixtures actually get tested, matching the
port's always-on semantics:
  sensitive: redact_sensitive_text(in, force=True)
  cdp:       redact_cdp_url(in)  (calls sensitive + URL/query/userinfo redaction)
The harness emits the input string and the C output; compare exactly."""
import json
import os
import sys

# Path.home() under native Windows needs the user env or it raises; default it
# so the oracle runs under any caller (suite temp-HOME included).
os.environ.setdefault("USERPROFILE", "C:/Users/eman5")
os.environ.setdefault("HOMEDRIVE", "C:")
os.environ.setdefault("HOMEPATH", "\\Users\\eman5")
os.environ.setdefault("LOCALAPPDATA", "C:/Users/eman5/AppData/Local")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def main() -> int:
    from agent.redact import redact_sensitive_text, redact_cdp_url
    # gate has no config -> squeeze the enabled check open so fixtures run
    try:
        import agent.redact as m
        m._redact_enabled = lambda *a, **k: True
    except Exception:
        pass
    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        val = c.get("in") or ""
        if fn == "sensitive":
            want = redact_sensitive_text(val, force=True) or ""
        elif fn == "cdp":
            want = redact_cdp_url(val) or ""
        else:
            continue
        if want != c.get("out"):
            mism += 1
            print(f"MISMATCH {fn} in={val!r} C={c.get('out')!r} PY={want!r}")
    if total == 0:
        print("ORACLE browser_redact: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE browser_redact: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
