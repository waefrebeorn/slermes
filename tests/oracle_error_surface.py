#!/usr/bin/env python3
"""Oracle for agent/error_surface.py surface builder + predicates.

The live module imports cleanly; replay it directly. Harness families:
  custom_endpoint: {family, case, arg:<str|null>, ret:<0|1>}     _is_custom_endpoint
  stream_drop:     {family, case, arg:<str|null>, ret:<0|1>}     _looks_like_stream_drop
  disk_full:       {family, case, arg:<str|null>, ret:<0|1>}     _disk_full
  layer:           {family, case, reason, error, provider, layer}
                                                    _result_layer(reason,error,provider)
  result:          {family, case, doc:<json|null>, surface:<json|null>}
                         build_error_surface_from_result(doc, "", "")

The harness passes provider/model "" (and NULL label/auth_kind) to the C port's
esc_build_from_result, matching live _surface's `if provider:` guards (registry
label/auth lookups skipped for empty provider). Compare canonically.
"""
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

PYROOT = "C:/Users/eman5/hermes-agent"


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def main() -> int:
    sys.path.insert(0, PYROOT)
    import agent.error_surface as es

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        if fam == "custom_endpoint":
            want = 1 if es._is_custom_endpoint(c.get("arg")) else 0
            got = int(c.get("ret"))
            if want != got:
                mism += 1
                print(f"MISMATCH custom_endpoint {c.get('case')} arg={c.get('arg')!r} C={got} PY={want}")
        elif fam == "stream_drop":
            want = 1 if es._looks_like_stream_drop(c.get("arg") or "") else 0
            got = int(c.get("ret"))
            if want != got:
                mism += 1
                print(f"MISMATCH stream_drop {c.get('case')} arg={c.get('arg')!r} C={got} PY={want}")
        elif fam == "disk_full":
            want = 1 if es._disk_full(c.get("arg")) else 0
            got = int(c.get("ret"))
            if want != got:
                mism += 1
                print(f"MISMATCH disk_full {c.get('case')} arg={c.get('arg')!r} C={got} PY={want}")
        elif fam == "layer":
            want = es._result_layer(c.get("reason") or "", c.get("error") or "",
                                    c.get("provider") or "")
            got = c.get("layer")
            if want != got:
                mism += 1
                print(f"MISMATCH layer {c.get('case')} reason={c.get('reason')!r} "
                      f"err={c.get('error')!r} prov={c.get('provider')!r} C={got} PY={want}")
        elif fam == "result":
            doc_raw = c.get("doc")
            try:
                doc_val = json.loads(doc_raw) if isinstance(doc_raw, str) else doc_raw
            except ValueError:
                doc_val = doc_raw
            want = es.build_error_surface_from_result(doc_val, "", "")
            got = c.get("surface")
            if _canon(want) != _canon(got):
                mism += 1
                print(f"MISMATCH result {c.get('case')} doc={doc_raw}\n"
                      f"  C={got}\n  PY={want}")
    if total == 0:
        print("ORACLE error_surface: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE error_surface: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
