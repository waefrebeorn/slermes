#!/usr/bin/env python3
"""
sta_oracle_anthropic_credentials.py — Oracle for the classification core of
agent/anthropic_credentials.py.

Replays each case against the LIVE module: _is_oauth_token,
base_url_hostname/is_native_anthropic_host, anthropic_route_is_oauth,
is_claude_code_token_valid and claude_code_credentials_path.
"""
import json
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
os.environ.setdefault("USERPROFILE", r"C:\Users\eman5")
import types as _types  # noqa: E402
from urllib.parse import urlparse  # noqa: E402

# agent.anthropic_credentials imports utils -> hermes_yaml -> ruamel.yaml, which
# is not installed here. Stub utils with the same two definitions the module
# uses, transcribed from utils.py (``base_url_hostname`` is the reference this
# oracle checks the C port against).
_utils = _types.ModuleType("utils")


def _base_url_hostname(base_url):
    raw = (base_url or "").strip()
    parsed = urlparse(raw if "://" in raw else f"//{raw}") if raw else None
    return (parsed.hostname or "").lower().rstrip(".") if parsed else ""


_utils.base_url_hostname = _base_url_hostname
_utils.atomic_json_write = lambda *a, **k: None
sys.modules["utils"] = _utils

import agent.anthropic_credentials as AC  # noqa: E402
utils = _utils


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
        print("ORACLE anthropic_credentials: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "oauth_token":
            want = AC._is_oauth_token(c["token"])
        elif fam == "host":
            want_host = utils.base_url_hostname(c["url"] or "")
            if c["host"] != want_host:
                mism += 1
                print(f"MISMATCH host {c['url']!r}: C={c['host']!r} PY={want_host!r}")
                continue
            want = AC.is_native_anthropic_host(c["url"])
        elif fam == "route":
            want = AC.anthropic_route_is_oauth(c["url"], c["cred"], provider=c["provider"])
        elif fam == "token_valid":
            real = time.time
            time.time = lambda: c["now"] / 1000.0
            try:
                want = AC.is_claude_code_token_valid(json.loads(c["json"]))
            finally:
                time.time = real
        elif fam == "creds_path":
            dirs = {"default": None, "empty_env": "", "blank_env": "   ",
                    "absolute": "/custom/cfg", "tilde": "~/cfg",
                    "bare_tilde": "~", "relative": "rel/cfg"}
            old_dir = os.environ.get("CLAUDE_CONFIG_DIR")
            old_home = Path.home
            old_expand = os.path.expanduser

            def _fake_home():
                return Path("/home/u")

            Path.home = staticmethod(_fake_home)
            os.path.expanduser = lambda p: p.replace("~", "/home/u", 1)
            value = dirs[c["case"]]
            if value is None:
                os.environ.pop("CLAUDE_CONFIG_DIR", None)
            else:
                os.environ["CLAUDE_CONFIG_DIR"] = value
            try:
                want = str(AC.claude_code_credentials_path())
            finally:
                Path.home = old_home
                os.path.expanduser = old_expand
                if old_dir is None:
                    os.environ.pop("CLAUDE_CONFIG_DIR", None)
                else:
                    os.environ["CLAUDE_CONFIG_DIR"] = old_dir
            # Python runs on Windows here and Path renders backslashes; the
            # port builds POSIX-style paths, so compare with one separator.
            if c["out"].replace("\\", "/") != want.replace("\\", "/"):
                mism += 1
                print(f"MISMATCH creds_path {c['case']}: C={c['out']!r} PY={want!r}")
            continue
        else:
            continue

        got = c["ret"] if "ret" in c else c.get("native")
        if bool(got) != bool(want):
            mism += 1
            print(f"MISMATCH {c['case'] if 'case' in c else c.get('url') or c.get('token')} "
                  f"[{fam}]: C={got} PY={want}")

    print(f"ORACLE anthropic_credentials: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
