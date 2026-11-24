#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/feishu/feishu_comment.py
(gate w30_feishucomm_6493fb_p1: event parse + request/payload shaping).

The live module is imported by file path (it has no package-level imports).
The only seam is ``_exec_request`` (the lark_oapi transport): it is replaced by
a recorder that replays canned (code, msg, data) triples, so what is compared
is exactly the request the call builds and the decision it makes from the
answer -- no tenant token, no network, no SDK.

The retry sleep is pinned to 0 so the 6-attempt loop runs instantly.
"""
import asyncio
import importlib.util
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_feishucomm_6493fb_p1"
LIVE_PATH = QUARRY / "plugins/platforms/feishu/feishu_comment.py"

WANTED = {"parse_event", "meta", "comment", "pages", "reaction", "reply",
          "whole_comment", "deliver"}


def load_live():
    spec = importlib.util.spec_from_file_location("fc_live_" + GATE, str(LIVE_PATH))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


live = load_live()

# The add path imports lark_oapi.AccessTokenType only to build a request; a
# stub module keeps that import resolvable without the SDK.
sys.modules.setdefault("lark_oapi", SimpleNamespace(AccessTokenType=SimpleNamespace(TENANT="tenant")))
live._COMMENT_RETRY_DELAY_S = 0.0

ORIG_REPLY = live.reply_to_comment
ORIG_WHOLE = live.add_whole_comment


def rec(v):
    """Tuples -> lists, so a recorded request compares as JSON."""
    if isinstance(v, dict):
        return {k: rec(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [rec(x) for x in v]
    return v


def fake_transport(results):
    """Install a recorder/replayer as ``_exec_request``; return its call log."""
    calls = []
    queue = list(results or [])

    async def fake(client, method, uri, paths=None, queries=None, body=None):
        calls.append({"method": method, "uri": uri, "paths": rec(paths),
                      "queries": rec(queries), "body": rec(body)})
        r = queue.pop(0) if queue else {"code": 0, "data": {}}
        return r.get("code", 0), r.get("msg", ""), r.get("data", {})

    live._exec_request = fake
    return calls


def req_of(calls):
    c = calls[0] if calls else {"method": None, "uri": None, "paths": None,
                                "queries": None, "body": None}
    return {"method": c["method"], "uri": c["uri"], "paths": c["paths"],
            "queries": c["queries"], "body": c["body"]}


def parse_event(r):
    payload = r.get("payload")
    payload = payload if isinstance(payload, dict) else {}
    return live.parse_drive_comment_event(SimpleNamespace(**payload))


def meta(r):
    calls = fake_transport(r.get("results"))
    out = asyncio.run(live.query_document_meta(None, r.get("file_token", ""), r.get("file_type", "")))
    got = req_of(calls)
    got["result"] = out
    return got


def comment(r):
    calls = fake_transport(r.get("results"))
    out = asyncio.run(live.batch_query_comment(None, r.get("file_token", ""),
                                               r.get("file_type", ""), r.get("comment_id", "")))
    got = req_of(calls)
    got["result"] = out
    got["attempts"] = len(calls)
    return got


def pages(r):
    calls = fake_transport(r.get("pages"))
    items, ok = asyncio.run(live._list_all_pages(None, "/open-apis/x", {"file_token": "ft"},
                                                 [("file_type", "docx")], fail_msg="pages failed: %s %s"))
    return {"items": items, "fetch_ok": ok, "calls": len(calls)}


def reaction(r):
    calls = fake_transport([{"code": r.get("code", 0)}])
    ok = asyncio.run(live.update_comment_reaction(
        None, r.get("action", "add"), file_token=r.get("file_token", ""),
        file_type=r.get("file_type", ""), reply_id=r.get("reply_id", ""),
        reaction_type=r.get("reaction_type", "OK")))
    got = req_of(calls)
    got["ok"] = ok
    return got


def reply(r):
    calls = fake_transport([{"code": r.get("code", 0)}])
    ok, code = asyncio.run(live.reply_to_comment(None, r.get("file_token", ""),
                                                 r.get("file_type", ""), r.get("comment_id", ""),
                                                 r.get("text", "")))
    got = req_of(calls)
    got["ok"] = ok
    got["code"] = code
    return got


def whole_comment(r):
    calls = fake_transport([{"code": r.get("code", 0)}])
    ok = asyncio.run(live.add_whole_comment(None, r.get("file_token", ""),
                                            r.get("file_type", ""), r.get("text", "")))
    got = req_of(calls)
    got["ok"] = ok
    return got


def deliver(r):
    steps = []
    codes = [c.get("code", 0) if isinstance(c, dict) else c
             for c in (r.get("reply_codes") or [])]
    woks = list(r.get("whole_ok") or [])

    async def fake_reply(client, ft, ftyp, cid, text):
        code = codes.pop(0) if codes else 0
        steps.append({"api": "reply", "text": text, "ok": code == 0, "code": code})
        return code == 0, code

    async def fake_whole(client, ft, ftyp, text):
        ok = woks.pop(0) if woks else True
        steps.append({"api": "whole", "text": text, "ok": bool(ok)})
        return bool(ok)

    live.reply_to_comment = fake_reply
    live.add_whole_comment = fake_whole
    try:
        ok = asyncio.run(live.deliver_comment_reply(None, r.get("file_token", ""),
                                                    r.get("file_type", ""), r.get("comment_id", ""),
                                                    r.get("text", ""), bool(r.get("is_whole"))))
    finally:
        live.reply_to_comment = ORIG_REPLY
        live.add_whole_comment = ORIG_WHOLE
    return {"ok": ok, "steps": steps}


def expected(r):
    f = r.get("func")
    if f == "parse_event":
        return parse_event(r)
    if f == "meta":
        return meta(r)
    if f == "comment":
        return comment(r)
    if f == "pages":
        return pages(r)
    if f == "reaction":
        return reaction(r)
    if f == "reply":
        return reply(r)
    if f == "whole_comment":
        return whole_comment(r)
    if f == "deliver":
        return deliver(r)
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
