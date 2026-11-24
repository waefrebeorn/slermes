#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/search.py
(gate w30_search_d40ff0_p1 — query normalization).

The live search module is imported BY FILE PATH (its absolute imports pull the
store package, whose chain the seam stubs cover on this host).  Nothing under
test touches sqlite: every compared value is a pure function of the request.

Ported symbols and their live entry points (see
src/persistence/sqlite/conversation/_store/search.c):

  sanitize           _sanitize_fts5_query(query)
  quote_tokens       _quote_fts_tokens(raw)
  non_op_tokens      _non_operator_tokens(raw)
  strip_wildcards    _strip_cjk_wildcards(raw)
  or_relaxed         _or_relaxed_query(query)
  trigram_eligible   _trigram_eligible_tokens(query)
  short_cjk          _has_short_cjk_token(raw)
  trigram_route_ok   _trigram_route_ok(holder, raw)        (flag = seam)
  is_cjk             _is_cjk(codepoint)
  contains_cjk       _contains_cjk(text)
  count_cjk          _count_cjk(text)
  escape_like        sql_common.escape_like(term)
  like_params        _like_params(term)
  flatten_text       _flatten_text(decoded)
  fields             _search_message_fields(fields)        (raises -> error)
  backfill_info      _BackfillProgress(...).info()
  constants          module constants
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
GATE = "w30_search_d40ff0_p1"

_TMPHOME = tempfile.mkdtemp(prefix="w30-search-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME
os.environ["SLERMES_HOME"] = _TMPHOME

sys.path.append(str(REPO / "tests"))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
sys.path.insert(0, str(QUARRY))

_SPEC = importlib.util.spec_from_file_location(
    "w30_search_live_p1", QUARRY / "persistence" / "sqlite" / "conversation" / "_store" / "search.py")
live = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(live)

from persistence.sqlite.conversation._store import sql_common as sqlc  # noqa: E402

WANTED = {
    "sanitize", "quote_tokens", "non_op_tokens", "strip_wildcards", "or_relaxed",
    "trigram_eligible", "short_cjk", "trigram_route_ok", "is_cjk", "contains_cjk",
    "count_cjk", "escape_like", "like_params", "flatten_text", "fields",
    "backfill_info", "constants",
}


class _Probe:
    """Bare holder stand-in: only the index-availability flag is consulted."""

    def __init__(self, available):
        self._trigram_available = bool(available)


def sanitize(r):
    return live._sanitize_fts5_query(r.get("query", ""))


def quote_tokens(r):
    return live._quote_fts_tokens(r.get("raw", ""))


def non_op_tokens(r):
    return live._non_operator_tokens(r.get("raw", ""))


def strip_wildcards(r):
    return live._strip_cjk_wildcards(r.get("raw", ""))


def or_relaxed(r):
    return live._or_relaxed_query(r.get("query", ""))


def trigram_eligible(r):
    return live._trigram_eligible_tokens(r.get("query", ""))


def short_cjk(r):
    return live._has_short_cjk_token(r.get("raw", ""))


def trigram_route_ok(r):
    return live._trigram_route_ok(_Probe(r.get("trigram_available")), r.get("raw", ""))


def is_cjk(r):
    return live._is_cjk(int(r.get("codepoint", 0)))


def contains_cjk(r):
    return live._contains_cjk(r.get("text", ""))


def count_cjk(r):
    return live._count_cjk(r.get("text", ""))


def escape_like(r):
    return sqlc.escape_like(r.get("term", ""))


def like_params(r):
    return live._like_params(r.get("term", ""))


def flatten_text(r):
    return live._flatten_text(r.get("decoded"))


def fields(r):
    try:
        out = live._search_message_fields(r.get("fields"))
    except Exception as exc:  # noqa: BLE001 -- the error IS the compared value
        return {"ok": False, "error": type(exc).__name__ + ": " + str(exc)}
    return {"ok": True, "fields": None if out is None else list(out)}


def backfill_info(r):
    progress = live._BackfillProgress(
        phase=r.get("phase", ""), total=int(r.get("total", 0)), indexed=int(r.get("indexed", 0)))
    return progress.info()


def constants():
    return {
        "max_fts5_query_chars": sqlc.MAX_FTS5_QUERY_CHARS,
        "fts5_special_chars": live._FTS5_SPECIAL_CHARS,
        "cjk_ranges": [list(pair) for pair in live._CJK_RANGES],
        "fts_operators": sorted(live._FTS_OPERATORS),
        "like_skip_tokens": sorted(live._LIKE_SKIP_TOKENS),
        "result_fields": list(live._SEARCH_MESSAGE_RESULT_FIELDS),
    }


def expected(r):
    f = r.get("func")
    if f == "constants":
        return constants()
    return {
        "sanitize": sanitize, "quote_tokens": quote_tokens, "non_op_tokens": non_op_tokens,
        "strip_wildcards": strip_wildcards, "or_relaxed": or_relaxed,
        "trigram_eligible": trigram_eligible, "short_cjk": short_cjk,
        "trigram_route_ok": trigram_route_ok, "is_cjk": is_cjk,
        "contains_cjk": contains_cjk, "count_cjk": count_cjk,
        "escape_like": escape_like, "like_params": like_params,
        "flatten_text": flatten_text, "fields": fields,
        "backfill_info": backfill_info,
    }[f](r)


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
