#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/teams/summary_writer.py (gate w19_summarywri_99693d).

Extracts the live helpers/methods via ast.parse and execs them into a stub-filled globals dict:
the i18n catalog and the env fallbacks are injected seams, modelled identically in C.
"""
import ast
import html
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = Path("C:/Users/eman5/hermes-agent")
SRC = Q / "plugins" / "platforms" / "teams" / "summary_writer.py"
GATE = "w19_summarywri_99693d"

# The i18n catalog the C side carries as literals.
CAT = {
    "platform.teams.summary.empty": "(no summary)",
    "platform.teams.summary.summary_line": "Summary: {summary}",
    "platform.teams.summary.key_decisions": "Key decisions",
    "platform.teams.summary.action_items": "Action items",
    "platform.teams.summary.risks": "Risks",
    "platform.teams.summary.heading": "Summary",
    "platform.teams.summary.none_item": "(none)",
    "platform.teams.summary.title": "Meeting {meeting_id}",
    "platform.teams.summary.title_fallback": "(untitled)",
}

METHODS = {"_text", "_bullet_lines", "_title", "_render_summary_markdown",
           "_render_summary_html", "_resolve_delivery_config"}


def _t(key, **kw):
    text = CAT.get(key, key)
    return text.format(**kw) if kw else text


class _FakeOs:
    def __init__(self, env):
        self._env = env or {}

    def getenv(self, name, default=None):
        return self._env.get(name, default)


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "w19_summary_live", "__builtins__": __builtins__, "t": _t, "html": html,
         "os": _FakeOs({})}
    for node in tree.body:
        target = None
        if isinstance(node, ast.Assign):
            for t_ in node.targets:
                if getattr(t_, "id", None) in ("_LIST_SECTIONS", "_ENV_KEYS"):
                    target = node
        elif isinstance(node, ast.AnnAssign) and getattr(node.target, "id", None) in (
                "_LIST_SECTIONS", "_ENV_KEYS"):
            target = node
        if target is not None:
            exec(compile(ast.Module(body=[target], type_ignores=[]), str(SRC), "exec"), g)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_parse_bool":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        if isinstance(node, ast.ClassDef) and node.name == "TeamsSummaryWriter":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in METHODS:
                    sub.decorator_list = []
                    if sub.name == "_bullet_lines":      # classmethod: drop `cls`
                        sub.args.args = sub.args.args[1:]
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), str(SRC), "exec"), g)
    return g


G = extract()


class Obj:
    def __init__(self, **kw):
        self.__dict__.update(kw)


def make_payload(data):
    obj = Obj()
    for key, value in (data or {}).items():
        if key == "meeting_ref":
            if value is None:
                setattr(obj, key, None)
            else:
                setattr(obj, key, Obj(**dict(value or {})))
        else:
            setattr(obj, key, value)
    return obj


class _Self:
    """The renderers' self: the real coerced helpers, bound as the class binds them."""
    _text = staticmethod(G["_text"])
    _title = staticmethod(G["_title"])
    _bullet_lines = staticmethod(G["_bullet_lines"])


def make_platform_cfg(data):
    if data is None:
        return None
    kw = {"extra": data.get("extra"), "token": data.get("token")}
    home = data.get("home_channel_chat_id")
    kw["home_channel"] = Obj(chat_id=home) if home else None
    return Obj(**kw)


def run_case(r):
    f = r.get("func")
    if f == "parse_bool":
        value = r.get("value")
        if isinstance(value, float) and value.is_integer() and not isinstance(value, bool):
            value = int(value)
        return bool(G["_parse_bool"](value, default=bool(r.get("default"))))
    if f == "text":
        return G["_text"](r.get("value"), r.get("fallback", "(no summary)"))
    if f == "bullet_lines":
        return G["_bullet_lines"](r.get("values"))
    if f == "title":
        return G["_title"](make_payload(r.get("payload")))
    if f == "render_markdown":
        return G["_render_summary_markdown"](_Self(), make_payload(r.get("payload")))
    if f == "render_html":
        return G["_render_summary_html"](_Self(), make_payload(r.get("payload")))
    if f == "resolve_config":
        env = r.get("env") or {}
        G["os"] = _FakeOs(env)
        G["_get_scoped_secret"] = lambda name, default="": env.get(name, default)
        holder = _Self()
        holder._platform_config = make_platform_cfg(r.get("platform_cfg"))
        return G["_resolve_delivery_config"](holder, r.get("config"))
    raise KeyError(f)


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
        if f not in {"parse_bool", "text", "bullet_lines", "title", "render_markdown",
                     "render_html", "resolve_config"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:  # noqa: BLE001
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
