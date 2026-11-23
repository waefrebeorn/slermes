#!/usr/bin/env python3
"""Differential oracle for w14_sourcerele_3edd99 (hermes_cli/source_releases.py).

update_channel's tag regexes are seeded onto a stub module; the git-origin
regex is exercised through source_repository with a recording subprocess.
"""
import importlib.util
import json
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _pkg(name, path=None):
    m = sys.modules.get(name)
    if m is None:
        m = _mod(name)
    if not hasattr(m, "__path__"):
        m.__path__ = [path] if path else []
    return m


_pkg("hermes_cli", Q + "/hermes_cli")

_STABLE = re.compile(r"^v(?:0|[1-9]\d{0,2})\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)$")
_CANARY = re.compile(
    r"^v(?:0|[1-9]\d{0,2})\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\+canary\.20\d{6}T\d{6}Z$")


def _is_canary(tag):
    return isinstance(tag, str) and bool(_CANARY.fullmatch(tag.strip()))


_mod("hermes_cli.update_channel", STABLE_TAG_RE=_STABLE, is_canary_tag=_is_canary)

SRC = Q + "/hermes_cli/source_releases.py"
spec = importlib.util.spec_from_file_location("hermes_cli.source_releases_w14_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

_mod("hermes_cli.source_check", source_git_env=lambda: {})


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def compute(r):
    f = r.get("func")
    if f == "valid_tag":
        return live._valid_tag(r.get("tag"), r.get("channel", ""))
    if f == "published":
        return live._published(r.get("release"), r.get("channel", ""))
    if f == "origin":
        url = r.get("url", "")
        def fake_run(argv, **kw):
            return types.SimpleNamespace(returncode=0, stdout=url + "\n")
        old = live.subprocess
        live.subprocess = types.SimpleNamespace(run=fake_run, DEVNULL=None)
        try:
            return live.source_repository(git_cmd=["git"], cwd=None)
        finally:
            live.subprocess = old
    if f == "target_label":
        tgt = live.SourceTarget(
            requested_channel=r.get("channel", ""), channel=r.get("channel", ""),
            repository="NousResearch/hermes-agent", version=r.get("version"),
            commit=r.get("commit"), ahead=bool(r.get("ahead", False)))
        return tgt.label
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = canon(compute(r))
    except Exception as exc:
        print(f"LIVE ERROR {r.get('func')} case={r.get('case')}: {exc!r}",
              file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)

print(f"ORACLE w14_sourcerele_3edd99: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
