"""twin for transcription_tools_shell_quote_context_stt.

Imports the LIVE tools/transcription_tools.py::_shell_quote_context_stt and
traces the SAME branch ladder as the C harness. Mirrors the tts gate twin.
Input encoding: "<template>|<position>" (split on LAST '|').
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_transcription_tools", os.path.join(DEV, "tools", "transcription_tools.py"))
_tsc = ilu.module_from_spec(_spec)
sys.modules["wc_live_transcription_tools"] = _tsc
_spec.loader.exec_module(_tsc)


def trace(command, position):
    c = Cup("shell_quote_context_stt")
    if command is None or position < 0:            # B1
        c.branch(True)
        result = ""
    else:
        c.branch(False)
        quote = None
        escaped = False
        i = 0
        while i < position and i < len(command):
            c.loop()
            char = command[i]
            if quote == "'":                       # B2
                c.branch(True)
                c.branch(char == "'")
                if char == "'":
                    quote = None
            elif quote == '"':                     # B3
                c.branch(False)
                c.branch(True)
                if escaped:
                    c.branch(True); c.branch(False); c.branch(False)
                    escaped = False
                else:
                    c.branch(False)
                    c.branch(char == "\\"); c.branch(char == '"'); c.branch(not (char == "\\" or char == '"'))
                    if char == "\\":
                        escaped = True
                    elif char == '"':
                        quote = None
            elif char == "'":                      # B4
                c.branch(False)
                c.branch(True); c.branch(False); c.branch(False)
                quote = "'"
            elif char == '"':                      # B5
                c.branch(False)
                c.branch(False); c.branch(True); c.branch(False)
                quote = '"'
            elif char == "\\":                     # B6
                c.branch(False)
                c.branch(False); c.branch(False); c.branch(True)
                i += 1
            else:
                c.branch(False)
                c.branch(False); c.branch(False); c.branch(False)
            i += 1
        result = quote or ""
    c.valstr(result if result else " ")
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        tpl, _, pos = raw.rpartition("|")
        cups.append(trace(tpl, int(pos)))
    return cups
