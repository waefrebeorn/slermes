#!/usr/bin/env python3
"""Differential oracle for gate w14_runtimeses_cd33f5 (agent/runtime_session_child.py)."""
import importlib.util
import json
import os
import sys
import types
from pathlib import PurePosixPath

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = [os.path.join(Q, *name.split("."))]
        sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("agent")
_pkg("persistence")
_pkg("persistence.workers")


class _FakeStore:
    """Inert base: captures the child's reserved scope/outbox without filesystem/RPC."""
    def __init__(self, rpc, scope, outbox_dir):
        self.rpc, self.scope, self.outbox_dir = rpc, scope, outbox_dir

    def create_session(self, *a, **kwargs):
        return kwargs


_mod("agent.runtime_session_store", RuntimeSessionStore=_FakeStore,
     call_off_event_loop=lambda *a, **k: None)
vocab = _mod("persistence.workers.vocabulary", WORKER_CONSTRUCTOR_CONFIG_KEYS=frozenset())

NAME = "w14_runtimeses_cd33f5_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "runtime_session_child.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "refusal":
        code = r.get("code", "")
        return live._REFUSALS.get(code, 'the gateway owner refused a subagent: ' + code)
    if f == "constructor_fields":
        vocab.WORKER_CONSTRUCTOR_CONFIG_KEYS = frozenset(r.get("allowed_keys") or [])
        fields = r.get("fields") or {}
        return live.ChildSessionStore.create_session(
            live.ChildSessionStore.__new__(live.ChildSessionStore), "sid", "subagent", **fields)
    if f == "outbox_name":
        live.secrets.token_urlsafe = lambda n: "fixed-secret"
        result = {"child": {"execution_id": r.get("execution_id", ""),
                            "scope_extra": r.get("scope_extra", {})}}
        live.call_off_event_loop = lambda *a, **k: result
        parent = types.SimpleNamespace(
            scope=r.get("scope", {}),
            path=PurePosixPath(r.get("parent_path", "/tmp/parent/pending.json")),
            _apply=lambda *a, **k: None,
            rpc=lambda *a, **k: None)
        store = live.reserve_child_store(parent)
        return store.outbox_dir.name
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        if not isinstance(r, dict) or "out" not in r:
            continue
        cases += 1
        py, got = compute(r), r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
    print("ORACLE w14_runtimeses_cd33f5: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
