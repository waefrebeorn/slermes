#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 7 oracle.

Writes tests/fixtures/telegram_adapter_flow7.jsonl.  Each case is
{"func", "in": {"args", "state"?, "io"?, "errors"?}}.  Re-running reproduces
byte-identical fixtures.  Every ported function gets >= 6 cases and every seam
name appears in at least one case's io.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow7.jsonl"


def case(func, args=None, state=None, io=None, errors=None, **kw):
    c = {"func": func, "in": {"args": args or {}}}
    if state is not None:
        c["in"]["state"] = state
    if io is not None:
        c["in"]["io"] = io
    if errors is not None:
        c["in"]["errors"] = errors
    c["in"].update(kw)
    return c


def sc(**kw):
    d = {"__script__": True}
    d.update(kw)
    return d


def raise_sc(kind, msg):
    return sc(**{"raise": {"type": kind, "message": msg}})


# Shared exception-graph registry.  cause/context reference other ids.
ERR = {
    "auth_inv": {"class": "InvalidToken", "message": "token invalid"},
    "auth_for": {"class": "Forbidden", "message": "forbidden"},
    "forbidden_lo": {"class": "forbidden", "message": "x"},
    "badreq": {"class": "BadRequest", "message": "bad request"},
    "mybadreq": {"class": "MyBadRequest", "message": "x"},
    "notbad": {"class": "NotBadRequest", "message": "x"},
    "neterr": {"class": "NetworkError", "message": "net"},
    "timedout": {"class": "TimedOut", "message": "timed out"},
    "connerr": {"class": "ConnectionError", "message": "conn"},
    "oserr": {"class": "OSError", "message": "boom"},
    "val": {"class": "ValueError", "message": "nope"},
    "epnf": {"class": "EndpointNotFound", "message": "x"},
    "attrerr": {"class": "AttributeError", "message": "x"},
    "typeerr": {"class": "TypeError", "message": "x"},
    "notimpl": {"class": "NotImplementedError", "message": "x"},
    "err404": {"class": "RuntimeError", "message": "x", "error_code": 404},
    "err200": {"class": "RuntimeError", "message": "x", "error_code": 200},
    "methodnf": {"class": "RuntimeError", "message": "method not found"},
    "nosuch": {"class": "RuntimeError", "message": "no such method"},
    "unsup": {"class": "RuntimeError", "message": "unsupported feature"},
    "notimplmsg": {"class": "RuntimeError", "message": "not implemented here"},
    "replymiss": {"class": "BadRequest", "message": "Reply message not found"},
    "replymiss2": {"class": "BadRequest", "message": "Message to be replied not found"},
    "replyok": {"class": "BadRequest", "message": "something else"},
    "threadnf": {"class": "BadRequest", "message": "Thread not found"},
    "pool_cls": {"class": "PoolTimeout", "message": "x"},
    "pool_txt": {"class": "TimedOut", "message": "pool timeout"},
    "pool_occ": {"class": "TimedOut", "message": "connection pool occupied"},
    "ct_root": {"class": "TimedOut", "message": "timed out", "cause": "ct_child"},
    "ct_child": {"class": "ConnectTimeout", "message": "connect timeout"},
    "ct_ctx": {"class": "TimedOut", "message": "timed out", "context": "ct_ctx_child"},
    "ct_ctx_child": {"class": "ConnectTimeout", "message": "connect timed out"},
    "deep1": {"class": "Wrapper", "message": "outer", "cause": "deep2", "context": "deep3"},
    "deep2": {"class": "Inner", "message": "inner badrequest"},
    "deep3": {"class": "Side", "message": "side pool timeout"},
    "cycle1": {"class": "A", "message": "a", "cause": "cycle2"},
    "cycle2": {"class": "B", "message": "b", "context": "cycle1"},
    "empty": {"class": "RuntimeError", "message": ""},
    "proxyerr": {"class": "RuntimeError", "message": "secret"},
}

CASES = []


def add(func, *cases):
    for c in cases:
        assert c["func"] == func
        CASES.append(c)


# ── _iter_exception_graph ──
add("_iter_exception_graph",
    case("_iter_exception_graph", {"error": "val"}, errors=ERR),
    case("_iter_exception_graph", {"error": "ct_root"}, errors=ERR),
    case("_iter_exception_graph", {"error": "ct_ctx"}, errors=ERR),
    case("_iter_exception_graph", {"error": "deep1"}, errors=ERR),
    case("_iter_exception_graph", {"error": "cycle1"}, errors=ERR),
    case("_iter_exception_graph", {"error": "pool_occ"}, errors=ERR),
)

# ── _exception_graph_matches ──
add("_exception_graph_matches",
    case("_exception_graph_matches", {"error": "auth_inv", "name_marker": "invalidtoken", "text_markers": []}, errors=ERR),
    case("_exception_graph_matches", {"error": "val", "name_marker": "nope", "text_markers": ["ope"]}, errors=ERR),
    case("_exception_graph_matches", {"error": "ct_root", "name_marker": "connecttimeout", "text_markers": []}, errors=ERR),
    case("_exception_graph_matches", {"error": "ct_ctx", "name_marker": "x", "text_markers": ["connect timed out"]}, errors=ERR),
    case("_exception_graph_matches", {"error": "deep1", "name_marker": "inner", "text_markers": []}, errors=ERR),
    case("_exception_graph_matches", {"error": "val", "name_marker": "z", "text_markers": ["missing"]}, errors=ERR),
)

# ── _looks_like_auth_error ──
add("_looks_like_auth_error",
    *[case("_looks_like_auth_error", {"error": eid}, errors=ERR)
      for eid in ("auth_inv", "auth_for", "forbidden_lo", "badreq", "neterr", "oserr")],
)

# ── _looks_like_network_error ──
add("_looks_like_network_error",
    *[case("_looks_like_network_error", {"error": eid}, errors=ERR)
      for eid in ("badreq", "auth_inv", "forbidden_lo", "neterr", "timedout", "connerr", "oserr", "val")],
)

# ── _looks_like_connect_timeout ──
add("_looks_like_connect_timeout",
    case("_looks_like_connect_timeout", {"error": "ct_root"}, errors=ERR,
         io={"_exception_graph_matches": True}),
    case("_looks_like_connect_timeout", {"error": "val"}, errors=ERR,
         io={"_exception_graph_matches": False}),
    case("_looks_like_connect_timeout", {"error": "val"}, errors=ERR,
         io={"_exception_graph_matches": None}),
    case("_looks_like_connect_timeout", {"error": "val"}, errors=ERR,
         io={"_exception_graph_matches": raise_sc("ValueError", "x")}),
    case("_looks_like_connect_timeout", {"error": "val"}, errors=ERR,
         io={"_exception_graph_matches": raise_sc("SystemExit", "halt")}, expect_fail_closed=True),
    case("_looks_like_connect_timeout", {"error": "val"}, errors=ERR,
         io={"_exception_graph_matches": [True, False, True, False]}),
)

# ── _looks_like_pool_timeout ──
add("_looks_like_pool_timeout",
    *[case("_looks_like_pool_timeout", {"error": eid}, errors=ERR)
      for eid in ("pool_cls", "pool_txt", "pool_occ", "val", "deep1", "ct_root")],
)

# ── _is_thread_not_found_error ──
add("_is_thread_not_found_error",
    *[case("_is_thread_not_found_error", {"error": eid}, errors=ERR)
      for eid in ("threadnf", "replymiss", "val", "oserr", "timedout", "empty")],
)

# ── _is_bad_request_error ──
add("_is_bad_request_error",
    *[case("_is_bad_request_error", {"error": eid}, errors=ERR)
      for eid in ("badreq", "mybadreq", "notbad", "replymiss", "auth_inv", "neterr")],
)

# ── _is_reply_anchor_missing_error ──
add("_is_reply_anchor_missing_error",
    *[case("_is_reply_anchor_missing_error", {"error": eid}, errors=ERR)
      for eid in ("replymiss", "replymiss2", "replyok", "threadnf", "val", "empty")],
)

# ── _is_timed_out ──
add("_is_timed_out",
    *[case("_is_timed_out", {"exc": eid}, errors=ERR)
      for eid in ("timedout", "ct_root", "pool_txt", "val", "neterr", "empty")],
)

# ── _is_rich_capability_error ──
add("_is_rich_capability_error",
    *[case("_is_rich_capability_error", {"exc": eid}, errors=ERR)
      for eid in ("epnf", "auth_inv", "attrerr", "typeerr", "notimpl", "err404", "err200",
                  "methodnf", "nosuch", "val")],
)

# ── _is_rich_fallback_error ──
RF = {"exc": "val"}
add("_is_rich_fallback_error",
    case("_is_rich_fallback_error", RF, errors=ERR,
         io={"_is_bad_request_error": True, "_is_rich_capability_error": False}),
    case("_is_rich_fallback_error", RF, errors=ERR,
         io={"_is_bad_request_error": False, "_is_rich_capability_error": True}),
    case("_is_rich_fallback_error", {"exc": "unsup"}, errors=ERR,
         io={"_is_bad_request_error": False, "_is_rich_capability_error": False}),
    case("_is_rich_fallback_error", {"exc": "notimplmsg"}, errors=ERR,
         io={"_is_bad_request_error": False, "_is_rich_capability_error": False}),
    case("_is_rich_fallback_error", RF, errors=ERR,
         io={"_is_bad_request_error": False, "_is_rich_capability_error": False}),
    case("_is_rich_fallback_error", RF, errors=ERR,
         io={"_is_bad_request_error": raise_sc("SystemExit", "halt")}, expect_fail_closed=True),
    case("_is_rich_fallback_error", RF, errors=ERR,
         io={"_is_bad_request_error": False, "_is_rich_capability_error": raise_sc("ValueError", "x")}),
)

# ── _should_retry_without_dm_topic_reply_anchor ──
SR = {"error": "replymiss", "metadata": {"dm": True}, "reply_to_message_id": 42}
add("_should_retry_without_dm_topic_reply_anchor",
    case("_should_retry_without_dm_topic_reply_anchor", SR, errors=ERR,
         io={"_dm_topic_fallback": False}),
    case("_should_retry_without_dm_topic_reply_anchor", SR, errors=ERR,
         io={"_dm_topic_fallback": True, "_is_bad_request_error": False}),
    case("_should_retry_without_dm_topic_reply_anchor",
         dict(SR, reply_to_message_id=None), errors=ERR,
         io={"_dm_topic_fallback": True, "_is_bad_request_error": True}),
    case("_should_retry_without_dm_topic_reply_anchor", SR, errors=ERR,
         io={"_dm_topic_fallback": True, "_is_bad_request_error": True,
             "_is_reply_anchor_missing_error": True}),
    case("_should_retry_without_dm_topic_reply_anchor", SR, errors=ERR,
         io={"_dm_topic_fallback": True, "_is_bad_request_error": True,
             "_is_reply_anchor_missing_error": False}),
    case("_should_retry_without_dm_topic_reply_anchor", SR, errors=ERR,
         io={"_dm_topic_fallback": raise_sc("SystemExit", "halt")}, expect_fail_closed=True),
)

# ── _is_own_message ──
add("_is_own_message",
    case("_is_own_message", {"message": {"from_user": {"id": 7}}}, state={"_bot": {"id": 7}}),
    case("_is_own_message", {"message": {"from_user": {"id": 8}}}, state={"_bot": {"id": 7}}),
    case("_is_own_message", {"message": {}}, state={"_bot": {"id": 7}}),
    case("_is_own_message", {"message": {"from_user": {"id": 7}}}, state={"_bot": None}),
    case("_is_own_message", {"message": {"from_user": {"id": 7}}}),
    case("_is_own_message", {"message": {"from_user": {"id": 7}}}, state={"_bot": {"id": None}}),
)

# ── _sender_is_other_bot ──
add("_sender_is_other_bot",
    case("_sender_is_other_bot", {"message": {"from_user": {"is_bot": True, "id": 8}}}, state={"_bot": {"id": 7}}),
    case("_sender_is_other_bot", {"message": {"from_user": {"is_bot": True, "id": 7}}}, state={"_bot": {"id": 7}}),
    case("_sender_is_other_bot", {"message": {"from_user": {"is_bot": False, "id": 8}}}, state={"_bot": {"id": 7}}),
    case("_sender_is_other_bot", {"message": {"from_user": {"is_bot": True, "id": 8}}}, state={"_bot": None}),
    case("_sender_is_other_bot", {"message": {"from_user": {"is_bot": True}}}, state={"_bot": {"id": 7}}),
    case("_sender_is_other_bot", {"message": {"from_user": {"is_bot": True, "id": 8}}}),
    case("_sender_is_other_bot", {"message": {}}, state={"_bot": {"id": 7}}),
)

# ── _redact_telegram_error_text ──
add("_redact_telegram_error_text",
    case("_redact_telegram_error_text", {"error": None, "proxy_url": None}, errors=ERR),
    case("_redact_telegram_error_text", {"error": "proxyerr"}, errors=ERR,
         io={"redact_sensitive_text": "redacted"}),
    case("_redact_telegram_error_text", {"error": "empty"}, errors=ERR),
    case("_redact_telegram_error_text", {"error": "proxyerr", "proxy_url": "http://u:p@host"}, errors=ERR,
         io={"describe_proxy_error": "http://u:***@host", "redact_sensitive_text": "redacted"}),
    case("_redact_telegram_error_text", {"error": "proxyerr", "proxy_url": "http://host"}, errors=ERR,
         io={"describe_proxy_error": None, "redact_sensitive_text": "redacted"}),
    case("_redact_telegram_error_text", {"error": "proxyerr"}, errors=ERR,
         io={"redact_sensitive_text": raise_sc("ValueError", "x")}),
    case("_redact_telegram_error_text", {"error": "proxyerr"}, errors=ERR,
         io={"redact_sensitive_text": raise_sc("SystemExit", "halt")}, expect_fail_closed=True),
    case("_redact_telegram_error_text", {"error": "proxyerr", "proxy_url": "http://host"}, errors=ERR,
         io={"describe_proxy_error": raise_sc("KeyboardInterrupt", "stop")}, expect_fail_closed=True),
)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = [json.dumps(c, ensure_ascii=False, separators=(",", ":")) for c in CASES]
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    from collections import Counter
    counts = Counter(c["func"] for c in CASES)
    ios = set()
    for c in CASES:
        ios |= set((c["in"].get("io") or {}))
    print(f"wrote {len(CASES)} cases to {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    print(f"  io seams covered ({len(ios)}): {sorted(ios)}")
    assert all(v >= 6 for v in counts.values()), "every func needs >=6 cases"


if __name__ == "__main__":
    main()
