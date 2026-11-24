#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_session_seed.py (w20_methodsses_a61454).

The live ``_persist_branch`` is imported straight out of the quarry and run for
real against a scripted persistence handle: the fixture says what the parent row
looks like, what the batch append returns (per-row outcome annotations) and
which write fails, and this oracle reports what the live function did -- the
NewSession it handed to ``sessions.create``, the rows it appended, the title
call it made, whether it compensated, and the text it re-raised.

Seams stubbed where the live body reads them: ``_persistence_for`` (the handle),
``_new_message`` (the server's row -> NewMessage builder), ``logger``,
``_discard_unadmitted`` (recorded), ``message_identity`` (the fixture's identity
map), ``sync_flushed_message_markers`` and ``is_disk_full_error``.
"""
import importlib
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

GATE = "w20_methodsses_a61454"
SESSION_FIELDS = ("session_id", "source", "model", "model_config", "parent_session_id",
                  "cwd", "profile_name", "user_id", "system_prompt")


class _NullLog:
    def debug(self, *a, **k):
        pass

    def warning(self, *a, **k):
        pass

    info = debug
    error = debug


class _Ctx:
    def __init__(self, value):
        self.value = value

    def __enter__(self):
        return self.value

    def __exit__(self, *exc):
        return False


class FakeOutcome:
    def __init__(self, annotation):
        self._annotation = annotation

    def as_row_annotation(self):
        return dict(self._annotation or {})


class FakeMessages:
    def __init__(self, state):
        self.state = state

    def append_batch(self, session_id, messages, chunk_rows=None):
        # References, not copies: the live body mutates these row dicts in place
        # from each outcome's annotation after the batch returns.
        self.state["append_calls"].append({"session_id": session_id,
                                           "messages": list(messages),
                                           "chunk_rows": chunk_rows})
        if self.state["fail"] == "append":
            raise RuntimeError(self.state["fail_message"])
        outcomes = [FakeOutcome(a) for a in self.state["annotations"]]
        return type("Batch", (), {"outcomes": outcomes})()


class FakeSessions:
    def __init__(self, state):
        self.state = state

    def get(self, key):
        if self.state["parent_error"]:
            raise RuntimeError("parent read failed")
        if self.state["parent"] is None:
            return None
        return type("Parent", (), {"system_prompt": self.state["parent"].get("system_prompt")})()

    def create(self, new_session):
        self.state["created"] = {f: getattr(new_session, f) for f in SESSION_FIELDS}

    def set_title(self, key, title):
        if self.state["fail"] == "title":
            raise RuntimeError(self.state["fail_message"])
        self.state["title"] = {"kind": "user", "title": title}

    def set_auto_title(self, key, title, source=None):
        if self.state["fail"] == "title":
            raise RuntimeError(self.state["fail_message"])
        self.state["title"] = {"kind": "auto", "title": title, "source": source.value}


class FakePersistence:
    def __init__(self, state):
        self.sessions = FakeSessions(state)
        self.messages = FakeMessages(state)


def run_branch(spec):
    """Drive the live _persist_branch once against a scripted persistence handle."""
    spec = spec if isinstance(spec, dict) else {}
    parent = spec.get("parent")
    state = {
        "parent": parent if isinstance(parent, dict) and not parent.get("error") else None,
        "parent_error": bool(isinstance(parent, dict) and parent.get("error")),
        "fail": spec.get("fail") or "none",
        "fail_message": spec.get("fail_message") or "transcript write failed",
        "annotations": spec.get("annotations") or [],
        "created": None,
        "title": None,
        "append_calls": [],
        "discarded": [],
    }
    mod = importlib.import_module("tui_gateway.methods_session_seed")
    mod.logger = _NullLog()
    persistence = FakePersistence(state)
    mod._persistence_for = lambda db: _Ctx(persistence)
    # The row dict itself: NewMessage is a builder, and the only thing this gate
    # observes is the row dict the live body keeps mutating after the batch.
    mod._new_message = lambda row: row
    mod._discard_unadmitted = lambda pers, key: state["discarded"].append(key)

    import agent.transcript_repair as repair
    repair.sync_flushed_message_markers = lambda history, rows: None
    import storage_errors
    storage_errors.is_disk_full_error = lambda exc: bool(spec.get("disk_full"))

    meta = sys.modules["agent.message_metadata"]
    orig = meta.message_identity
    it = iter(list(spec.get("identities") or []))
    meta.message_identity = lambda msg: next(it, {}) or {}
    raised = None
    try:
        mod._persist_branch(
            object(),
            spec.get("new_key") or "",
            spec.get("parent_key") or "",
            spec.get("title") or "",
            list(spec.get("history") or []),
            source=spec.get("source") or "",
            cwd=spec.get("cwd"),
            profile_name=spec.get("profile_name"),
            model=spec.get("model") or "",
            copy_fields=tuple(spec.get("copy_fields") or ()),
            compensate=bool(spec.get("compensate")),
            title_source=spec.get("title_source") or "derived",
            user_id=spec.get("user_id"),
        )
    except Exception as exc:  # noqa: BLE001 - the re-raise IS the case
        raised = str(exc)
    finally:
        meta.message_identity = orig

    call = state["append_calls"][0] if state["append_calls"] else None
    # The rows were handed over before any failure, and the annotations mutated
    # them in place, so the references are the final rows.
    rows = [dict(row) for row in call["messages"]] if call else []
    return {
        "session": state["created"],
        "rows": rows,
        "title": state["title"],
        "parent_read_failed": bool(state["parent_error"]),
        "compensated": bool(state["discarded"]),
        "raised": raised,
    }


def expected(r):
    f = r.get("func")
    if f == "title_is_user":
        return r.get("title_source") == "user"
    if f == "branch_rows":
        return run_branch({"new_key": "child", "parent_key": "parent", "source": "desktop",
                           "model": "m", "history": r.get("history") or [],
                           "copy_fields": r.get("copy_fields") or [],
                           "identities": r.get("identities") or []})["rows"]
    if f == "branch_persist":
        return run_branch(r.get("spec"))
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
        if not isinstance(r, dict) or r.get("func") is None:
            continue
        cases += 1
        try:
            exp = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if exp != got:
            mism += 1
            print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
