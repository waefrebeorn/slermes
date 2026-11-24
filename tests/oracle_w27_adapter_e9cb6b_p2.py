#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/matrix/adapter.py (gate p2).

Config/env normalisation is driven through the live helpers with the fixture as
the only input: env-fed readers get the fixture value planted in a scratch
variable, and extra-fed readers get a stub config carrying exactly that value.
"""

import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_e9cb6b_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("HERMES_HOME", tempfile.mkdtemp(prefix="w27-oracle-home-"))

for _k in ("MATRIX_MAX_MESSAGE_LENGTH", "MATRIX_E2EE_MODE", "MATRIX_ENCRYPTION",
           "MATRIX_REQUIRE_MENTION", "MATRIX_THREAD_REQUIRE_MENTION",
           "MATRIX_AUTO_THREAD", "MATRIX_REACTIONS", "W27_ENV", "W27_NUM"):
    os.environ.pop(_k, None)

from plugins.platforms.matrix import adapter as live  # noqa: E402

ADAPTER = live.MatrixAdapter

WANTED = {
    "normalize_e2ee_mode", "resolve_e2ee_mode", "env_truthy", "env_number", "csv_set",
    "thread_root", "extra_truthy", "configured_bool", "parse_require_mention",
    "parse_thread_require_mention", "extract_server_ed25519", "is_connected",
    "session_scope", "reactions_enabled",
}


class _Cfg:
    def __init__(self, extra, token=None):
        self.extra = extra
        self.token = token


class _Keys:
    def __init__(self, keys):
        self.keys = {} if keys is None else keys


def _normalize(r):
    return live._normalize_e2ee_mode(r.get("value"))


def _resolve(r):
    extra = {}
    if r.get("explicit") is not None:
        extra["e2ee_mode"] = r.get("explicit")
    if r.get("legacy_enabled") is not None:
        extra["encryption"] = r.get("legacy_enabled")
    return live._resolve_e2ee_mode(extra)


def _env_truthy(r):
    v = r.get("value")
    os.environ.pop("W27_ENV", None)
    if v is not None:
        os.environ["W27_ENV"] = v if isinstance(v, str) else str(v)
        return live._env_truthy("W27_ENV", "")
    return live._env_truthy("W27_ENV", "")


def _env_number(r):
    v = r.get("value")
    cast = float if r.get("want_float") else int
    default = r.get("default")
    os.environ.pop("W27_NUM", None)
    if v is not None:
        os.environ["W27_NUM"] = v if isinstance(v, str) else str(v)
        out = live._env_number("W27_NUM", default, cast)
        os.environ.pop("W27_NUM", None)
        return out
    return live._env_number("W27_NUM", default, cast)


def _csv(r):
    return sorted(live._csv_set(r.get("raw")))


def _thread_root(r):
    rt = r.get("relates_to")
    return live._thread_root(rt if isinstance(rt, dict) else {})


def _extra_truthy(r):
    return ADAPTER._extra_truthy(_Cfg({"k": r.get("configured")}), "k", "W27_UNSET_ENV", "false")


def _configured_bool(r):
    return ADAPTER._configured_bool(_Cfg({"k": r.get("configured")}), "k")


def _require_mention(r):
    v = r.get("configured")
    return ADAPTER._parse_require_mention(_Cfg({} if v is None else {"require_mention": v}))


def _thread_require_mention(r):
    v = r.get("configured")
    return ADAPTER._parse_thread_require_mention(
        _Cfg({} if v is None else {"thread_require_mention": v}))


def _ed25519(r):
    return ADAPTER._extract_server_ed25519(_Keys(r.get("keys")))


def _connected(r):
    # The env rung of _is_connected must not decide the fixture.
    import hermes_cli.gateway as gateway_mod
    orig = gateway_mod.get_env_value
    gateway_mod.get_env_value = lambda key: ""
    try:
        cfg = _Cfg({} if r.get("homeserver") is None else {"homeserver": r.get("homeserver")},
                   token=r.get("token"))
        return live._is_connected(cfg)
    finally:
        gateway_mod.get_env_value = orig


def _session_scope(r):
    # The exact expression MatrixAdapter.__init__ evaluates (adapter.py:735-736).
    raw = str(r.get("raw")).strip().lower()
    return raw if raw in {"auto", "room", "thread"} else "auto"


def _reactions(r):
    # The exact expression MatrixAdapter.__init__ evaluates (adapter.py:738).
    return str(r.get("raw")).lower() not in {"false", "0", "no"}


CASES = {
    "normalize_e2ee_mode": _normalize,
    "resolve_e2ee_mode": _resolve,
    "env_truthy": _env_truthy,
    "env_number": _env_number,
    "csv_set": _csv,
    "thread_root": _thread_root,
    "extra_truthy": _extra_truthy,
    "configured_bool": _configured_bool,
    "parse_require_mention": _require_mention,
    "parse_thread_require_mention": _thread_require_mention,
    "extract_server_ed25519": _ed25519,
    "is_connected": _connected,
    "session_scope": _session_scope,
    "reactions_enabled": _reactions,
}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = CASES[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
