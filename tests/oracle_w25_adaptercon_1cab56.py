#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/adapter_config_readers.py
(gate w25_adaptercon_1cab56).

The live readers are imported and called with ``read_raw_config`` patched to
return the fixture's raw value under the key the reader looks it up in -- the
config file is the seam, the value is the input.  ``_resolve_exec_approval_admin_gate``
needs no patch: it takes ``extra`` as an argument.
"""
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(Path(tempfile.gettempdir()))

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_adaptercon_1cab56"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.config as hermes_config  # noqa: E402
from plugins.platforms.discord import adapter_config_readers as live  # noqa: E402
import agent.deadline as deadline  # noqa: E402
from gateway.slash_access import _coerce_id_list  # noqa: E402

WANTED = {"dm_role_auth_guild", "discord_prompt_timeout", "exec_approval_gate",
          "coerce_id_list", "constants"}

_MISSING = object()


def _with_config(cfg):
    """Patch the config reader, run the reader, restore.  ``cfg`` is the whole
    raw config.yaml tree the fixture describes."""

    class _Patch:
        def __enter__(self):
            self.real = hermes_config.read_raw_config
            hermes_config.read_raw_config = lambda: cfg
            return self

        def __exit__(self, *exc):
            hermes_config.read_raw_config = self.real
            return False

    return _Patch()


def dm_role_auth_guild(req):
    raw = req.get("raw")
    with _with_config({"discord": {"dm_role_auth_guild": raw}}):
        return live._read_dm_role_auth_guild()


def discord_prompt_timeout(req):
    raw = req.get("raw")
    with _with_config({"approvals": {"discord_prompt_timeout": raw}}):
        return live._read_discord_prompt_timeout()


def exec_approval_gate(req):
    require_admin, admin_ids = live._resolve_exec_approval_admin_gate(req.get("extra"))
    return {"require_admin": require_admin, "admin_user_ids": sorted(admin_ids)}


def coerce_id_list(req):
    return sorted(_coerce_id_list(req.get("raw")))


def constants(_req):
    return {
        "prompt_timeout_default": live._DISCORD_PROMPT_TIMEOUT_DEFAULT,
        "prompt_timeout_min": live._DISCORD_PROMPT_TIMEOUT_MIN,
        "prompt_timeout_max": live._DISCORD_PROMPT_TIMEOUT_MAX,
        "dm_role_auth_guild_key": "dm_role_auth_guild",
        "prompt_timeout_key": "discord_prompt_timeout",
        "require_admin_key": "require_admin_for_exec_approval",
        "allow_admin_from_key": "allow_admin_from",
    }


def expected(r):
    f = r.get("func")
    return {
        "dm_role_auth_guild": dm_role_auth_guild,
        "discord_prompt_timeout": discord_prompt_timeout,
        "exec_approval_gate": exec_approval_gate,
        "coerce_id_list": coerce_id_list,
        "constants": constants,
    }[f](r)


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
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
