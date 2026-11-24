#!/usr/bin/env python3
"""Differential oracle for hermes_cli/worktree_registry.py (w16_worktreere_0628f9).

Filesystem reads, .git existence and the cross-process lock are injected: utils
is a fake module, live._read/_repo_exists/registry_path are patched per case,
and os.path.realpath is the identity so Windows Path backslashes never appear.
"""
import contextlib
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

STATE = {"data": {}, "written": None}
utils = types.ModuleType("utils")
utils.read_json_or_empty = lambda path: STATE["data"]
utils.atomic_json_write = lambda path, data: STATE.__setitem__("written", data)
sys.modules["utils"] = utils

SRC = Q + "/hermes_cli/worktree_registry.py"
spec = importlib.util.spec_from_file_location("w16_worktreere_0628f9_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class FakePath:
    name = "worktree_repos.json"

    def with_name(self, name):
        return name


def patch_live(repos, existing, repo=None):
    saved = (live._read, live._repo_exists, live.registry_path, live.os)
    live._read = lambda path: repos
    live._repo_exists = lambda name: name in set(existing)
    live.registry_path = lambda: FakePath()
    if repo is not None:
        live.os = types.SimpleNamespace(
            path=types.SimpleNamespace(realpath=lambda p: p))
    return saved


def restore_live(saved):
    live._read, live._repo_exists, live.registry_path, live.os = saved


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func not in ("read", "registered", "merge"):
        continue
    cases += 1
    try:
        if func == "read":
            STATE["data"] = r.get("data", {})
            py = live._read(None)
        elif func == "registered":
            saved = patch_live(r.get("repos", []), r.get("existing", []))
            try:
                py = live.registered_repos()
            finally:
                restore_live(saved)
        else:
            repo = r.get("repo", "")
            saved = patch_live(r.get("current", []), r.get("existing", []), repo)
            auth = types.ModuleType("hermes_cli.auth")
            auth._file_lock = lambda *a, **k: contextlib.nullcontext()
            old_auth = sys.modules.get("hermes_cli.auth")
            sys.modules["hermes_cli.auth"] = auth
            STATE["written"] = None
            try:
                live.record_worktree_repo(repo)
            finally:
                restore_live(saved)
                if old_auth is None:
                    sys.modules.pop("hermes_cli.auth", None)
                else:
                    sys.modules["hermes_cli.auth"] = old_auth
            py = (STATE["written"]["repos"] if STATE["written"] is not None
                  else r.get("current", []))
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w16_worktreere_0628f9: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
