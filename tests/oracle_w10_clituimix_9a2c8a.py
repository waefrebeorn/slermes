#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_tui_mixin.py (gate w10_clituimix_9a2c8a).

The live module imports prompt_toolkit and cli internals eagerly; inert stubs
are registered in sys.modules so the real pure helpers can be loaded and called
via spec_from_file_location. The i18n seam t() is the identity on its key.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/cli_tui_mixin.py"
sys.path.insert(0, QUARRY)


def stub(name, **attrs):
    mod = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        pm = sys.modules.get(parent)
        if pm is None:
            pm = types.ModuleType(parent)
            pm.__path__ = []
            sys.modules[parent] = pm
        setattr(pm, parts[i], sys.modules[".".join(parts[:i + 1])])
    return mod


_hc = stub("hermes_cli")
_hc.__path__ = [QUARRY + "/hermes_cli"]

stub("agent.i18n", t=lambda key, **kw: key)
stub("agent.interrupt_compat", request_hard_interrupt=lambda *a, **k: None)
stub("hermes_cli.commands_completion", SlashCommandAutoSuggest=object, cli_slash_completer=object())
for m, attrs in [
    ("prompt_toolkit.filters", {"Condition": object}),
    ("prompt_toolkit.history", {"FileHistory": object}),
    ("prompt_toolkit.key_binding", {"KeyBindings": object}),
    ("prompt_toolkit.layout", {"ConditionalContainer": object, "FormattedTextControl": object,
                               "Layout": object, "Window": object, "WindowAlign": object}),
    ("prompt_toolkit.layout.dimension", {"Dimension": object}),
    ("prompt_toolkit.layout.menus", {"CompletionsMenu": object}),
    ("prompt_toolkit.layout.processors", {"ConditionalProcessor": object, "PasswordProcessor": object,
                                          "Processor": object, "Transformation": object}),
    ("prompt_toolkit.styles", {"Style": object}),
    ("prompt_toolkit.widgets", {"TextArea": object}),
    ("hermes_cli.cli_enter_burst", {"RAPID_INPUT_ENTER_WINDOW_S": 0.3}),
    ("hermes_cli.cli_footer_split", {"FooterSplit": object}),
    ("hermes_cli.cli_panel", {"_Panel": object}),
    ("hermes_cli.cli_tui_wrap", {"_prefix_wrapped_rows": lambda *a, **k: [],
                                 "_wrap_rows": lambda *a, **k: []}),
]:
    stub(m, **attrs)

spec = importlib.util.spec_from_file_location("w10_clituimix_9a2c8a_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_clituimix_9a2c8a_live"] = live
spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "num_prefix":
        return live._num_prefix(int(r.get("i") or 0))
    if func == "choice":
        return live._approval_choice_label(r.get("choice") or "")
    if func == "tn":
        return live._tn(r.get("key") or "", int(r.get("count") or 0))
    if func == "audio_bar":
        fake = types.SimpleNamespace(
            _voice_recorder=types.SimpleNamespace(current_rms=int(r.get("rms") or 0)))
        return live.CLITuiMixin._audio_level_bar(fake)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except json.JSONDecodeError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w10_clituimix_9a2c8a: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH w10_clituimix_9a2c8a {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w10_clituimix_9a2c8a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
