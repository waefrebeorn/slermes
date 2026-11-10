#!/usr/bin/env python3
"""Oracle for agent/vault_login_classifier.py (classification + fill half).

The live module imports cleanly (stdlib + dataclasses + re + unicodedata), so
replay it directly — the most faithful oracle. Harness families (all emit their
inputs):

  login:   {family:"login",  case, control:<json>, ok, score, token}
             LoginControl.from_dict(control) -> classify_login_control
  checkout:{family:"checkout", case, control:<json>, ok, score, token}
             -> classify_checkout_control
  otp:     {family:"otp", case, code, fixtures:[<json>], controls:[{index,score,token}], fills}
             -> classify_otp_controls(fixtures) == controls; build_otp_fills(...) == fills
  select_password: {family:"select_password", case, password, fixtures:[<json>], out}
             classify fixtures -> select_password_fill
  select_checkout: {family:"select_checkout", case, fixtures, secret, map, out}
             classify fixtures -> select_checkout_fills

ok=1 means a classification was returned (score/token present). Compare scores
and tokens; the ok flag must agree with None-vs-not.
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
    import agent.vault_login_classifier as v

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

        if fam in ("login", "checkout"):
            fn = (v.classify_login_control if fam == "login"
                  else v.classify_checkout_control)
            ctrl = v.LoginControl.from_dict(c.get("control"))
            cl = fn(ctrl)
            got_ok = int(c.get("ok"))
            want_ok = 1 if cl is not None else 0
            if want_ok != got_ok:
                mism += 1
                print(f"MISMATCH {fam} ok case={c.get('case')} C={got_ok} PY={want_ok}")
                continue
            if cl is not None:
                if cl.score != int(c.get("score")) or cl.token != c.get("token"):
                    mism += 1
                    print(f"MISMATCH {fam} case={c.get('case')} "
                          f"C=({c.get('score')},{c.get('token')}) "
                          f"PY=({cl.score},{cl.token})")

        elif fam == "otp":
            ctls = [v.LoginControl.from_dict(fx) for fx in c.get("fixtures")]
            live_cl = v.classify_otp_controls(ctls)
            got_ctls = c.get("controls") or []
            # compare by the sorted (index,score,token) triples live vs C
            live_triples = sorted((cl.control.index, cl.score, cl.token)
                                  for cl in live_cl)
            got_triples = sorted((int(x["index"]), int(x["score"]), x["token"])
                                 for x in got_ctls)
            if live_triples != got_triples:
                mism += 1
                print(f"MISMATCH otp case={c.get('case')}\n  C={got_triples}\n  PY={live_triples}")
            # fills
            try:
                live_fills = v.build_otp_fills(live_cl, c.get("code") or "")
            except (ValueError, IndexError):
                live_fills = []
            want_fills = json.dumps(live_fills, sort_keys=True)
            got_fills = c.get("fills")
            got_obj = json.loads(got_fills) if isinstance(got_fills, str) else got_fills
            if _canon(json.loads(want_fills)) != _canon(got_obj):
                mism += 1
                print(f"MISMATCH otp fills case={c.get('case')}\n  C={got_obj}\n  PY={live_fills}")

        elif fam == "select_password":
            ctls = [v.LoginControl.from_dict(fx) for fx in c.get("fixtures")]
            cl = [x for x in (v.classify_login_control(x) for x in ctls) if x]
            live = v.select_password_fill(cl, c.get("password") or "")
            got = c.get("out")
            got_obj = json.loads(got) if isinstance(got, str) else got
            if _canon(live) != _canon(got_obj):
                mism += 1
                print(f"MISMATCH select_password case={c.get('case')}\n  C={got_obj}\n  PY={live}")

        elif fam == "select_checkout":
            ctls = [v.LoginControl.from_dict(fx) for fx in c.get("fixtures")]
            cl = [x for x in (v.classify_checkout_control(x) for x in ctls) if x]
            secret = c.get("secret") if isinstance(c.get("secret"), dict) else json.loads(c.get("secret") or "{}")
            kmap = c.get("map") if isinstance(c.get("map"), dict) else json.loads(c.get("map") or "{}")
            live = v.select_checkout_fills(cl, secret, kmap)
            got = c.get("out")
            got_obj = json.loads(got) if isinstance(got, str) else got
            if _canon(live) != _canon(got_obj):
                mism += 1
                print(f"MISMATCH select_checkout case={c.get('case')}\n  C={got_obj}\n  PY={live}")

    if total == 0:
        print("ORACLE vault_login_classifier: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE vault_login_classifier: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
