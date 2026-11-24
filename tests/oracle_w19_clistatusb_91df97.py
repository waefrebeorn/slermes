#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_status_bar_mixin.py (gate w19_clistatusb_91df97).

The live module is re-executed from its own AST (module-level defs/classes/assigns) with the
prompt_toolkit / pet / i18n collaborators stubbed. Time is injected through a fake `time` module
so _format_prompt_elapsed and _fmt_stash_age see the fixture's `now`.
"""
import ast
import json
import sys
import types
import __future__

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/hermes_cli/cli_status_bar_mixin.py"
GATE = "w19_clistatusb_91df97"
_ANN = __future__.annotations.compiler_flag

if Q not in sys.path:
    sys.path.insert(0, Q)

NOW = {"t": 0.0}
CLI_CONFIG_HOLDER = {"cfg": None}


class FakeTime:
    def time(self):
        return NOW["t"]

    def monotonic(self):
        return NOW["t"]

    def sleep(self, _s):
        pass


def _mk(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


_mk("prompt_toolkit")
_mk("prompt_toolkit.utils", get_cwidth=lambda s: len(s or ""))
_mk("cli", CLI_CONFIG=CLI_CONFIG_HOLDER)


# The i18n catalog is a collaborator seam: agent.i18n cannot be imported under the gate's
# login shell (Path.home() has no HOME), so the en templates are pinned here, copied verbatim
# from locales/en.yaml. Both sides therefore format the same English strings.
_EN = {
    "cli.shared.duration_s": "{seconds}s",
    "cli.shared.duration_m": "{minutes}m",
    "cli.shared.duration_ms": "{minutes}m {seconds}s",
    "cli.shared.duration_hm": "{hours}h {minutes}m",
    "cli.shared.duration_hms": "{hours}h {minutes}m {seconds}s",
    "cli.shared.duration_dhm": "{days}d {hours}h {minutes}m",
    "cli.shared.just_now": "just now",
    "cli.shared.ago_seconds": "{seconds}s ago",
    "cli.shared.ago_minutes": "{minutes} min ago",
    "cli.shared.ago_hours": "{hours}h ago",
    "cli.status_bar.goal": "\u2299 goal",
    "cli.status_bar.goal_turns": "\u2299 goal {used}/{max}",
}


def _t(key, **kw):
    tpl = _EN.get(key)
    return tpl.format(**kw) if tpl is not None else key


def extract():
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    g = {"__name__": "sb_live", "t": _t, "errno": __import__("errno"),
         "shutil": __import__("shutil"), "threading": __import__("threading"),
         "time": FakeTime(), "Any": object, "Dict": dict, "Optional": object,
         "terminal_image": types.SimpleNamespace(
             supports_kitty_placeholders=lambda: False, kitty_color_hex=lambda i: "#000000"),
         "pet_render": types.SimpleNamespace(PetRenderer=object, kitty_image_id=lambda s: 0),
         "_format_context_length": lambda n: str(n),
         "__builtins__": __builtins__}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.ClassDef, ast.Assign, ast.AnnAssign)):
            mod = ast.Module(body=[node], type_ignores=[])
            exec(compile(mod, SRC, "exec", flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
CLS = G["CLIStatusBarMixin"]


def _fake_self():
    return types.SimpleNamespace()


def run_case(r):
    f = r.get("func")
    if f == "threshold":
        ladder = [(p[0], p[1]) for p in (r.get("ladder") or [])]
        return G["_threshold_style"](r.get("value"), ladder, r.get("fallback"))
    if f == "finite":
        return G["_finite"](r.get("v"))
    if f == "context_style":
        return CLS._status_bar_context_style(None, r.get("percent"))
    if f == "cache_style":
        return CLS._cache_hit_rate_style(None, r.get("pct"))
    if f == "battery_style":
        return CLS._battery_status_style(r.get("category"))
    if f == "compression_style":
        return CLS._compression_count_style(r.get("count"))
    if f == "context_bar":
        return CLS._build_context_bar(None, r.get("percent"), r.get("width", 10))
    if f == "prompt_elapsed":
        NOW["t"] = r.get("now", 0.0)
        return CLS._format_prompt_elapsed(r.get("start"), r.get("duration", 0.0),
                                          live=bool(r.get("live")))
    if f == "trim":
        return CLS._trim_status_bar_text(r.get("text", ""), r.get("max_width", 0))
    if f == "title_badge":
        res = CLS._status_title_badge(r.get("title", ""), r.get("width", 80))
        return list(res) if res is not None else None
    if f == "right_align":
        return CLS._right_align_status_title(r.get("text", ""), r.get("title", ""),
                                             r.get("width", 80))
    if f == "minimal_chrome":
        return CLS._use_minimal_tui_chrome(None, width=r.get("width", 80))
    if f == "scrollback":
        return CLS._scrollback_box_width(r.get("width"))
    if f == "cache_rate":
        res = CLS._cache_hit_rate(None, r.get("snapshot") or {}, r.get("precision", 1))
        return list(res) if res is not None else None
    if f == "goal":
        return CLS._status_bar_goal_segment(r.get("snapshot") or {})
    if f == "stash_age":
        NOW["t"] = r.get("now", 0.0)
        return CLS._fmt_stash_age(r.get("stashed_at", 0.0))
    if f == "field_set":
        CLI_CONFIG_HOLDER["cfg"] = r.get("cfg")
        sys.modules["cli"].CLI_CONFIG = r.get("cfg")
        res = CLS._get_status_bar_field_set(_fake_self())
        return sorted(res) if res is not None else None
    return "UNKNOWN_FUNC"


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
