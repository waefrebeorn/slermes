#!/usr/bin/env python3
"""Differential oracle for tools/connectors/mcp.py (gate w31_mcp_de9cb4_p2).

The live module is imported whole and every seam is answered from the fixture:

  * the catalog entry (``hermes_cli.mcp_catalog.get_entry``) becomes a bare
    namespace, so no manifest is read;
  * ``display_hermes_home`` / ``hermes_home_key`` become the fixture's home;
  * ``_RUNNERS`` holds a recording stand-in for the real runner, so the card's
    answer is folded into a real ``ConnectionOperation`` without a backend call;
  * ``_OBSERVE`` holds a recorder, so ``observe`` is asked whether it may read
    without reading anything.

Compared here: which declared credentials still block an install, the no-card
install refusal, the late-attempt key, a target's snapshot, the whole no-card
payload (``op_id``/``seq``/``deadline_at`` are identity the caller owns, so they
are normalised on both sides), the per-entry decision the card's answer makes,
and the three one-line predicates.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_mcp_de9cb4_p2"

sys.path.insert(0, str(REPO / "tests"))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.mcp_catalog as catalog_mod  # noqa: E402
import hermes_constants  # noqa: E402
from tools.connectors import mcp as live  # noqa: E402
from tools.connectors.contract import TargetState  # noqa: E402
from tools.connectors.operation import ConnectionOperation, DetachedOperation, Target  # noqa: E402

WANTED = {
    "missing_required", "install_now_message", "late_key", "target_snapshot",
    "no_card_result", "answer_plan", "installs_with_oauth", "actor_for",
    "observable",
}


class _Swapped:
    """Set module attributes, restore them on exit."""

    def __init__(self, patches):
        self.patches = patches

    def __enter__(self):
        self.old = [(owner, name, getattr(owner, name)) for owner, name, _ in self.patches]
        for owner, name, value in self.patches:
            setattr(owner, name, value)
        return self

    def __exit__(self, *exc):
        for owner, name, value in self.old:
            setattr(owner, name, value)
        return False


def _swapped(patches):
    return _Swapped(patches)


def _state(value):
    return TargetState(str(value or "pending"))


# --------------------------------------------------------------- functions --


def missing_required(req):
    declared = req.get("declared")
    declared = list(declared) if isinstance(declared, list) else []
    env = req.get("env") if isinstance(req.get("env"), dict) else {}
    backend = types.SimpleNamespace(required_env=lambda name: list(declared))
    runner = types.SimpleNamespace(backend=backend)
    target = types.SimpleNamespace(name="srv")
    return live._missing_required(runner, target, env)


def install_now_message(req):
    """``_install_now`` refuses when a required credential has no value."""
    declared = req.get("required")
    declared = list(declared) if isinstance(declared, list) else []
    home = str(req.get("home") or "")
    target = Target("srv", "mcp", "install")
    op = DetachedOperation([target], session_key="s1")
    backend = types.SimpleNamespace(required_env=lambda name: list(declared))
    runner = types.SimpleNamespace(action="install", work={}, approved_env={}, backend=backend)
    with _swapped([(hermes_constants, "display_hermes_home", lambda: home)]):
        live._install_now(runner, op, target, {})
    return target.detail


def late_key(req):
    op = types.SimpleNamespace(profile_key=str(req.get("profile_key") or ""),
                               session_key=str(req.get("session_key") or ""))
    home = str(req.get("home_key") or "")
    with _swapped([(live, "hermes_home_key", lambda: home)]):
        return list(live._late_key(op))


def _apply_target(t, f):
    t.state = _state(f.get("state"))
    t.detail = str(f.get("detail") or "")
    t.instructions = str(f.get("instructions") or "")
    t.connect_url = f.get("connect_url")
    t.connection_id = f.get("connection_id")
    t.attempt = f.get("attempt")
    t.required_env = list(f.get("required_env") or [])
    t.launch = f.get("launch")
    t.extra = dict(f.get("extra") or {})
    t.alias = f.get("alias")
    t.phase = f.get("phase")


def target_snapshot(req):
    f = req.get("target") if isinstance(req.get("target"), dict) else {}
    t = Target(str(f.get("name") or ""), str(f.get("kind") or "mcp"), str(f.get("action") or "authorize"))
    _apply_target(t, f)
    return t.snapshot(with_url=bool(req.get("with_url", True)))


class _NoCardRunner:
    """Stands in for the runner applying ``_NO_CARD``: it writes the row only."""

    def __init__(self, fields):
        self.fields = fields

    def run(self, table, operation, target, env=None):
        _apply_target(target, self.fields[target.name])


def no_card_result(req):
    raw = req.get("targets")
    rows = [r for r in raw if isinstance(r, dict)] if isinstance(raw, list) else []
    fields = {str(r.get("name") or ""): r for r in rows}
    runner = _NoCardRunner(fields)
    action = str(req.get("action") or "install")
    payload = json.loads(live._no_card_result(runner, list(fields), action, "s1"))
    # identity the caller owns: a fresh uuid, the write counter and the deadline
    payload["op_id"] = req.get("op_id")
    payload["seq"] = 0
    payload["deadline_at"] = float(req.get("deadline_at") or 0.0)
    return payload


class _RecordingOperation(ConnectionOperation):
    """A real operation that keeps the order its targets were addressed in."""

    def __init__(self, targets, log):
        super().__init__(targets, session_key="s1")
        self.log = log
        self._in_transition = False

    def target(self, name):
        # a transition looks the target up too; only the answer's own lookup counts
        if self._in_transition:
            return ConnectionOperation.target(self, name)
        row = {"name": name, "matched": False, "action": "none"}
        self.log.append(row)
        found = ConnectionOperation.target(self, name)
        if found is not None:
            row["matched"] = True
        return found

    def transition(self, name, to, actor, **fields):
        self._in_transition = True
        try:
            change = ConnectionOperation.transition(self, name, to, actor, **fields)
        finally:
            self._in_transition = False
        if to is TargetState.skipped and self.log:
            self.log[-1]["action"] = "skip"
        return change


class _RecordingRunner:
    """Records which table ``apply_answer`` reached for, without running it."""

    def __init__(self, log):
        self.log = log
        self.work = {}
        self.approved_env = {}

    def run(self, table, operation, target, env=None):
        if self.log:
            self.log[-1]["action"] = "approve" if table is live._APPROVE else "retry"


def answer_plan(req):
    raw = req.get("answer")
    raw = raw if isinstance(raw, str) else json.dumps(raw)
    targets = req.get("targets")
    targets = [t for t in targets if isinstance(t, dict)] if isinstance(targets, list) else []
    op_targets = [Target(str(t.get("name") or ""), str(t.get("kind") or "mcp"), "install",
                         state=_state(t.get("state"))) for t in targets]
    log = []
    op = _RecordingOperation(op_targets, log)
    runner = _RecordingRunner(log)
    runs = {op.op_id: runner} if req.get("runner", True) else {}
    with _swapped([(live, "_RUNNERS", runs)]):
        live.apply_answer(op, raw)
    return {"targets": log, "settled": bool(op.settled)}


def installs_with_oauth(req):
    entry = types.SimpleNamespace(auth=types.SimpleNamespace(type=req.get("auth_type"),
                                                             provider=req.get("auth_provider")))
    with _swapped([(catalog_mod, "get_entry", lambda name: entry)]):
        return bool(live._CatalogBackend().installs_with_oauth("srv"))


def actor_for(req):
    target = types.SimpleNamespace(state=_state(req.get("state")))
    return live._actor(target).value


def observable(req):
    op = ConnectionOperation([Target("srv", "mcp", "authorize", state=_state(req.get("state")))])
    if req.get("settled"):
        op.settled_at = 1.0
    fired = []
    runner = live._Runner("authorize", None)
    with _swapped([(live, "_OBSERVE",
                    {"authorize": lambda r, o, t: fired.append(t.name)})]):
        runner.observe(op)
    return bool(fired)


def expected(r):
    f = r.get("func")
    if f == "missing_required":
        return missing_required(r)
    if f == "install_now_message":
        return install_now_message(r)
    if f == "late_key":
        return late_key(r)
    if f == "target_snapshot":
        return target_snapshot(r)
    if f == "no_card_result":
        return no_card_result(r)
    if f == "answer_plan":
        return answer_plan(r)
    if f == "installs_with_oauth":
        return installs_with_oauth(r)
    if f == "actor_for":
        return actor_for(r)
    if f == "observable":
        return observable(r)
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
