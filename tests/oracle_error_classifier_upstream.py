#!/usr/bin/env python3
"""Oracle for agent/error_classifier.py upstream-provider detection
(port src/cli/port_agent_error_classifier.c). Direct-import the live module
(stdlib-only, imports cleanly) and replay the two live fns on the harness
emitted body/provider:
  is_upstream:   _is_openrouter_upstream_error(body, provider) -> bool
  provider_name: _extract_upstream_provider_name(body) -> str|None
The harness emits body as JSON (json.loads already gives the dict) and provider
as a plain string ("" for NULL)."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def main() -> int:
    from agent.error_classifier import (
        _is_openrouter_upstream_error, _extract_upstream_provider_name)
    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        c = json.loads(line)
        # harness emits body via a js() escaping emitter -> it lands as a JSON
        # *string*; the live fns need the parsed dict (the C side json_parses it).
        body = c.get("body")
        if isinstance(body, str):
            try:
                body = json.loads(body)
            except ValueError:
                pass
        c["body"] = body
        total += 1
        fn = c.get("fn")
        if fn == "is_upstream":
            provider = c.get("provider") or ""
            want = bool(_is_openrouter_upstream_error(c.get("body"), provider))
            if want != bool(c.get("out")):
                mism += 1
                print(f"MISMATCH is_upstream provider={provider!r} C={c.get('out')} PY={want}")
        elif fn == "provider_name":
            want = _extract_upstream_provider_name(c.get("body"))
            want = want if want is not None else ""
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH provider_name C={c.get('out')!r} PY={want!r}")
    if total == 0:
        print("ORACLE error_classifier_upstream: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE error_classifier_upstream: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
