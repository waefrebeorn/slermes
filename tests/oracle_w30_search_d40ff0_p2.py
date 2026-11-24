#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/search.py
(gate w30_search_d40ff0_p2 — statement composition, routing, ranking).

The live search module is imported BY FILE PATH.  sqlite is the seam here:
each route builder runs against a recording holder whose ``_read_all`` /
``_read_ctx`` answer from the fixture, so what gets compared is the exact SQL
text and bind order the live code hands to the database.

Ported symbols and their live entry points:

  filter_clauses    _search_filter_clauses(where, params, **filters)
  select_sql        _search_select_sql(...)
  fts_match_sql     _fts_match_sql(table, match, order_by, **kw)
  like_boolean      _compile_like_boolean_query(query)
  like_fallback     _search_messages_like_fallback(holder, ...)
  gap               _search_unindexed_gap(holder, ...)
  cjk_like          _search_cjk(holder, query, False, route)   (LIKE branch)
  session_rank      search_sessions_by_id (list_sessions_rich scripted)
  order_by          _FTS_ORDER_BY.get(...)
  route             _search_messages_impl's route selection
  describe_path     _describe_search_path(holder, query)
  sql_constants     module SQL fragments / limits
"""
import importlib.util
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_search_d40ff0_p2"

_TMPHOME = tempfile.mkdtemp(prefix="w30-search-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME
os.environ["SLERMES_HOME"] = _TMPHOME

sys.path.append(str(REPO / "tests"))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
sys.path.insert(0, str(QUARRY))

_SPEC = importlib.util.spec_from_file_location(
    "w30_search_live_p2", QUARRY / "persistence" / "sqlite" / "conversation" / "_store" / "search.py")
live = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(live)

from persistence.sqlite._schema_ddl import (  # noqa: E402
    FTS_STALE_KEY, FTS_TOOL_CONTENT_PREFIX_CHARS, FTS_TRIGRAM_EXCLUDED_SOURCES, fts_trigram_session_sql,
)

WANTED = {
    "filter_clauses", "select_sql", "fts_match_sql", "like_boolean", "like_fallback", "gap",
    "cjk_like", "session_rank", "order_by", "route", "describe_path", "sql_constants",
}


class _Sent(Exception):
    def __init__(self, tag):
        super().__init__(tag)
        self.tag = tag


class _LikeHolder:
    """A holder whose reads are scripted: state_meta answers from the fixture,
    the search statement itself is recorded instead of executed."""

    read_only = False
    _fts_stale = False
    _fts_enabled = True
    _trigram_available = False

    def __init__(self, meta=None):
        self._meta_rows = list(meta or [])
        self.calls = []

    def _read_all(self, sql, params=()):
        if "state_meta" in sql:
            return list(self._meta_rows)
        self.calls.append({"sql": sql, "params": list(params)})
        return []


class _RouteHolder:
    """Holder that aborts at the first read so the route is visible."""

    read_only = False

    def __init__(self, stale, enabled, trigram):
        self._fts_stale = bool(stale)
        self._fts_enabled = bool(enabled)
        self._trigram_available = bool(trigram)

    def _read_one(self, sql, params=()):
        return None

    def _read_all(self, sql, params=()):
        if "state_meta" in sql:
            return []
        raise _Sent("like")


class _PathProbe:
    def __init__(self, stale, trigram):
        self._fts_stale = bool(stale)
        self._trigram_available = bool(trigram)


def filt(r):
    return dict(
        include_inactive=bool(r.get("include_inactive", False)),
        source_filter=r.get("source_filter"),
        exclude_sources=r.get("exclude_sources"),
        role_filter=r.get("role_filter"),
        after_ts=r.get("after_ts"),
        before_ts=r.get("before_ts"),
    )


def filter_clauses(r):
    where = list(r.get("where") or [])
    params = list(r.get("params") or [])
    live._search_filter_clauses(where, params, **filt(r))
    return {"where": where, "params": params}


def select_sql(r):
    return live._search_select_sql(
        r.get("snippet_sql", ""), r.get("from_sql", ""), list(r.get("where") or []),
        r.get("order_by", ""), r.get("limit_sql", ""))


def fts_match_sql(r):
    sql, params = live._fts_match_sql(
        r.get("table", "messages_fts"), r.get("match_query", ""), r.get("order_by_sql", live._FTS_RANK_ORDER_BY),
        limit=r.get("limit", 20), offset=r.get("offset", 0), **filt(r))
    return {"sql": sql, "params": params}


def like_boolean(r):
    predicate, params, snippet_term = live._compile_like_boolean_query(r.get("query", ""))
    return {"predicate": predicate, "params": params, "snippet_term": snippet_term}


def like_fallback(r):
    holder = _LikeHolder()
    live._search_messages_like_fallback(
        holder, r.get("query", ""), limit=r.get("limit", 20), offset=r.get("offset", 0),
        sort=r.get("sort"), **filt(r))
    return holder.calls[-1] if holder.calls else None


def gap(r):
    meta = r.get("meta")
    rows = []
    if isinstance(meta, dict):
        if "high_water" in meta:
            rows.append({"key": "fts_rebuild_high_water", "value": meta["high_water"]})
        if "progress" in meta:
            rows.append({"key": "fts_rebuild_progress", "value": meta["progress"]})
    holder = _LikeHolder(rows)
    live._search_unindexed_gap(holder, r.get("fts_query", ""), r.get("limit", 20), **filt(r))
    return holder.calls[-1] if holder.calls else None


def cjk_like(r):
    holder = _LikeHolder()
    route = dict(order_by_sql=live._FTS_RANK_ORDER_BY, limit=r.get("limit", 20),
                 offset=r.get("offset", 0), **filt(r))
    live._search_cjk(holder, r.get("query", ""), False, route)
    return holder.calls[-1] if holder.calls else None


_ORIG_LSR = live.list_sessions_rich


def session_rank(r):
    rows = [dict(x) for x in (r.get("rows") or [])]
    captured = {}

    def fake(holder=None, **kw):
        captured.update(kw)
        return [dict(x) for x in rows]

    live.list_sessions_rich = fake
    try:
        ranked = live.search_sessions_by_id(None, r.get("query"), limit=r.get("limit", 20))
    finally:
        live.list_sessions_rich = _ORIG_LSR
    return {"needle": captured.get("id_query"), "fetch_limit": captured.get("limit"),
            "ranked": ranked}


def order_by(r):
    sort = r.get("sort")
    return live._FTS_ORDER_BY.get(sort.strip().lower() if isinstance(sort, str) else None,
                                  live._FTS_RANK_ORDER_BY)


_ORIG = {}


def route(r):
    q = r.get("query") or ""
    holder = _RouteHolder(r.get("fts_stale"), r.get("fts_enabled", True), r.get("trigram_available"))
    saved = (live._in_one_read, live._match_rows, live._search_messages_stale,
             live._search_messages_like_fallback)
    live._in_one_read = lambda *a, **k: (_ for _ in ()).throw(_Sent("fts"))
    live._match_rows = lambda *a, **k: (_ for _ in ()).throw(_Sent("trigram"))
    live._search_messages_stale = lambda *a, **k: (_ for _ in ()).throw(_Sent("stale"))
    live._search_messages_like_fallback = lambda *a, **k: (_ for _ in ()).throw(_Sent("like"))
    tag = None
    try:
        live._search_messages_impl(
            holder, q, role_filter=r.get("role_filter"), source_filter=r.get("source_filter"),
            limit=20, offset=0, sort=None, include_inactive=False, fields=None)
    except _Sent as exc:
        tag = exc.tag
    finally:
        (live._in_one_read, live._match_rows, live._search_messages_stale,
         live._search_messages_like_fallback) = saved
    if tag == "like":
        rf = r.get("role_filter")
        return "tool_like" if isinstance(rf, list) and "tool" in rf else "like_scan"
    if tag:
        return tag
    if not q.strip() or not live._sanitize_fts5_query(q):
        return "empty"
    if not r.get("fts_enabled", True):
        return "disabled"
    return "unknown"


def describe_path(r):
    return live._describe_search_path(_PathProbe(r.get("fts_stale"), r.get("trigram_available")),
                                      r.get("query", ""))


def sql_constants():
    return {
        "search_select_tail": live._SEARCH_SELECT_TAIL,
        "like_snippet_sql": live._LIKE_SNIPPET_SQL,
        "like_any_column_sql": live._LIKE_ANY_COLUMN_SQL,
        "like_coalesced_column_sql": live._LIKE_COALESCED_COLUMN_SQL,
        "fts_rank_order_by": live._FTS_RANK_ORDER_BY,
        "fts_order_by": dict(live._FTS_ORDER_BY),
        "context_window_sql": live._CONTEXT_WINDOW_SQL,
        "base_boundary_sweep_sql": live._BASE_BOUNDARY_SWEEP_SQL,
        "trigram_boundary_sweep_sql": live._TRIGRAM_BOUNDARY_SWEEP_SQL,
        "chunk_insert_sql": live._CHUNK_INSERT_SQL,
        "trigram_chunk_insert_sql": live._TRIGRAM_CHUNK_INSERT_SQL,
        "fts_tool_content_prefix_chars": FTS_TOOL_CONTENT_PREFIX_CHARS,
        "fts_trigram_excluded_sources": list(FTS_TRIGRAM_EXCLUDED_SOURCES),
        "fts_stale_key": FTS_STALE_KEY,
        "trigram_session_sql_s": fts_trigram_session_sql("s"),
        "trigram_session_sql_bare": fts_trigram_session_sql(),
        "search_message_result_fields": list(live._SEARCH_MESSAGE_RESULT_FIELDS),
    }


HANDLERS = {
    "filter_clauses": filter_clauses, "select_sql": select_sql, "fts_match_sql": fts_match_sql,
    "like_boolean": like_boolean, "like_fallback": like_fallback, "gap": gap,
    "cjk_like": cjk_like, "session_rank": session_rank, "order_by": order_by,
    "route": route, "describe_path": describe_path,
}


def expected(r):
    f = r.get("func")
    if f == "sql_constants":
        return sql_constants()
    return HANDLERS[f](r)


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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
