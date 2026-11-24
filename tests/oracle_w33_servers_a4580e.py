#!/usr/bin/env python3
"""Differential oracle for agent/lsp/servers.py (gate w33_servers_a4580e).

The live registry module is imported as a package module (its ``agent.lsp``
context supplies ``workspace.nearest_root``) and only its pure value paths are
called.  The three process/file seams it reaches for are replaced with
deterministic answers, never with a different implementation:

  * ``nearest_root`` -> the fixture's two probe answers, so
    ``_markers_root``/``_root_or_workspace`` decide over values instead of a
    directory walk;
  * ``_which`` / ``_find_binary`` -> the fixture's resolved binary path;
  * ``_make_spec`` -> records the argv/init/seed it was handed, instead of
    importing ``pm.env_for`` and reading the operator's environment.

Everything compared is otherwise the live code: ``_file_ext_or_basename``,
``LANGUAGE_BY_EXT``/``language_id_for``, ``ServerDef.matches``, ``SERVERS``,
``find_server_for_file``, ``custom_servers``, ``UNTRUSTED_SAFE_SERVERS`` and
the positional assembly of ``_simple_spawn``/``_custom_spawn``.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_servers_a4580e"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.lsp import servers as live  # noqa: E402

WANTED = {
    "ext_or_basename",
    "language_id",
    "language_table",
    "matches",
    "find_server",
    "registry",
    "custom_servers",
    "root_decision",
    "untrusted_safe",
    "simple_spawn",
    "custom_argv",
}

_NOOP_ROOT = lambda fp, ws: None  # noqa: E731  (ServerDef construction only)


def desc(d):
    return {
        "server_id": d.server_id,
        "extensions": list(d.extensions),
        "seed_first_push": bool(d.seed_first_push),
        "multi_root": bool(d.multi_root),
        "description": d.description,
        "language_id": d.language_id,
    }


def ext_or_basename(req):
    return live._file_ext_or_basename(req.get("path") or "")


def language_id(req):
    lid = req.get("srv_language_id") or ""
    srv = live.ServerDef("probe", (), _NOOP_ROOT, _NOOP_ROOT, language_id=str(lid)) if lid else None
    return live.language_id_for(req.get("path") or "", srv)


def matches(req):
    srv = live.ServerDef("probe", tuple(req.get("extensions") or ()), _NOOP_ROOT, _NOOP_ROOT)
    return bool(srv.matches(req.get("path") or ""))


def find_server(req):
    cfg = req.get("custom")
    pool = (live.custom_servers(cfg) if cfg is not None else []) + list(live.SERVERS)
    hit = live.find_server_for_file(req.get("path") or "", pool)
    return None if hit is None else desc(hit)


def custom_servers(req):
    return [desc(d) for d in live.custom_servers(req.get("cfg"))]


def root_decision(req):
    ws = req.get("workspace")
    ws = "" if ws is None else str(ws)
    found = req.get("found")
    plain = req.get("found_plain")
    probes = []

    def fake_nearest_root(file_path, markers, excludes=(), ceiling=None):
        probes.append(1)
        return found if len(probes) == 1 else plain

    old = live.nearest_root
    live.nearest_root = fake_nearest_root
    try:
        markers = req.get("markers")
        resolve = live._markers_root(markers, tuple(req.get("excludes") or ()))
        return resolve(req.get("path") or "f.py", ws)
    finally:
        live.nearest_root = old


def simple_spawn(req):
    old_make, old_find = live._make_spec, live._find_binary
    live._make_spec = lambda root, ctx, sid, command, init=None, seed=False: {
        "command": list(command), "init": init, "seed": bool(seed)}
    live._find_binary = lambda ctx, sid, which, pkg: req.get("bin")
    try:
        root = str(req.get("root") or "")
        ctx = live.ServerContext(workspace_root=root, trusted=bool(req.get("trusted")))
        build = live._simple_spawn(
            str(req.get("server_id") or "probe"),
            tuple(req.get("which") or ("probe",)),
            tuple(req.get("args") or ()),
            req.get("install_pkg"),
            req.get("base_init"),
            bool(req.get("seed")),
            req.get("untrusted_init"),
        )
        return build(root, ctx)
    finally:
        live._make_spec, live._find_binary = old_make, old_find


def custom_argv(req):
    old_which, old_make = live._which, live._make_spec
    live._which = lambda *names: req.get("bin")
    live._make_spec = lambda root, ctx, sid, command, init=None, seed=False: {"command": list(command)}
    try:
        build = live._custom_spawn(str(req.get("server_id") or "probe"), list(req.get("command") or []))
        spec = build(str(req.get("root") or ""), live.ServerContext(workspace_root=""))
        return None if spec is None else spec["command"]
    finally:
        live._which, live._make_spec = old_which, old_make


def expected(r):
    f = r.get("func")
    if f == "ext_or_basename":
        return ext_or_basename(r)
    if f == "language_id":
        return language_id(r)
    if f == "language_table":
        return dict(live.LANGUAGE_BY_EXT)
    if f == "matches":
        return matches(r)
    if f == "find_server":
        return find_server(r)
    if f == "registry":
        return [desc(d) for d in live.SERVERS]
    if f == "custom_servers":
        return custom_servers(r)
    if f == "root_decision":
        return root_decision(r)
    if f == "untrusted_safe":
        return req_id(r) in live.UNTRUSTED_SAFE_SERVERS
    if f == "simple_spawn":
        return simple_spawn(r)
    if f == "custom_argv":
        return custom_argv(r)
    raise AssertionError("unknown func %r" % (f,))


def req_id(r):
    sid = r.get("server_id")
    return sid if isinstance(sid, str) else None


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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
