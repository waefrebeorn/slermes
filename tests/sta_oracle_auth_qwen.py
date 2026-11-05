#!/usr/bin/env python3
"""
sta_oracle_auth_qwen.py — Oracle for the file/env half of hermes_cli/auth_qwen.py.

Replays the same cases against the LIVE module (path, base URL, expiry skew,
token-file reading with BOM, save round trip).
"""
import json
import os
import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
os.environ.setdefault("USERPROFILE", r"C:\Users\eman5")
import types as _types  # noqa: E402
import hermes_cli.auth_qwen as Q  # noqa: E402

# hermes_cli.auth pulls utils -> ruamel.yaml, which is not installed here. The
# module under test only imports two helpers from it lazily, so stub them.
AUTH = _types.ModuleType("hermes_cli.auth")
AUTH._qwen_cli_auth_path = lambda: Path.home() / ".qwen" / "oauth_creds.json"
def _save_private(target, data, **kw):
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(data, sort_keys=kw.get("sort_keys", False)))


AUTH._save_private_json = _save_private
sys.modules["hermes_cli.auth"] = AUTH


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
        print("ORACLE auth_qwen: 0 cases (link failure in disguise)")
        return 1

    tmp = Path(tempfile.mkdtemp(dir=".testbin"))
    (tmp / "bom.json").write_bytes(
        b"\xef\xbb\xbf" + b'{"access_token":"tok","expiry_date":123}')
    (tmp / "bad.json").write_text("{not json")
    (tmp / "array.json").write_text("[1,2,3]")

    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "path":
            want = "/home/u/.qwen/oauth_creds.json"
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH path: C={c['out']!r} PY={want!r}")
        elif fam == "base_url":
            old = os.environ.get("HERMES_QWEN_BASE_URL")
            if c["env"] is None:
                os.environ.pop("HERMES_QWEN_BASE_URL", None)
            else:
                os.environ["HERMES_QWEN_BASE_URL"] = c["env"]
            want = Q._qwen_base_url()
            if old is None:
                os.environ.pop("HERMES_QWEN_BASE_URL", None)
            else:
                os.environ["HERMES_QWEN_BASE_URL"] = old
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH base_url {c['case']}: C={c['out']!r} PY={want!r}")
        elif fam == "expiring":
            import time
            now = c["now"]
            real = time.time
            time.time = lambda: now
            try:
                want = Q._qwen_access_token_is_expiring(c["expiry"], c["skew"])
            finally:
                time.time = real
            if bool(c["ret"]) != bool(want):
                mism += 1
                print(f"MISMATCH expiring {c['case']}: C={c['ret']} PY={want}")
        elif fam == "read":
            which = {
                "bom": tmp / "bom.json",
                "bad_json": tmp / "bad.json",
                "not_object": tmp / "array.json",
                "missing": tmp / "missing.json",
            }[c["case"]]
            AUTH._qwen_cli_auth_path = lambda p=which: p
            try:
                doc = Q._read_qwen_cli_tokens()
                want_class, want_fields = "none", doc
            except Exception as exc:
                text = str(exc)
                if "not found" in text:
                    want_class = "not_found"
                elif "Failed to read" in text:
                    want_class = "read_failed"
                elif "Invalid Qwen CLI credentials" in text:
                    want_class = "invalid"
                else:
                    want_class = "other"
                want_fields = None
            if bool(c["ok"]) != (want_fields is not None):
                mism += 1
                print(f"MISMATCH read {c['case']}: C ok={c['ok']} PY ok={want_fields is not None}")
            elif c["err_class"] != want_class:
                mism += 1
                print(f"MISMATCH read {c['case']}.err_class: C={c['err_class']} PY={want_class}")
            elif want_fields is not None and c["fields"] != want_fields:
                mism += 1
                print(f"MISMATCH read {c['case']}.fields: C={c['fields']} PY={want_fields}")
        elif fam == "save":
            target = tmp / "saved" / "creds.json"
            AUTH._qwen_cli_auth_path = lambda: target
            Q._save_qwen_cli_tokens({"access_token": "a", "expiry_date": 42})
            want = json.loads(target.read_text())
            if not c["ok"] or not c["read_ok"]:
                mism += 1
                print(f"MISMATCH save: C ok={c['ok']} read_ok={c['read_ok']}")
            elif c["access_token"] != want.get("access_token"):
                mism += 1
                print(f"MISMATCH save.token: C={c['access_token']} PY={want.get('access_token')}")

    print(f"ORACLE auth_qwen: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
