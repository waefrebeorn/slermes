#!/usr/bin/env python3
"""Differential oracle for gate w16_vaultlogin_58f247 (agent/vault_login_classifier.py).

The LIVE module is stdlib-only and imports cleanly, so it is replayed directly
via importlib.spec_from_file_location (the most faithful oracle). Harness
families emit `func`, `out` and echo all input keys; `out` is compared to the
live classification/fill result.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "agent" / "vault_login_classifier.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)

_SPEC = importlib.util.spec_from_file_location(
    "w16_vault_login_classifier_live", str(Path(QUARRY) / "agent" / "vault_login_classifier.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, list): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def _cls(c):
    return {"index": c.control.index, "score": c.score, "token": c.token}


def expected(rec):
    f = rec.get("func")
    if f == "classify_login":
        c = mod.classify_login_control(mod.LoginControl.from_dict(rec.get("control") or {}))
        return {"ok": c is not None, "score": c.score if c else 0, "token": c.token if c else None}
    if f == "classify_checkout":
        c = mod.classify_checkout_control(mod.LoginControl.from_dict(rec.get("control") or {}))
        return {"ok": c is not None, "score": c.score if c else 0, "token": c.token if c else None}
    ctrls = [mod.LoginControl.from_dict(x) for x in (rec.get("controls") or [])]
    if f == "classify_otp":
        return [_cls(c) for c in mod.classify_otp_controls(ctrls)]
    if f == "select_password":
        cl = [x for x in (mod.classify_login_control(c) for c in ctrls) if x]
        return mod.select_password_fill(cl, rec.get("password") or "")
    if f == "select_checkout":
        cl = [x for x in (mod.classify_checkout_control(c) for c in ctrls) if x]
        return mod.select_checkout_fills(cl, rec.get("secret") or {}, rec.get("map") or {})
    if f == "build_otp_fills":
        cl = mod.classify_otp_controls(ctrls)
        try:
            return mod.build_otp_fills(cl, rec.get("code") or "")
        except (ValueError, IndexError):
            return []
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        rec = json.loads(line)
        n += 1
        want = expected(rec)
        got = rec.get("out")
        if _canon(got) != _canon(want):
            mism += 1
            if mism <= 16:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w16_vaultlogin_58f247: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
