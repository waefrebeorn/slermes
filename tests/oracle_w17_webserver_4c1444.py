#!/usr/bin/env python3
"""Oracle for the C port of hermes_cli/web_server_gateway.py (gate w17_webserver_4c1444).

Loads the LIVE module via importlib.util.spec_from_file_location with the
parent-package bootstrap + sys.modules registration. The TTS sentence regex is
supplied by a registered stub of tools/tts_streaming.py (its one line is
transcribed verbatim from that live module) so the live split helper runs
without importing the whole audio stack. Exits 1 on any mismatch.
"""
import importlib.util
import json
import os
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if not os.path.isfile(os.path.join(Q, "hermes_cli", "web_server_gateway.py")):
    Q = os.path.join(os.path.expanduser("~"), "hermes-agent")
sys.path.insert(0, Q)
SRC = os.path.join(Q, "hermes_cli", "web_server_gateway.py")


def _load_live():
    pkg = "hermes_cli"
    if pkg not in sys.modules:
        pdir = os.path.join(Q, pkg)
        sp = importlib.util.spec_from_file_location(
            pkg, os.path.join(pdir, "__init__.py"), submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp)
        sys.modules[pkg] = pm
        try:
            sp.loader.exec_module(pm)
        except Exception:
            pass
    name = "hermes_cli.web_server_gateway"
    sp = importlib.util.spec_from_file_location(name, SRC)
    live = importlib.util.module_from_spec(sp)
    sys.modules[name] = live
    sp.loader.exec_module(live)
    return live


live = _load_live()

# tools.tts_streaming.SENTENCE_BOUNDARY_RE (verbatim from the live module).
if "tools" not in sys.modules:
    tp = types.ModuleType("tools")
    tp.__path__ = [os.path.join(Q, "tools")]
    sys.modules["tools"] = tp
tts = types.ModuleType("tools.tts_streaming")
tts.SENTENCE_BOUNDARY_RE = re.compile(r"(?<=[.!?])(?:\s|\n)|(?:\n\n)")
sys.modules["tools.tts_streaming"] = tts

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "build":
        py = live._windows_build_number(r.get("version", ""), r.get("platform_label", ""))
    elif func == "display":
        py = live._display_system_platform(
            system=r.get("system", ""), release=r.get("release", ""),
            version=r.get("version", ""), platform_label=r.get("platform_label", ""))
    elif func == "named":
        py = live._named_profile_from_action(r.get("subcommand") or [])
    elif func == "hostspawn":
        py = live._is_host_gateway_spawn(r.get("subcommand") or [])
    elif func == "split":
        py = live._split_text_for_speak_stream(r.get("text", ""), int(r.get("cap", 4000)))
    elif func == "strip":
        py = live._strip_session_list_rows(list(r.get("sessions") or []))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)
print("ORACLE w17_webserver_4c1444: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
