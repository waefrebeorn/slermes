#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_stream_mixin.py (gate w15_clistream_56f8e8).

Stubs t() as identity, rich.markup.escape as identity, and injects the terminal
width through the fixture's `columns` field (live _terminal_columns is patched).
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parent, _, child = name.rpartition(".")
    if parent and parent in sys.modules:
        setattr(sys.modules[parent], child, m)
    return m


agent = _mod("agent")
agent.__path__ = [Q + "/agent"]
hcli = _mod("hermes_cli")
hcli.__path__ = [Q + "/hermes_cli"]
_mod("agent.i18n", t=lambda key, **kw: key)
_mod("agent.think_scrubber",
     THINK_OPEN_TAGS=["<think>"],
     THINK_CLOSE_TAGS=["</think>"])
rich = _mod("rich")
rich.__path__ = []
_mod("rich.markup", escape=lambda s: str(s))

SRC = Q + "/hermes_cli/cli_stream_mixin.py"
spec = importlib.util.spec_from_file_location("hermes_cli.cli_stream_mixin", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.cli_stream_mixin"] = live
spec.loader.exec_module(live)


class _Self:
    pass


STATE_KEYS = (
    "_stream_buf", "_stream_started", "_stream_box_opened", "_stream_text_ansi",
    "_stream_prefilt", "_in_reasoning_block", "_stream_last_was_newline",
    "_reasoning_box_opened", "_reasoning_buf", "_reasoning_preview_buf",
    "_stream_table_buf", "_in_stream_table", "_stream_box_live",
)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "slow_command_status":
            py = live.CLIStreamMixin._slow_command_status(
                live.CLIStreamMixin(), r.get("command", ""))
        elif func == "current_reasoning_callback":
            s = _Self()
            s.show_reasoning = r.get("show_reasoning")
            s.streaming_enabled = r.get("streaming_enabled")
            s.verbose = r.get("verbose")
            s._stream_reasoning_delta = "stream"
            s._on_reasoning = "on_reasoning"
            cb = live.CLIStreamMixin._current_reasoning_callback(s)
            py = cb if isinstance(cb, str) else None
        elif func == "reset_stream_state":
            s = _Self()
            for k in STATE_KEYS:
                setattr(s, k, "SENTINEL")
            s._tool_gen_announced = True
            live.CLIStreamMixin._reset_stream_state(s)
            assert not hasattr(s, "_tool_gen_announced")
            py = {k: getattr(s, k) for k in STATE_KEYS}
        elif func == "flush_reasoning_preview":
            columns = r.get("columns", 80)
            live._terminal_columns = lambda: columns
            s = _Self()
            s._reasoning_preview_buf = r.get("buf", "")
            captured = {}

            def _emit(text):
                captured["text"] = text

            s._emit_reasoning_preview = _emit
            live.CLIStreamMixin._flush_reasoning_preview(s, force=bool(r.get("force", False)))
            py = {"buf": s._reasoning_preview_buf,
                  "flush": captured.get("text") if "text" in captured else None}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w15_clistream_56f8e8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
