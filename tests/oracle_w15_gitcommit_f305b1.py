#!/usr/bin/env python3
"""Differential oracle for hermes_cli/git_commit_graph.py (gate w15_gitcommit_f305b1).

Stubs _subprocess_compat/_startup_fast at module load. A fake Path tree backed
by the fixture's path->mtime mapping replaces the filesystem; `now` and
`chain_text` are injected per case.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

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
    parent, _, child = name.rpartition(".")
    if parent and parent in sys.modules:
        setattr(sys.modules[parent], child, m)
    return m


hcli = _mod("hermes_cli")
hcli.__path__ = [Q + "/hermes_cli"]
_mod("hermes_cli._subprocess_compat",
     IS_WINDOWS=False,
     NO_LAZY_FETCH_ENV={},
     bounded_probe_run=lambda *a, **k: None,
     kill_process_tree=lambda *a, **k: None,
     noninteractive_git_env=lambda: {},
     windows_detach_flags_without_breakaway=lambda: 0,
     windows_detach_popen_kwargs=lambda: {})
_mod("hermes_cli._startup_fast", project_root_str=lambda: Q)

SRC = Q + "/hermes_cli/git_commit_graph.py"
spec = importlib.util.spec_from_file_location("hermes_cli.git_commit_graph", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.git_commit_graph"] = live
spec.loader.exec_module(live)

CHAIN = {"text": None}


class FP:
    def __init__(self, path, files):
        self.path = path.strip("/")
        self.files = files

    def __truediv__(self, other):
        return FP(self.path + "/" + str(other), self.files)

    def is_file(self):
        return self.path in self.files

    def stat(self):
        return SimpleNamespace(st_mtime=self.files[self.path])

    def read_text(self, encoding=None):
        if self.path.endswith("commit-graph-chain") and CHAIN["text"] is not None:
            return CHAIN["text"]
        raise OSError("no such file")


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
    try:
        files = r.get("files") or {}
        CHAIN["text"] = r.get("chain_text")
        live.time.time = lambda: r.get("now", 0.0)
        git_dir = FP("", files)
        if func == "commit_graph_stale":
            py = live.commit_graph_stale(git_dir)
        elif func == "graph_on_disk":
            py = live._graph_on_disk(git_dir)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w15_gitcommit_f305b1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
