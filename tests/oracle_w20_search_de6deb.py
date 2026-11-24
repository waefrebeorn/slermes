#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/search.py
(w20_search_de6deb).

The real SearchHit/SearchContext/SearchSort dataclasses are loaded from the
quarry, then the module under test is loaded as persistence.sqlite.conversation.
search with stubbed backend seams: _backend, _schema, _store.search (which
captures the keyword set it is called with) and _errors.refusals. Everything
compared -- the sort table, _optional_list, _hit's projection and the kwargs
SQLiteSessionSearch.search_messages forwards -- is the live code path.
"""
import contextlib
import importlib.util
import json
import os
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _load(name, path):
    sp = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(sp)
    sys.modules[name] = m
    sp.loader.exec_module(m)
    return m


for pkg in ("persistence", "persistence.core", "persistence.conversation",
            "persistence.sqlite", "persistence.sqlite.conversation",
            "persistence.sqlite.conversation._store"):
    _mk(pkg)

identity = _load("persistence.core.identity", os.path.join(Q, "persistence", "core", "identity.py"))
sys.modules["persistence"].MessageId = identity.MessageId
sys.modules["persistence"].SessionId = identity.SessionId

contract = _load("persistence.conversation.search",
                 os.path.join(Q, "persistence", "conversation", "search.py"))
SearchSort = contract.SearchSort
SearchHit = contract.SearchHit

CAPTURED = {}
CUR_MATCHES = [[]]


def _fake_search(holder, query, **kw):
    CAPTURED.clear()
    CAPTURED["query"] = query
    CAPTURED.update(kw)
    return CUR_MATCHES[0]


_mk("persistence.sqlite._backend", SQLiteStore=object,
    register_repository=lambda *a, **k: None)
_mk("persistence.sqlite._schema", retry_deferred_fts_recovery=lambda holder: True)
_mk("persistence.sqlite.conversation._errors",
    refusals=lambda holder: contextlib.nullcontext())
_mk("persistence.sqlite.conversation._store.search",
    search_messages=_fake_search,
    fts_rebuild_status=lambda holder: None,
    rebuild_fts=lambda holder: 0)

NAME = "persistence.sqlite.conversation.search"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "persistence", "sqlite", "conversation", "search.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class _Store:
    def holder(self):
        return SimpleNamespace()


STORE = _Store()


def plain_hit(h):
    return {
        "message_id": h.message_id,
        "session_id": h.session_id,
        "role": h.role,
        "snippet": h.snippet,
        "timestamp": h.timestamp,
        "tool_name": h.tool_name,
        "source": h.source,
        "model": h.model,
        "session_started": h.session_started,
        "context": [{"role": c.role, "content": c.content} for c in h.context],
    }


def _sort_expected(name):
    try:
        return {"value": live._SORT[getattr(SearchSort, name.upper())]}
    except KeyError:
        return {"error": "KeyError"}
    except AttributeError:
        return {"error": "KeyError"}


def _hit_expected(match):
    try:
        return plain_hit(live._hit(match))
    except Exception as exc:  # noqa: BLE001 - report the live exception shape
        return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}


def _call_expected(params):
    params = params if isinstance(params, dict) else {}
    CUR_MATCHES[0] = params.get("matches") or []
    sort_name = params.get("sort", "relevance")
    svc = live.SQLiteSessionSearch(STORE)
    try:
        hits = svc.search_messages(
            params.get("query", ""),
            source_filter=params.get("source_filter"),
            exclude_sources=params.get("exclude_sources"),
            role_filter=params.get("role_filter"),
            limit=params.get("limit", 20),
            offset=params.get("offset", 0),
            sort=getattr(SearchSort, sort_name.upper()),
            include_inactive=params.get("include_inactive", False),
            fields=params.get("fields"),
            after_ts=params.get("after_ts"),
            before_ts=params.get("before_ts"),
        )
    except Exception as exc:  # noqa: BLE001
        return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}
    return {"status": "ok", "value": {"kwargs": dict(CAPTURED),
                                      "hits": [plain_hit(h) for h in hits]}}


def expected(r):
    f = r.get("func")
    if f == "sort_arg":
        return _sort_expected(r.get("sort") or "")
    if f == "optional_list":
        values = r.get("values")
        if values is None:
            return None
        try:
            return list(values)
        except TypeError as exc:
            return {"status": "raised", "type": "TypeError", "message": str(exc)}
    if f == "hit":
        return _hit_expected(r.get("match"))
    if f == "store_call":
        return _call_expected(r.get("params"))
    raise AssertionError("unknown func %r" % (f,))


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
print("ORACLE w20_search_de6deb: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
