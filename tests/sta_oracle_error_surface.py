#!/usr/bin/env python3
"""
sta_oracle_error_surface.py — Oracle for agent/error_surface.py.

Replays the LIVE predicates / _result_layer / build_error_surface_from_result
over the harness fixtures. Provider lookups are exercised only where Python
does not need them (provider="" and the pure layer table), because the C port
takes provider_label/auth_kind/api_key_env as parameters.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import agent.error_surface as ES  # noqa: E402


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE error_surface: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam = c["family"]
        arg = c.get("arg")
        if fam == "custom_endpoint":
            want = ES._is_custom_endpoint(arg)
        elif fam == "stream_drop":
            want = ES._looks_like_stream_drop(arg if arg is not None else "")
        elif fam == "disk_full":
            want = ES._disk_full(arg)
        elif fam == "layer":
            want = ES._result_layer(c["reason"], c["error"],
                                    c["provider"] or "")
        else:
            doc = json.loads(c["doc"]) if c["doc"] is not None else None
            want = ES.build_error_surface_from_result(doc, "", "")
        got = c["ret"] if "ret" in c else c.get("layer", c.get("surface"))
        if fam in ("custom_endpoint", "stream_drop", "disk_full"):
            got = bool(c["ret"])
            want = bool(want)
        if got != want:
            mism += 1
            print(f"MISMATCH {c['case']} [{fam}]:\n  C ={got}\n  PY={want}")

    print(f"ORACLE error_surface: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
