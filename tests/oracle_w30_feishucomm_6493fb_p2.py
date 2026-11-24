#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/feishu/feishu_comment.py
(gate w30_feishucomm_6493fb_p2: rich text, links, timeline, prompts).

The live module is imported by file path.  Three seams are pinned so the
comparison is value->value: ``list_whole_comments`` and ``list_comment_replies``
replay already-fetched replies, and ``_referenced_docs_text`` is the same
extract+format chain without the wiki-node GET (a wiki link keeps its own
type/token).  Everything else -- element-to-text, link extraction/dedupe, the
timeline window, the prompt text -- runs as upstream wrote it.
"""
import asyncio
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_feishucomm_6493fb_p2"
LIVE_PATH = QUARRY / "plugins/platforms/feishu/feishu_comment.py"

WANTED = {"sanitize", "chunk", "extract_text", "reply_user", "reply_list", "extract_links",
          "format_docs", "truncate", "select", "timeline", "build_local", "build_whole",
          "whole_prompt", "local_prompt"}


def load_live():
    spec = importlib.util.spec_from_file_location("fc_live_" + GATE, str(LIVE_PATH))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


live = load_live()

CANNED = {"whole": [], "replies": []}


async def _fake_list_whole(client, file_token, file_type):
    return CANNED["whole"]


async def _fake_list_replies(client, file_token, file_type, comment_id, *, expect_reply_id=""):
    return CANNED["replies"]


async def _fake_referenced_docs_text(client, replies, file_token):
    return live._format_referenced_docs(live._extract_docs_links(replies), file_token)


live.list_whole_comments = _fake_list_whole
live.list_comment_replies = _fake_list_replies
live._referenced_docs_text = _fake_referenced_docs_text


def T(e):
    return (e.get("user_id", ""), e.get("text", ""), bool(e.get("is_self")))


def toj(tl):
    return [{"user_id": u, "text": t, "is_self": bool(s)} for u, t, s in tl]


def sanitize(r):
    return live._sanitize_comment_text(r.get("text", ""))


def chunk(r):
    return live._chunk_text(r.get("text", ""), r.get("limit", 4000))


def extract_text(r):
    reply = r.get("reply")
    reply = reply if isinstance(reply, dict) else {}
    return live._extract_reply_text(reply, semantic=bool(r.get("semantic")),
                                    self_open_id=r.get("self_open_id", ""))


def reply_user(r):
    reply = r.get("reply")
    return live._get_reply_user_id(reply if isinstance(reply, dict) else {})


def reply_list(r):
    whole = r.get("whole")
    return live._reply_list_replies(whole if isinstance(whole, dict) else {})


def extract_links(r):
    return live._extract_docs_links(r.get("replies") or [])


def format_docs(r):
    return live._format_referenced_docs(r.get("links") or [], r.get("current", ""))


def truncate(r):
    if "limit" in r:
        return live._truncate(r.get("text", ""), r["limit"])
    return live._truncate(r.get("text", ""))


def select(r):
    tl = [T(e) for e in (r.get("timeline") or [])]
    pinned = tuple(r.get("pinned") or ())
    return toj(live._select_timeline(tl, r.get("limit", 0), r.get("center", -1), pinned=pinned))


def timeline(r):
    return toj([live._timeline_entry(x, r.get("self_open_id", "")) for x in (r.get("replies") or [])])


def build_local(r):
    kw = dict(r)
    for k in ("func", "case", "out"):
        kw.pop(k, None)
    kw["timeline"] = [T(e) for e in (r.get("timeline") or [])]
    return live.build_local_comment_prompt(**kw)


def build_whole(r):
    kw = dict(r)
    for k in ("func", "case", "out"):
        kw.pop(k, None)
    kw["timeline"] = [T(e) for e in (r.get("timeline") or [])]
    return live.build_whole_comment_prompt(**kw)


def whole_prompt(r):
    doc = r.get("doc") or {}
    CANNED["whole"] = r.get("whole_comments") or []
    return asyncio.run(live._whole_comment_prompt(None, r.get("from_open_id", ""), doc))


def local_prompt(r):
    doc = r.get("doc") or {}
    CANNED["replies"] = r.get("replies") or []
    return asyncio.run(live._local_comment_prompt(
        None, r.get("comment_id", ""), r.get("reply_id", ""), r.get("from_open_id", ""),
        r.get("quote_text", ""), doc))


def expected(r):
    f = r.get("func")
    if f == "sanitize":
        return sanitize(r)
    if f == "chunk":
        return chunk(r)
    if f == "extract_text":
        return extract_text(r)
    if f == "reply_user":
        return reply_user(r)
    if f == "reply_list":
        return reply_list(r)
    if f == "extract_links":
        return extract_links(r)
    if f == "format_docs":
        return format_docs(r)
    if f == "truncate":
        return truncate(r)
    if f == "select":
        return select(r)
    if f == "timeline":
        return timeline(r)
    if f == "build_local":
        return build_local(r)
    if f == "build_whole":
        return build_whole(r)
    if f == "whole_prompt":
        return whole_prompt(r)
    if f == "local_prompt":
        return local_prompt(r)
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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
