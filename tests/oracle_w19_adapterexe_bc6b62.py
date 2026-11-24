#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/adapter_exec_approval_view.py
(gate w19_adapterexe_bc6b62).

The live module only builds a discord.py view class, so the oracle execs that inner
ClassDef against stub globals (base, discord, t, logger) and then drives the real
__init__ / _check_auth / retire / _resolve against recorded fakes.
"""
import ast, asyncio, json, sys, types, __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
GATE = "w19_adapterexe_bc6b62"
SRC = Path("C:/Users/eman5/hermes-agent/plugins/platforms/discord/adapter_exec_approval_view.py")
_ANN = __future__.annotations.compiler_flag


class Color:
    def __init__(self, name):
        self.name = name

    @staticmethod
    def green():
        return Color("green")

    @staticmethod
    def blue():
        return Color("blue")

    @staticmethod
    def purple():
        return Color("purple")

    @staticmethod
    def red():
        return Color("red")

    @staticmethod
    def dark_grey():
        return Color("dark_grey")


class _UI:
    @staticmethod
    def button(**kw):
        def deco(fn):
            return fn
        return deco


DISCORD = types.SimpleNamespace(
    ui=_UI(),
    ButtonStyle=types.SimpleNamespace(green=1, grey=2, blurple=3, red=4),
    Color=Color, Interaction=object, ui_button=_UI.button)


class _Logger:
    def __init__(self):
        self.warnings = 0

    def warning(self, *a, **k):
        self.warnings += 1

    def info(self, *a, **k):
        pass

    def error(self, *a, **k):
        pass


class Base:
    state = {}

    def __init__(self, allowed_user_ids, allowed_role_ids=None, timeout=None):
        self.allowed_user_ids = set(allowed_user_ids or ())
        self.allowed_role_ids = set(allowed_role_ids or ())
        self.timeout = timeout
        self.removed = []
        self.sent = []
        self.finalize = None
        self.expired = []
        self.gate_args = None
        self.button_labels = {}
        self.resolved = False

    def _localize_buttons(self, **kw):
        self.button_labels = dict(kw)

    def remove_item(self, item):
        self.removed.append(item)

    def _check_auth(self, interaction):
        return bool(Base.state.get("base_ok", True))

    def stop(self):
        pass

    def _disable_all(self):
        pass

    async def _expire_embed(self, footer, keep_controls=True):
        self.expired.append((footer, keep_controls))

    async def _gate(self, interaction, resolved_msg=None, unauth_msg=None):
        self.gate_args = (resolved_msg, unauth_msg)
        return bool(Base.state.get("gate_ok", True))

    async def _finalize_embed(self, interaction, color, text):
        self.finalize = (getattr(color, "name", str(color)), text)


LOGGER = _Logger()


def _t(key, **kw):
    return key if isinstance(key, str) else "<i18n>"


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    cls = None
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "build_exec_approval_view":
            for inner in node.body:
                if isinstance(inner, ast.ClassDef) and inner.name == "ExecApprovalView":
                    cls = inner
    if cls is None:
        raise SystemExit("ExecApprovalView class not found")
    g = {"__name__": "aev_live", "base": Base, "discord": DISCORD, "asyncio": asyncio,
         "Optional": object, "logger": LOGGER, "t": _t,
         "unauthorized": lambda: "UNAUTH",
         "_read_discord_prompt_timeout": lambda: 30.0}
    exec(compile(ast.Module(body=[cls], type_ignores=[]), str(SRC), "exec",
                 flags=_ANN, dont_inherit=True), g)
    View = g["ExecApprovalView"]
    for name in ("allow_once", "allow_session", "allow_always", "deny"):
        setattr(View, name, name)
    return View


VIEW = extract()


def build(r):
    Base.state = {"base_ok": bool(r.get("base_ok", True)), "gate_ok": bool(r.get("gate_ok", True))}
    LOGGER.warnings = 0
    v = VIEW(
        r.get("session_key", "sess-1"), set(r.get("allowed_user_ids") or []),
        allowed_role_ids=set(r.get("allowed_role_ids") or []),
        require_admin=bool(r.get("require_admin", False)),
        admin_user_ids=set(r.get("admin_user_ids") or []),
        allow_permanent=bool(r.get("allow_permanent", True)),
        allow_session=bool(r.get("allow_session", True)),
        smart_denied=bool(r.get("smart_denied", False)),
        request_id=r.get("request_id"),
        allow_once=bool(r.get("allow_once", True)),
        approval_extra=r.get("approval_extra"),
    )
    return v


class Interaction:
    def __init__(self, uid, name="tester"):
        self.user = types.SimpleNamespace(id=uid, display_name=name)
        self.response = types.SimpleNamespace(send_message=self._send)

    async def _send(self, text, ephemeral=False):
        sent.append(text)


sent = []


def stub_imports(r):
    m1 = types.ModuleType("gateway.approval_actor")
    m1.approval_refusal = lambda *a, **k: (r.get("refusal") or None)
    m1.refusal_text = lambda who: "REFUSAL"
    m2 = types.ModuleType("tools.approval")

    def _resolve_card(*a, **k):
        if r.get("resolve_raises"):
            raise RuntimeError("boom")
        return int(r.get("count", 0))

    m2.resolve_gateway_approval_card = _resolve_card
    sys.modules["gateway.approval_actor"] = m1
    sys.modules["tools.approval"] = m2


def run_case(r):
    f = r.get("func")
    if f == "init_buttons":
        v = build(r)
        return v.removed
    if f == "check_auth":
        v = build(r)
        ok = v._check_auth(Interaction(r.get("uid")))
        return {"ok": bool(ok), "warned": LOGGER.warnings > 0}
    if f == "footer_key":
        v = build(r)
        asyncio.run(v.retire(r.get("reason", "resolved")))
        return v.expired[0][0] if v.expired else None
    if f == "resolve_outcome":
        stub_imports(r)
        del sent[:]
        v = build(r)
        inst = Interaction(r.get("uid"), r.get("display_name", "tester"))
        try:
            asyncio.run(v._resolve(inst, r.get("choice", "once"),
                                   Color(r.get("color", "green")),
                                   r.get("label_key", "platform.discord.approval.resolved_once")))
        except RuntimeError:
            pass
        if not Base.state.get("gate_ok", True):
            acts = ["return"]
        elif v.sent or sent:
            first = (v.sent or sent)[0]
            if first == "REFUSAL":
                acts = ["refuse"]
            elif first == "platform.discord.approval.click_failed":
                acts = ["click_failed"]
            else:
                acts = ["no_longer_pending"]
        elif v.finalize is not None:
            acts = ["finalize"]
        else:
            acts = []
        return {"actions": acts,
                "finalize_color": v.finalize[0] if v.finalize else None,
                "finalize_text": v.finalize[1] if v.finalize else None,
                "resolved": bool(v.resolved),
                "backend_settled": bool(v._backend_settled)}
    return None


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
        f = r.get("func")
        if f not in {"init_buttons", "check_auth", "footer_key", "resolve_outcome"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
