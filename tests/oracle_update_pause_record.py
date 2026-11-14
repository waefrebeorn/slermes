#!/usr/bin/env python3
"""Oracle for the pure slice of hermes_cli/update_pause_record.py.

The harness (t_port_update_pause_record.c) emits one JSON line per case with a
"fn" discriminator. The incarnation probe is a seam on both sides: the case
carries the verdict the C callback returns for even/odd pids, and Python feeds
the same verdict to `identity_is_live`'s `is not False` rule.
"""
import hashlib
import json
import os
import sys
import tempfile

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from hermes_cli import update_lock as _ul          # noqa: E402
from hermes_cli import update_pause_record as upr  # noqa: E402

# The module imports update_lock lazily inside its functions; bind it so the
# monkeypatch below is visible to them.
upr.update_lock = _ul

TMP = tempfile.mkdtemp(prefix="upr_oracle_")


def normcase(p):
    """os.path.normcase on Windows: lowercase and '/' -> '\\'."""
    return p.replace("/", "\\").lower()


def main() -> int:
    total = mism = 0
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
        i = c.get("in") or {}
        try:
            if fn == "install_key":
                want = hashlib.sha256(normcase(i["root"]).encode("utf-8")).hexdigest()[:12]
            elif fn == "digest_file":
                path = os.path.join(TMP, i["name"])
                if i["missing"]:
                    want = None
                else:
                    with open(path, "wb") as fh:
                        fh.write(i["bytes"].encode("utf-8"))
                    want = hashlib.sha256(i["bytes"].encode("utf-8")).hexdigest()
            elif fn == "has_work":
                want = upr._has_work(i["token"])
            elif fn == "without":
                pids = {str(p) for p in i["pids"]}
                tok = i["token"]
                want = upr._without(tok, pids)
            elif fn == "accepted_path":
                want = str(upr._accepted_path(upr.Path(i["path"])))
            elif fn == "retired_path":
                want = str(upr._retired_path(upr.Path(i["path"])))
            elif fn == "identity_is_live":
                verdict = i["verdict"]          # True / False / None
                ident = i.get("ident")
                got = {"v": verdict}

                def fake_live(pid, ct, _g=got):
                    return _g["v"]

                upr.update_lock.incarnation_live = fake_live
                want = upr.identity_is_live(ident)
            elif fn == "live_pids":
                ve, vo = i["v_even"], i["v_odd"]

                def fake_live(pid, ct, _ve=ve, _vo=vo):
                    return _ve if int(pid) % 2 == 0 else _vo

                _ul.incarnation_live = fake_live
                want = sorted(upr._live_pids(i["token"]))
            elif fn == "holder":
                body = i.get("body")
                held = upr._holder(upr.Path(i["src"]), body)
                want = held
            elif fn == "split_draining":
                ve, vo = i["v_even"], i["v_odd"]

                def fake_live(pid, ct, _ve=ve, _vo=vo):
                    return _ve if int(pid) % 2 == 0 else _vo

                _ul.incarnation_live = fake_live
                tok = i["token"]
                draining = upr.split_draining(tok)
                want = {"draining": draining, "token": tok}
            elif fn == "merge_into":
                tok = i.get("token")
                if tok is None:
                    want = upr.merge_into(None, i["adopted"])
                else:
                    want = upr.merge_into(tok, i["adopted"])
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        raw = c["out"]
        got = raw["str"] if isinstance(raw, dict) and set(raw) == {"str"} else raw
        if got != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:300]} "
                  f"PY={want!r}")
    if total == 0:
        print("ORACLE update_pause_record: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE update_pause_record: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
