#!/usr/bin/env python3
"""Differential oracle for plugins/web/perplexity/provider.py (gate w26_provider_9eacd0).

The plugin module is imported by package path and its pure value->value helpers
are called directly: ``_normalize_search_results``, ``_normalize_snippets`` and
``_query_for_urls``.  What is compared is exactly what a 200 body decides -- the
position numbering, the per-URL error documents a partial success produces and
the path-word relevance query.  ``httpx.post``, the env key lookup and the
managed gateway never run: the response body is a fixed input from the fixture.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_provider_9eacd0"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.web.perplexity import provider as live  # noqa: E402

WANTED = {"normalize_search_results", "normalize_snippets", "query_for_urls", "constants"}


def normalize_search_results(r):
    return live._normalize_search_results(r.get("response"))


def normalize_snippets(r):
    urls = r.get("urls")
    return live._normalize_snippets(r.get("response"), list(urls) if isinstance(urls, list) else urls)


def query_for_urls(r):
    urls = r.get("urls")
    return live._query_for_urls(list(urls) if isinstance(urls, list) else urls)


def constants(_r):
    return {
        "default_base_url": live._DEFAULT_BASE_URL,
        "key_url": live._KEY_URL,
        "max_search_results": live._MAX_SEARCH_RESULTS,
        "max_tokens": live._MAX_TOKENS,
        "max_tokens_per_page": live._MAX_TOKENS_PER_PAGE,
        "missing_key_error": live._missing_key_error(),
    }


def expected(r):
    f = r.get("func")
    if f == "normalize_search_results":
        return normalize_search_results(r)
    if f == "normalize_snippets":
        return normalize_snippets(r)
    if f == "query_for_urls":
        return query_for_urls(r)
    if f == "constants":
        return constants(r)
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
