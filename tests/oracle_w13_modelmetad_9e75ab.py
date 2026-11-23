#!/usr/bin/env python3
"""Differential oracle for agent/model_metadata_cache.py (w13_modelmetad_9e75ab).

The cross-process lock and atomic YAML write are seams: hermes_cli.auth._file_lock
is stubbed to always succeed, utils.atomic_yaml_write captures the document,
and agent.model_metadata supplies a scripted document/load. The C port projects
the write payload as `out` (JSON null when the drop is a no-op).
"""
import contextlib
import copy
import importlib.util
import json
import pathlib
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


DOC = [{}]
CAPTURE = []
utils = _mod("utils", atomic_yaml_write=lambda path, doc: CAPTURE.append(copy.deepcopy(doc)))
agent = _mod("agent")
agent.__path__ = []
mm = _mod("agent.model_metadata",
          _get_context_cache_path=lambda: pathlib.Path("/tmp/context_length_cache.yaml"),
          _load_context_cache_document=lambda: DOC[0])
agent.model_metadata = mm
hc = _mod("hermes_cli")
hc.__path__ = []
hc.auth = _mod("hermes_cli.auth", _file_lock=lambda *a, **k: contextlib.nullcontext())

SRC = Q + "/agent/model_metadata_cache.py"
spec = importlib.util.spec_from_file_location("model_metadata_cache_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["model_metadata_cache_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "drop":
        continue
    DOC[0] = r.get("document") or {}
    CAPTURE.clear()
    try:
        live.drop_context_cache_rows(set(r.get("stale_keys") or []), r.get("label", ""))
    except Exception as exc:
        print(f"LIVE ERROR case={r}: {exc!r}", file=sys.stderr)
        continue
    py = CAPTURE[0] if CAPTURE else None
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w13_modelmetad_9e75ab: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
