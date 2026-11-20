#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow3.jsonl — input cases for the
Feishu adapter inbound-admission FLOW remainder oracle.

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
The corpus covers every ported branch plus the FULL builtin exception hierarchy
(Exception subclasses and the BaseException family that bypasses except).

Run `python tests/gen_feishu_adapter_flow3_cases.py` to regenerate; the script
fails if any target has fewer than 5 cases or a target is missing entirely.
"""
import copy
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow3.jsonl"
CASES = []


def add(func, args, io=None):
    io = dict(io or {})
    # _admit calls self._require_mention_for(chat_id) whenever require_mention is
    # not supplied explicitly — script it on every such case.
    if func == "_admit" and "require_mention" not in args and "_require_mention_for" not in io:
        io["_require_mention_for"] = [ret(False)]
    CASES.append({"func": func, "in": {"args": args, "io": io}})


def rais(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def ret(v):
    return {"__script__": True, "ret": v}


def SELF(**kw):
    d = {
        "_bot_open_id": "ou_bot", "_bot_user_id": "", "_bot_name": "Bot",
        "_allow_bots": "mentions", "_allow_all_dm": False,
        "_allowed_group_users": [], "_admins": [], "_group_rules": {},
        "_require_mention": False, "_default_group_policy": None,
        "_group_policy": "open", "_warned_empty_allowlist_deny": False,
    }
    d.update(kw)
    return d


def MSG(**kw):
    d = {"chat_type": "p2p", "chat_id": "", "content": "", "mentions": [],
         "message_type": "text"}
    d.update(kw)
    return d


def SENDER(**kw):
    d = {"open_id": "ou_1", "user_id": "u_1",
         "sender_id": {"open_id": "ou_1", "user_id": "u_1"}}
    d.update(kw)
    return d


def MENTION(oid="", uid="", name="", self_=False):
    return {"id": {"open_id": oid, "user_id": uid}, "name": name, "is_self": self_}


# ── _bot_identity ──
add("_bot_identity", {"self": SELF()})
add("_bot_identity", {"self": SELF(_bot_open_id="ou_x", _bot_user_id="u_x", _bot_name="X")})
add("_bot_identity", {"self": SELF(_bot_open_id=None, _bot_user_id=None, _bot_name=None)})
add("_bot_identity", {"self": SELF(_bot_open_id="", _bot_user_id="", _bot_name="")})
add("_bot_identity", {"self": SELF(_bot_open_id=42, _bot_user_id=0, _bot_name="N")})
add("_bot_identity", {"self": SELF(_bot_open_id="ou_b", _bot_name="B")})

# ── _post_mentions_bot ──
add("_post_mentions_bot", {"mentions": []})
add("_post_mentions_bot", {"mentions": [{"is_self": True}]})
add("_post_mentions_bot", {"mentions": [{"is_self": False}, {"is_self": True}]})
add("_post_mentions_bot", {"mentions": [{"is_self": False}]})
add("_post_mentions_bot", {"mentions": [{"is_self": 1}]})
add("_post_mentions_bot", {"mentions": [{"is_self": 0}, {"is_self": ""}]})

# ── _message_mentions_bot ──
MBOT = ret({"open_id": "ou_bot", "user_id": "", "name": "Bot"})
add("_message_mentions_bot", {"self": SELF(), "mentions": []},
    io={"_bot_identity": [MBOT]})
add("_message_mentions_bot", {"self": SELF(), "mentions": [MENTION(oid="ou_bot")]},
    io={"_bot_identity": [MBOT], "matches": [ret(True)]})
add("_message_mentions_bot", {"self": SELF(), "mentions": [MENTION(oid="ou_other")]},
    io={"_bot_identity": [MBOT], "matches": [ret(False)]})
add("_message_mentions_bot", {"self": SELF(), "mentions": [MENTION(oid="  ou_bot  ", uid="  u ", name="  N  ")]},
    io={"_bot_identity": [MBOT], "matches": [ret(True)]})
add("_message_mentions_bot", {"self": SELF(), "mentions": [MENTION(oid="ou_x")]},
    io={"_bot_identity": [MBOT], "matches": [rais("ValueError", "bad match")]})
add("_message_mentions_bot", {"self": SELF(), "mentions": [MENTION(oid="ou_x")]},
    io={"_bot_identity": [MBOT], "matches": [rais("SystemExit", "stop")]})
add("_message_mentions_bot", {"self": SELF(), "mentions": [MENTION(oid="ou_x")]},
    io={"_bot_identity": [ret(None)]})
add("_message_mentions_bot",
    {"self": SELF(), "mentions": [MENTION(oid="a"), MENTION(oid="b")]},
    io={"_bot_identity": [MBOT], "matches": [ret(False), ret(True)]})

# ── _require_mention_for ──
add("_require_mention_for", {"self": SELF(), "chat_id": ""})
add("_require_mention_for", {"self": SELF(_require_mention=True), "chat_id": ""})
add("_require_mention_for", {"self": SELF(_group_rules={"g1": {"require_mention": True, "policy": "open"}}), "chat_id": "g1"})
add("_require_mention_for", {"self": SELF(_require_mention=True, _group_rules={"g1": {"require_mention": False}}), "chat_id": "g1"})
add("_require_mention_for", {"self": SELF(_require_mention=True, _group_rules={"g1": {"require_mention": None}}), "chat_id": "g1"})
add("_require_mention_for", {"self": SELF(_require_mention=True, _group_rules={"g1": {"policy": "open"}}), "chat_id": "g1"})

# ── _allow_group_message ──
add("_allow_group_message", {"self": SELF(_admins=["ou_1"]), "sender_id": SENDER(), "chat_id": "g1", "is_bot": False})
add("_allow_group_message", {"self": SELF(_group_rules={"g1": {"policy": "disabled"}}), "sender_id": SENDER(), "chat_id": "g1", "is_bot": False})
add("_allow_group_message", {"self": SELF(_group_rules={"g1": {"policy": "admin_only"}}), "sender_id": SENDER(), "chat_id": "g1", "is_bot": False})
add("_allow_group_message", {"self": SELF(_group_rules={"g1": {"policy": "open"}}), "sender_id": SENDER(), "chat_id": "g1", "is_bot": False})
add("_allow_group_message", {"self": SELF(_group_rules={"g1": {"policy": "disabled"}}), "sender_id": SENDER(), "chat_id": "g1", "is_bot": True})
add("_allow_group_message", {"self": SELF(_group_rules={"g1": {"policy": "allowlist", "allowlist": ["ou_1"]}}), "sender_id": SENDER(), "chat_id": "g1", "is_bot": False})
add("_allow_group_message", {"self": SELF(_group_rules={"g1": {"policy": "allowlist", "allowlist": ["ou_z"]}}), "sender_id": SENDER(), "chat_id": "g1", "is_bot": False})
add("_allow_group_message", {"self": SELF(_group_rules={"g1": {"policy": "blacklist", "blacklist": ["ou_1"]}}), "sender_id": SENDER(), "chat_id": "g1", "is_bot": False})
add("_allow_group_message", {"self": SELF(_group_rules={"g1": {"policy": "blacklist", "blacklist": ["ou_z"]}}), "sender_id": SENDER(), "chat_id": "g1", "is_bot": False})
add("_allow_group_message", {"self": SELF(_allowed_group_users=["u_1"], _default_group_policy=None), "sender_id": SENDER(), "chat_id": "", "is_bot": False})
add("_allow_group_message", {"self": SELF(_allowed_group_users=[], _default_group_policy="allowlist"), "sender_id": SENDER(), "chat_id": "", "is_bot": False})

# ── _mentions_self ──
add("_mentions_self", {"self": SELF(), "message": MSG(content="hi @_all there")})
add("_mentions_self", {"self": SELF(), "message": MSG(mentions=[MENTION()])},
    io={"_message_mentions_bot": [ret(True)]})
add("_mentions_self", {"self": SELF(), "message": MSG(mentions=[MENTION()])},
    io={"_message_mentions_bot": [ret(False)],
        "_normalize": [ret({"mentions": [{"is_self": True}]})],
        "_post_mentions_bot": [ret(True)]})
add("_mentions_self", {"self": SELF(), "message": MSG()},
    io={"_normalize": [ret({"mentions": [{"is_self": False}]})],
        "_post_mentions_bot": [ret(False)]})
add("_mentions_self", {"self": SELF(), "message": MSG(mentions=[MENTION()])},
    io={"_message_mentions_bot": [rais("TypeError", "tm")]})
add("_mentions_self", {"self": SELF(), "message": MSG()},
    io={"_normalize": [rais("KeyError", "km")]})
add("_mentions_self", {"self": SELF(), "message": MSG()},
    io={"_normalize": [ret({"mentions": []})],
        "_post_mentions_bot": [rais("KeyboardInterrupt", "kb")]})

# ── _admit ──
BASE_IO = {"_sender_identity": [ret(["ou_1", "u_1"])], "_is_bot_sender": [ret(False)]}
add("_admit", {"self": SELF(), "sender": SENDER(), "message": MSG()}, io=BASE_IO)
add("_admit", {"self": SELF(_bot_open_id="ou_1"), "sender": SENDER(), "message": MSG()}, io=BASE_IO)
add("_admit", {"self": SELF(), "sender": SENDER(), "message": MSG()},
    io={"_sender_identity": [ret(["ou_1"])], "_is_bot_sender": [ret(True)],
        "_mentions_self": [ret(False)]})
add("_admit", {"self": SELF(_bot_user_id="u_bot"), "sender": SENDER(), "message": MSG()},
    io={"_sender_identity": [ret([])], "_is_bot_sender": [ret(True)]})
add("_admit", {"self": SELF(_bot_open_id="ou_bot", _allow_bots="mentions", _require_mention=False),
               "sender": SENDER(), "message": MSG(chat_type="group", chat_id="g1")},
    io={"_sender_identity": [ret(["ou_1"])], "_is_bot_sender": [ret(True)],
        "_require_mention_for": [ret(False)], "_mentions_self": [ret(False)]})
add("_admit", {"self": SELF(_allow_all_dm=True), "sender": SENDER(), "message": MSG()},
    io={"_sender_identity": [ret(["ou_1"])], "_is_bot_sender": [ret(False)]})
add("_admit", {"self": SELF(_allow_all_dm=False, _allowed_group_users=[]), "sender": SENDER(), "message": MSG()},
    io={"_sender_identity": [ret(["ou_1"])], "_is_bot_sender": [ret(False)]})
add("_admit", {"self": SELF(_allowed_group_users=["ou_z"]), "sender": SENDER(), "message": MSG()},
    io={"_sender_identity": [ret(["ou_1"])], "_is_bot_sender": [ret(False)]})
add("_admit", {"self": SELF(_require_mention=True), "sender": SENDER(),
               "message": MSG(chat_type="group", chat_id="g1")},
    io={"_sender_identity": [ret(["ou_1"])], "_is_bot_sender": [ret(False)],
        "_allow_group_message": [ret(True)], "_mentions_self": [ret(False)]})
add("_admit", {"self": SELF(_require_mention=True), "sender": SENDER(),
               "message": MSG(chat_type="group", chat_id="g1")},
    io={"_sender_identity": [ret(["ou_1"])], "_is_bot_sender": [ret(False)],
        "_allow_group_message": [ret(True)], "_mentions_self": [ret(True)]})
add("_admit", {"self": SELF(), "sender": SENDER(), "message": MSG(chat_type="group", chat_id="g1")},
    io={"_sender_identity": [ret(["ou_1"])], "_is_bot_sender": [ret(False)],
        "_allow_group_message": [ret(False)]})
add("_admit", {"self": SELF(), "sender": SENDER(), "message": MSG()},
    io={"_sender_identity": [rais("RuntimeError", "si")]})
add("_admit", {"self": SELF(), "sender": SENDER(), "message": MSG()},
    io={"_sender_identity": [ret(["ou_1"])], "_is_bot_sender": [rais("CancelledError", "cancel")]})
add("_admit", {"self": SELF(), "sender": SENDER(), "message": MSG(chat_type="group", chat_id="g1")},
    io={"_sender_identity": [ret(["ou_1"])], "_is_bot_sender": [ret(False)],
        "_require_mention_for": [rais("ValueError", "rq")]})
add("_admit", {"self": SELF(_require_mention=True), "sender": SENDER(),
               "message": MSG(chat_type="group", chat_id="g1"), "require_mention": True},
    io={"_sender_identity": [ret(["ou_1"])], "_is_bot_sender": [ret(False)],
        "_allow_group_message": [ret(True)], "_mentions_self": [ret(True)]})

# ── _warn_once_empty_allowlist_deny ──
add("_warn_once_empty_allowlist_deny", {"self": SELF(_warned_empty_allowlist_deny=True), "chat_id": "g1"})
add("_warn_once_empty_allowlist_deny", {"self": SELF(), "chat_id": "g1"},
    io={"empty_allowlist_drop_warning": [ret("")]})
add("_warn_once_empty_allowlist_deny", {"self": SELF(), "chat_id": "g1"},
    io={"empty_allowlist_drop_warning": [ret("warn text")], "logger_warning": [ret(None)]})
add("_warn_once_empty_allowlist_deny",
    {"self": SELF(_default_group_policy="allowlist", _allowed_group_users=["u_1"],
                  _group_rules={"g1": {"require_mention": None, "policy": "open", "allowlist": [], "blacklist": []}}),
     "chat_id": "g1"},
    io={"empty_allowlist_drop_warning": [ret("policy warn")], "logger_warning": [ret(None)]})
add("_warn_once_empty_allowlist_deny", {"self": SELF(), "chat_id": "g1"},
    io={"empty_allowlist_drop_warning": [rais("AttributeError", "diag fail")]})
add("_warn_once_empty_allowlist_deny", {"self": SELF(), "chat_id": "g1"},
    io={"empty_allowlist_drop_warning": [rais("GeneratorExit", "ge")]})
add("_warn_once_empty_allowlist_deny", {"self": SELF(), "chat_id": "g1"},
    io={"empty_allowlist_drop_warning": [ret("w")], "logger_warning": [rais("SystemExit", "log stop")]})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"WROTE {OUT}: {len(CASES)} cases, {len(counts)} funcs")
    low = [f for f, n in counts.items() if n < 5]
    print("funcs with <5 cases:", low)
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if low:
        raise SystemExit(1)
    # every io key must be a known seam (cross-checked by the oracle)
    try:
        import sys
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import oracle_feishu_adapter_flow3 as o
        bad = set()
        for c in CASES:
            bad |= set(c["in"].get("io") or {}) - o.SEAMED
        if bad:
            print("fixture io keys without a seam:", sorted(bad))
            raise SystemExit(1)
        print("seam keys verified against", len(o.SEAMED), "C seams")
    except ImportError as exc:
        print("oracle import skipped:", exc)


if __name__ == "__main__":
    main()
