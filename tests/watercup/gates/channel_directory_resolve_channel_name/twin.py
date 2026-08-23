"""twin for channel_directory_resolve_channel_name.

Imports the LIVE Python gateway/channel_directory.py::resolve_channel_name,
points its DIRECTORY_PATH at the same deterministic fixture the C harness
writes, and traces the SAME coarse rung ladder (B1 resolved / V id).
"""
import os
import sys
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_channel_directory", os.path.join(DEV, "gateway", "channel_directory.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_channel_directory"] = mod
spec.loader.exec_module(mod)

FIXTURE = Path("/tmp/wc_channel_dir_fixture/channel_directory.json")
FIXTURE.parent.mkdir(parents=True, exist_ok=True)
if not FIXTURE.exists():
    FIXTURE.write_text(
        '{"updated_at":null,"platforms":{'
        '"discord":['
        '{"id":"123","name":"bot-home","guild":"Wubu HQ"},'
        '{"id":"456","name":"general","guild":"Wubu HQ"},'
        '{"id":"789","name":"projects","guild":"Side Guild"},'
        '{"id":"999","name":"general-announcements","guild":"Side Guild"}],'
        '"telegram":['
        '{"id":"-100200","name":"Ops Room","type":"group"},'
        '{"id":"-100300","name":"alerts","type":"channel"}],'
        '"slack":['
        '{"id":"C0B0QV5434G","name":"engineering"},'
        '{"id":"C111","name":"random"}]}}'
    )
mod.DIRECTORY_PATH = FIXTURE

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        platform, _, query = raw.partition("|")
        c = Cup("resolve_channel_name")
        resolved = mod.resolve_channel_name(platform, query)
        c.branch(resolved is not None)                  # B1 resolved
        c.valstr(resolved or "")
        cups.append(c.finalize())
    return cups
