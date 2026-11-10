#!/usr/bin/env python3
"""Oracle for gateway/hosted_room_execution_policy.py.

The live module imports cleanly (stdlib + its own gateway relatives), so replay
it directly — the most faithful oracle. Harness families:

  digest:   {family:"digest", case, profile, toolsets:[...], mode, iters, digest:<64-hex>}
              reconstruct unsigned = {version, target_profile, enabled_toolsets,
              approval_mode, max_iterations} and compare _policy_digest(unsigned).
  validate: {family:"validate", case, doc:<json>, ok:<0|1>, err:<str|null>,
              mapping:<json|null>}
              parse doc; RoomExecutionPolicy.from_mapping(mapping); on success
              as_mapping() must equal the emitted mapping (canonical). On error,
              ok=0 and the C error text is the Python exception text.
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
    if isinstance(x, list):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def main() -> int:
    sys.path.insert(0, PYROOT)
    import gateway.hosted_room_execution_policy as m
    from gateway.hosted_rooms_common import compact_json
    import hashlib

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
        if fam == "digest":
            unsigned = {
                "version": 1,
                "target_profile": c.get("profile"),
                "enabled_toolsets": list(c.get("toolsets") or []),
                "approval_mode": c.get("mode"),
                "max_iterations": int(c.get("iters")),
            }
            want = hashlib.sha256(
                compact_json(unsigned).encode("ascii")).hexdigest()
            got = c.get("digest")
            if want != got:
                mism += 1
                print(f"MISMATCH digest case={c.get('case')} C={got} PY={want}")
        elif fam == "validate":
            doc = c.get("doc")
            mapping = None
            try:
                mapping = json.loads(doc)
            except Exception:
                pass
            want_ok = 1
            want_mapping = None
            want_err = None
            try:
                p = m.RoomExecutionPolicy.from_mapping(mapping)
                want_mapping = p.as_mapping()
            except Exception as e:
                want_ok = 0
                want_err = str(e)
            got_ok = int(c.get("ok"))
            got_mapping = c.get("mapping")
            got_err = c.get("err")
            got_m = None
            if isinstance(got_mapping, dict):
                got_m = got_mapping
            elif isinstance(got_mapping, str):
                try:
                    got_m = json.loads(got_mapping)
                except Exception:
                    got_m = None
            ok_ok = want_ok == got_ok
            mp_ok = _canon(want_mapping) == _canon(got_m)
            err_ok = (want_ok == 1) or (want_err == (got_err or ""))
            if not (ok_ok and mp_ok and err_ok):
                mism += 1
                print(f"MISMATCH validate case={c.get('case')}\n"
                      f"  ok C={got_ok} PY={want_ok}  err C={got_err!r} PY={want_err!r}\n"
                      f"  mapping C={got_m and _canon(got_m) or got_m!r}\n"
                      f"  want_mapping={want_mapping and _canon(want_mapping) or want_mapping!r}")
    if total == 0:
        print("ORACLE hosted_room_execution_policy: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE hosted_room_execution_policy: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
