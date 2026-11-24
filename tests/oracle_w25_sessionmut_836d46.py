#!/usr/bin/env python3
"""Differential oracle for gateway/session_mutation_policy.py (gate w25_sessionmut_836d46).

The live prepare_* coroutines run for real; their seams are supplied as fixed
answers -- the restored policy blob, the cwd/git probe, the profile config read
and the policy rebind -- so what is compared is exactly the decision those values
make: the ACP edit gate, the no-op edit policy, the TERMINAL_CWD merge and the
pinned-vs-reset reasoning level.
"""
import asyncio
import contextlib
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_sessionmut_836d46"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import gateway.session_policy as sp  # noqa: E402
import hermes_constants  # noqa: E402
import tui_gateway.git_probe as git_probe  # noqa: E402
from gateway import session_mutation_policy as live  # noqa: E402
from persistence.core.errors import AuthorityRefusal  # noqa: E402

WANTED = {"edit_policy", "workspace", "reasoning", "constants"}
PREPARED = {"snapshot": {"receipt": {"policy": {}}}}


def _replace(obj, **changes):
    """dataclasses.replace over the seam's policy stand-in."""
    return SimpleNamespace(**{**vars(obj), **changes})


def _refused(exc):
    return {"status": "refused", "reason": exc.reason, "field": exc.field, "detail": exc.detail}


def edit_policy(req):
    saved = (sp.restore_policy, live.replace, live._rebind)
    old = SimpleNamespace(source=req.get("source"), edit_approval_policy=req.get("old_policy"))
    sp.restore_policy = lambda blob: old
    live.replace = _replace
    live._rebind = lambda authority, prepared, old, policy, **extra: {
        "status": "rebound", "edit_approval_policy": policy.edit_approval_policy}
    try:
        try:
            return asyncio.run(live.prepare_edit_policy(
                None, None, {"policy": req.get("payload_policy")}, PREPARED))
        except AuthorityRefusal as exc:
            return _refused(exc)
    finally:
        sp.restore_policy, live.replace, live._rebind = saved


@contextlib.contextmanager
def _probe(req):
    """The stat and git spawns of prepare_workspace's probe(), fixed."""
    saved = (os.path.abspath, os.path.expanduser, os.path.isdir,
             hermes_constants.translate_cwd_for_wsl_backend,
             git_probe.branch, git_probe.common_repo_root)
    os.path.abspath = lambda path: path
    os.path.expanduser = lambda path: path
    os.path.isdir = lambda path: req.get("isdir", False)
    hermes_constants.translate_cwd_for_wsl_backend = lambda cwd: cwd
    git_probe.branch = lambda cwd: req.get("branch")
    git_probe.common_repo_root = lambda cwd: req.get("root")
    try:
        yield
    finally:
        (os.path.abspath, os.path.expanduser, os.path.isdir,
         hermes_constants.translate_cwd_for_wsl_backend,
         git_probe.branch, git_probe.common_repo_root) = saved


def workspace(req):
    saved = (sp.restore_policy, live.replace, live._rebind)
    old = SimpleNamespace(terminal_json=req.get("terminal_json", "{}"), cwd="/previous")
    sp.restore_policy = lambda blob: old
    live.replace = _replace

    def rebind(authority, prepared, old, policy, **extra):
        return {"status": "ok", "cwd": policy.cwd,
                "terminal": json.loads(policy.terminal_json), "git": extra.get("git")}

    live._rebind = rebind
    try:
        with _probe(req):
            try:
                return asyncio.run(live.prepare_workspace(
                    None, None, {"cwd": req.get("cwd")}, PREPARED))
            except AuthorityRefusal as exc:
                return _refused(exc)
    finally:
        sp.restore_policy, live.replace, live._rebind = saved


def reasoning(req):
    saved = (sp.restore_policy, sp.pin_reasoning_effort, sp.unpin_reasoning,
             sp.profile_config, live.replace, live._rebind)
    calls = []
    frozen = req.get("frozen") or {}
    old = SimpleNamespace(config=lambda: dict(frozen),
                          ignore_user_config=req.get("ignore_user_config", False))
    profile = req.get("profile") if isinstance(req.get("profile"), dict) else {}
    sp.restore_policy = lambda blob: old
    sp.pin_reasoning_effort = lambda frozen_policy, effort, session=False: calls.append(
        {"op": "pin", "value": effort})
    sp.unpin_reasoning = lambda frozen_policy, profile: calls.append(
        {"op": "unpin", "profile": profile})
    sp.profile_config = lambda authority: dict(profile)
    live.replace = _replace
    live._rebind = lambda authority, prepared, old, policy, **extra: {
        "calls": calls, "frozen": old.config()}
    try:
        return asyncio.run(live.prepare_reasoning(
            None, None, {"effort": req.get("effort")}, PREPARED))
    finally:
        (sp.restore_policy, sp.pin_reasoning_effort, sp.unpin_reasoning,
         sp.profile_config, live.replace, live._rebind) = saved


def constants():
    return {"prepare": list(live.PREPARE)}


def expected(r):
    f = r.get("func")
    if f == "edit_policy":
        return edit_policy(r)
    if f == "workspace":
        return workspace(r)
    if f == "reasoning":
        return reasoning(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


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
