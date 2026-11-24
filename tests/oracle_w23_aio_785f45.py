#!/usr/bin/env python3
"""Differential oracle for persistence/core/aio.py (gate w23_aio_785f45).

The module is a reflection layer, so the oracle builds the objects reflection
reads and lets the LIVE code decide:

  * a protocol class per fixture whose verb carries the declared return
    annotation (a real ``AbstractContextManager[dict]`` / ``Iterator[dict]``
    alias, a plain class, no annotation, or a *string* annotation that
    ``_refusal`` re-evals in the protocol module's globals -- resolvable or not);
  * a repository instance carrying (or not) the named attribute, callable or not;
  * a store instance for ``AwaitableStore.__getattr__``.

``AwaitableRepository.__getattr__``, ``_refusal``, ``AwaitableStore.__getattr__``
and ``_complete`` are the live functions; only the attribute facts are scripted.
``_repository_protocols`` (which imports ``.backend``) is stubbed to the empty
mapping, because the store view's protocol lookup is not what these cases pin.
"""
import collections.abc
import contextlib
import importlib.util
import json
import sys
import types
import typing
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "core" / "aio.py"
GATE = "w23_aio_785f45"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("aio_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["aio_live"] = live
_spec.loader.exec_module(live)

WANTED = {"no_awaitable_form", "refusal", "repository_attr", "store_attr", "complete"}

# The protocol module's globals: what a string annotation gets eval'd against.
PROTO_GLOBALS = {
    "contextlib": contextlib,
    "collections": collections.abc,
    "typing": typing,
    "Iterator": collections.abc.Iterator,
    "AbstractContextManager": contextlib.AbstractContextManager,
}

ANNOTATIONS = {
    "context_manager": "def verb(self) -> contextlib.AbstractContextManager[dict]: ...",
    "iterator": "def verb(self) -> collections.Iterator[dict]: ...",
    "iterable": "def verb(self) -> collections.Iterable[dict]: ...",
    "plain": "def verb(self) -> dict: ...",
    "none": "def verb(self): ...",
    "string_iterator": "def verb(self) -> 'Iterator[dict]': ...",
    "string_context_manager": "def verb(self) -> 'AbstractContextManager[dict]': ...",
    "string_unresolved": "def verb(self) -> 'Zed[dict]': ...",
}


def protocol_for(world):
    kind = world.get("returns", "none")
    ns = dict(PROTO_GLOBALS)
    exec(ANNOTATIONS[kind], ns)
    verb = ns["verb"]
    if not world.get("protocol_has", True):
        return type("Protocol", (), {})
    return type("Protocol", (), {world.get("name", ""): verb})


def refusal(world):
    world = world if isinstance(world, dict) else {}
    proto = protocol_for(world) if world.get("has_protocol", True) else None
    try:
        return {"reason": live._refusal(proto, world.get("name", ""))}
    except NameError as exc:
        return {"status": "error", "error": "NameError", "message": str(exc)}


def repository_attr(world):
    world = world if isinstance(world, dict) else {}
    repo_type = type(world.get("repo_type", "Repo"), (), {})
    repo = repo_type()
    name = world.get("name", "")
    if world.get("has_attr", True):
        if world.get("callable", True):
            setattr(repo, name, lambda *a, **k: None)
        else:
            setattr(repo, name, 5)   # present but not callable
    proto = protocol_for(world) if world.get("has_protocol", True) else None
    view = live.AwaitableRepository(repo, None, proto)
    try:
        getattr(view, name)
    except AttributeError as exc:
        return {"status": "error", "error": "AttributeError", "message": str(exc)}
    except NameError as exc:
        return {"status": "error", "error": "NameError", "message": str(exc)}
    return {"status": "awaitable"}


def store_attr(world):
    world = world if isinstance(world, dict) else {}
    store_type = type(world.get("store_type", "Store"), (), {})
    store = store_type()
    store.executor = None
    name = world.get("name", "")
    if world.get("has_attr", True):
        setattr(store, name, object())
    saved = live._repository_protocols
    live._repository_protocols = lambda: {}
    view = live.AwaitableStore(store)
    try:
        getattr(view, name)
    except AttributeError as exc:
        return {"status": "error", "error": "AttributeError", "message": str(exc)}
    finally:
        live._repository_protocols = saved
    return {"status": "repository"}


def complete(world):
    world = world if isinstance(world, dict) else {}
    kind = world.get("kind", "value")
    if kind == "generator":
        items = list(world.get("items") or [])

        def gen():
            for item in items:
                yield item

        return {"kind": "tuple", "items": list(live._complete(gen, (), {}))}
    if kind == "list":
        value = list(world.get("value") or [])
        return {"kind": "value", "value": live._complete(lambda: value, (), {})}
    return {"kind": "value", "value": live._complete(lambda: world.get("value"), (), {})}


def expected(r):
    f = r.get("func")
    if f == "no_awaitable_form":
        return sorted(live._NO_AWAITABLE_FORM.values())
    if f == "refusal":
        return refusal(r.get("world"))
    if f == "repository_attr":
        return repository_attr(r.get("world"))
    if f == "store_attr":
        return store_attr(r.get("world"))
    if f == "complete":
        return complete(r.get("world"))
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
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
