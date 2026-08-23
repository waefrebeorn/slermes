"""twin for hermes_cli_commands_get_suggestion.

Traces the LIVE
hermes_cli/commands.py::SlashCommandAutoSuggest.get_suggestion
still-typing-the-command-name branch over the C port's rung ladder:

  B1 null input / B2 slash strip / B3 empty word / B4 best-match-found,
  V-event = ghost-text remainder ("" for None).

The live suggester is driven through the real method with a stub document;
completer is None so the allowlist filter is inert on both sides. Per-command
iteration emits no events because the C registry and LIVE's COMMANDS map
(alias-inclusive, length-sorted) differ in membership/order — see gate.json
notes for the excluded divergent prefixes.
"""
import os, sys, types, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_hermes_cli_commands", os.path.join(DEV, "hermes_cli", "commands.py"))
_cmds = ilu.module_from_spec(_spec)
sys.modules["wc_live_hermes_cli_commands"] = _cmds
try:
    _spec.loader.exec_module(_cmds)
    from prompt_toolkit.auto_suggest import AutoSuggest  # noqa: F401
    _suggester = _cmds.SlashCommandAutoSuggest(None, None)
except Exception:  # pragma: no cover
    _suggester = None


def _live_suggestion(text):
    """Drive the LIVE get_suggestion; return remainder string ('' if none)."""
    if _suggester is None:
        return None
    doc = types.SimpleNamespace(text_before_cursor=text)
    sugg = _suggester.get_suggestion(None, doc)
    return sugg.text if sugg is not None else None


def trace(partial):
    c = Cup("get_suggestion")
    if partial is None:                                          # B1
        c.branch(True)
        c.valstr("")
        return c.finalize()
    c.branch(False)
    has_slash = partial.startswith("/")                          # B2
    word = partial[1:] if has_slash else partial
    c.branch(has_slash)
    if len(word) == 0:                                           # B3
        c.branch(True)
        c.valstr("")
        return c.finalize()
    c.branch(False)
    rem = _live_suggestion(partial)                              # registry walk
    if rem is None:                                              # B4
        c.branch(True)
        c.valstr("")
        return c.finalize()
    c.branch(False)
    c.valstr(rem)
    return c.finalize()


def run(inputs):
    return [trace(s) for s in inputs]
