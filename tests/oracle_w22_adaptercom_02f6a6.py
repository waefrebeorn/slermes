#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/adapter_component_auth.py
(gate w22_adaptercom_02f6a6).

Every decision runs live. Four things the module touches are seams and are
injected per case:

  env        -- `_scoped_gate_env` is rebound to `(env.get(name) or "").strip()`,
                exactly what the real helper returns for a single-profile
                install (no secret scope installed, no multiplex).
  pairing    -- `gateway.pairing` is replaced by a stub `PairingStore` whose
                `is_approved` answers from the case, so no DB is read.
  live_auth  -- the per-call callback the adapter passes in is built from the
                case (absent -> None, true/false -> a bool, "none" -> None).
  update     -- `approval_actor.update_prompt_refusal` reads a marker file out
                of the hermes home; it is rebound to `_authority_refusal` over
                the authority the fixture carries, which is what the marker
                read hands it. `approval_actor.refusal_text` (the i18n catalog)
                is rebound to the identity, so the compared value is the
                requester key the catalog would word.

The Python "\0no-requester" sentinel is canonicalized to the literal text the C
side carries ("\\0no-requester").
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "discord" / "adapter_component_auth.py"
GATE = "w22_adaptercom_02f6a6"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import gateway.approval_actor as approval_actor  # noqa: E402

_spec = importlib.util.spec_from_file_location("adapter_component_auth_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["adapter_component_auth_live"] = live
_spec.loader.exec_module(live)

WANTED = {"chat_context", "check_auth", "opener"}

# --- seams -----------------------------------------------------------------

_ENV = {}
PAIRED = False


def gate_env(name, default=""):
    return (str(_ENV.get(name) or default)).strip()


class _PairingStore:
    def is_approved(self, platform, uid):
        return bool(PAIRED)


pairing_mod = types.ModuleType("gateway.pairing")
pairing_mod.PairingStore = _PairingStore
sys.modules["gateway.pairing"] = pairing_mod

approval_actor.refusal_text = lambda requester: requester


def stub_update_refusal(actor_id, admin_ids=(), extra=None):
    return approval_actor._authority_refusal(_UPDATE_AUTHORITY, actor_id, admin_ids, extra)


approval_actor.update_prompt_refusal = stub_update_refusal
_UPDATE_AUTHORITY = None

live._scoped_gate_env = gate_env


def render(value):
    if value == "\0no-requester":
        return "\\0no-requester"
    return value


# --- case builders ---------------------------------------------------------


def _user_ns(obj):
    if not isinstance(obj, dict):
        return None
    ns = types.SimpleNamespace()
    if "id" in obj:
        ns.id = obj["id"]
    if "roles" in obj:
        roles = obj["roles"]
        if isinstance(roles, list):
            ns.roles = [types.SimpleNamespace(id=r["id"]) if isinstance(r, dict) else r for r in roles]
        else:
            ns.roles = roles
    return ns


def interaction_ns(obj):
    if not isinstance(obj, dict):
        return None
    ns = types.SimpleNamespace()
    if "channel_id" in obj:
        ns.channel_id = obj["channel_id"]
    if "guild" in obj:
        ns.guild = obj["guild"]
    if obj.get("channel_kind") is not None:
        ns.channel = types.SimpleNamespace()
    return ns


def chat_context(r):
    inter = interaction_ns(r.get("interaction"))
    cls = type("Thread", (), {})
    thread_cls = r.get("interaction")
    name = "Thread"
    if isinstance(thread_cls, dict) and isinstance(thread_cls.get("thread_cls"), str):
        name = thread_cls["thread_cls"]
    kind = (r.get("interaction") or {}).get("channel_kind") if isinstance(r.get("interaction"), dict) else None
    cls = type(name, (), {})
    if isinstance(inter, types.SimpleNamespace) and kind is not None:
        inter.channel = cls() if kind == name else types.SimpleNamespace()
    got = live.component_chat_context(inter, cls)
    return [got[0], got[1], got[2]]


def check_auth(r):
    global _ENV, PAIRED
    _ENV = r.get("env") or {}
    PAIRED = bool(r.get("paired"))
    inter = interaction_ns(r.get("interaction"))
    if isinstance(inter, types.SimpleNamespace):
        inter.user = _user_ns((r.get("interaction") or {}).get("user"))
    live_auth = r.get("live_auth")
    if live_auth is None or live_auth == "none":
        callback = (lambda i: None) if live_auth == "none" else None
    else:
        value = bool(live_auth)
        callback = (lambda i, v=value: v)
    users = r.get("allowed_user_ids")
    roles = r.get("allowed_role_ids")
    return 1 if live._component_check_auth(
        inter, set(users) if users is not None else None,
        set(roles) if roles is not None else None, callback) else 0


def opener(r):
    global _UPDATE_AUTHORITY
    spec = r.get("view") or {}
    view = types.SimpleNamespace()
    if "answer_extra" in spec:
        view.answer_extra = spec["answer_extra"]
    if spec.get("has_on_selected"):
        view.on_model_selected = types.SimpleNamespace(prompt_authority=spec.get("prompt_authority"))
    if "is_update_prompt" in spec:
        view.is_update_prompt = spec["is_update_prompt"]
    _UPDATE_AUTHORITY = spec.get("update_authority")
    inter = interaction_ns(r.get("interaction"))
    if isinstance(inter, types.SimpleNamespace):
        inter.user = _user_ns((r.get("interaction") or {}).get("user"))
    return render(live.opener_refusal(view, inter))


def expected(r):
    f = r.get("func")
    if f == "chat_context":
        return chat_context(r)
    if f == "check_auth":
        return check_auth(r)
    if f == "opener":
        return opener(r)
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
