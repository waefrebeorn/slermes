#!/usr/bin/env python3
"""Differential oracle for hermes_cli/release_channels.py (gate w15_releasecha_5f3af5)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_releasecha_5f3af5"
sys.path.insert(0, QUARRY)

# parent-package bootstrap for the live hermes_cli module
_hc = types.ModuleType("hermes_cli")
_hc.__path__ = []
sys.modules["hermes_cli"] = _hc

SRC = QUARRY + "/hermes_cli/release_channels.py"
_spec = importlib.util.spec_from_file_location("release_channels_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["release_channels_live"] = live
_spec.loader.exec_module(live)

FUNCS = ("name", "repository", "sha256", "commit", "build_prefix", "channel_key",
         "package_versions", "artifact_key", "head", "identity")


def live_out(rec):
    f = rec.get("func")
    v = rec.get("value")
    if f == "name":
        return live.validate_name(v)
    if f == "repository":
        return live.validate_repository(v)
    if f == "sha256":
        return live.require_sha256(v)
    if f == "commit":
        return live.require_commit(v)
    if f == "build_prefix":
        return live.build_prefix(v)
    if f == "channel_key":
        return live.channel_key(v)
    if f == "package_versions":
        return list(live.package_versions(int(rec.get("sequence", "0"))))
    if f == "artifact_key":
        return live.artifact_key(v)
    if f == "head":
        return live._head(v)
    if f == "identity":
        return live.validate_identity(v)
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in FUNCS:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except live.ChannelError as exc:
            py = f"ChannelError: {exc}"
        except Exception as exc:
            print(f"LIVE ERROR {rec.get('func')}: {exc!r}", file=sys.stderr)
            mism += 1
            continue
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
