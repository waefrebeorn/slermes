#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/adapter_vault_code.py
(gate w20_adaptervau_8ec608).

The live module is imported from the quarry and its real VaultCodeView class is
built with a stub discord.py and a stub _HermesView base. build_vault_code_view
binds ``_t_discord`` from the adapter module at build time, so the adapter's
names are re-pointed before the class is built: the compared values are the
card's own gate/auth/strip/disable/stop decisions and its selection of
notice/footer keys (the i18n catalog and discord.py stay upstream).
"""
import asyncio
import importlib.util
import json
import os
import sys
import types

sys.dont_write_bytecode = True
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import gateway.platforms.base as gbase                       # noqa: E402

# --- discord.py stub -------------------------------------------------------

discord_stub = types.ModuleType("discord")


class _Modal:
    def __init__(self, title=None, timeout=None):
        self.title = title
        self.timeout = timeout
        self.items = []

    def add_item(self, item):
        self.items.append(item)


class _TextInput:
    def __init__(self, **kw):
        self.__dict__.update(kw)
        self.value = None


class _Label:
    def __init__(self, text=None, component=None):
        self.text = text
        self.component = component


class _Embed:
    def __init__(self, title=None, color=None):
        self.title = title
        self.color = color


discord_stub.ui = types.SimpleNamespace(
    Modal=_Modal, TextInput=_TextInput, Label=_Label,
    button=lambda **kw: (lambda fn: fn))
discord_stub.ButtonStyle = types.SimpleNamespace(blurple=1)
discord_stub.Embed = _Embed
discord_stub.Color = types.SimpleNamespace(gold=lambda: "gold")
sys.modules["discord"] = discord_stub


# --- _HermesView stub ------------------------------------------------------

class BaseView:
    def __init__(self, allowed_user_ids, allowed_role_ids=None, timeout=None):
        self.allowed_user_ids = set(allowed_user_ids)
        self.timeout = timeout
        self.resolved = False
        self.admitted = True
        self.disabled = 0
        self.stopped = 0
        self.expired = []
        self.gate_notes = []

    def _localize_buttons(self, **kw):
        self.button_labels = kw

    def _check_auth(self, interaction):
        return self.admitted

    async def _gate(self, interaction, resolved_msg=None, unauth_msg=None):
        if not self._check_auth(interaction):
            self.gate_notes.append(unauth_msg)
            return False
        return True

    def _disable_all(self):
        self.disabled += 1

    def stop(self):
        self.stopped += 1

    async def _expire_embed(self, text):
        self.expired.append(text)


# --- live module -----------------------------------------------------------

NAME = "w20_adaptervau_8ec608_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "platforms", "discord", "adapter_vault_code.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

_CURRENT = {}


def t_discord(key, limit, **kw):
    """Identity translation: keys stay keys, cut to the Discord field budget.

    The prompt body key resolves to the case's injected body text, which is what
    a real catalog lookup would have produced.
    """
    text = _CURRENT.get("body") if key.endswith(".vault_code.body") else str(key)
    return gbase._prefix_within_utf16_limit(text, max(0, limit))


# The card imports these lazily; drive the adapter module's names so the card's
# own policy is what gets compared.
import plugins.platforms.discord.adapter as adapter_real             # noqa: E402

adapter_real._t_discord = t_discord
adapter_real._DISCORD_EMBED_TITLE_LIMIT = 256
adapter_real.discord = discord_stub

VaultView = live.build_vault_code_view(discord_stub, BaseView)
adapter_real.VaultCodeView = VaultView


# --- interaction stub ------------------------------------------------------

class Response:
    def __init__(self):
        self.deferred = 0
        self.messages = []
        self.modals = []

    async def defer(self):
        self.deferred += 1

    async def send_message(self, text, ephemeral=False):
        self.messages.append((text, ephemeral))

    async def send_modal(self, modal):
        self.modals.append(modal)


class Interaction:
    def __init__(self, uid):
        self.user = types.SimpleNamespace(id=uid)
        self.response = Response()


class Record:
    def __init__(self, message):
        self._message = message

    def getMessage(self):
        return self._message


def expected(r):
    f = r.get("func")
    _CURRENT.clear()
    if f == "frame_kept":
        return bool(live._DropVaultCodeFrames().filter(Record(r.get("message", ""))))
    if f == "truncate_utf16":
        return gbase._prefix_within_utf16_limit(str(r.get("text") or ""),
                                                max(0, int(r.get("limit", 0))))
    if f == "check_auth":
        view = VaultView(site="s", user_id=str(r.get("user_id", "")), submit=lambda c: True,
                         allowed_user_ids={1}, timeout=10)
        view.admitted = bool(r.get("admitted"))
        return view._check_auth(Interaction(r.get("interaction_user_id")))
    if f == "submit_plan":
        accepted = bool(r.get("accepted"))
        view = VaultView(site="discord.gg", user_id="42", submit=lambda code: accepted,
                         allowed_user_ids={1}, timeout=10)
        view.admitted = bool(r.get("admitted"))
        inter = Interaction(42)
        asyncio.run(view.submit_code(inter, r.get("code") or ""))
        if not view.admitted:
            return {"gated": True}
        if inter.response.deferred:
            return {"deferred": True}
        notice, ephemeral = inter.response.messages[-1]
        return {"notice": notice, "footer": view.expired[-1], "ephemeral": ephemeral,
                "disabled": bool(view.disabled), "stopped": bool(view.stopped),
                "resolved": bool(view.resolved)}
    if f == "prompt_content":
        _CURRENT["body"] = r.get("body")

        class Mix(live.DiscordVaultCodeMixin):
            def __init__(self):
                self._allowed_user_ids = {1}
                self._allowed_role_ids = set()
                self.captured = None

            async def _send_prompt(self, chat_id, metadata, build, fail_log=None):
                payload, _view = build(None)
                self.captured = payload["content"]
                return "SENT"

        mix = Mix()
        asyncio.run(mix.send_vault_code_prompt("chat-1", "discord.gg", r.get("hint") or "",
                                               user_id="42", submit=lambda c: True,
                                               timeout_s=10))
        return mix.captured
    if f == "input_id":
        return live._INPUT_ID
    if f == "limits":
        return {"modal": live._MODAL_TEXT_LIMIT, "hint": live._HINT_LIMIT}
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
    except Exception as exc:  # noqa: BLE001 - report the live failure verbatim
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_adaptervau_8ec608: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
