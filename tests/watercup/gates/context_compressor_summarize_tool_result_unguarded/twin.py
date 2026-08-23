"""twin for context_compressor_summarize_tool_result_unguarded.

Traces LIVE agent/context_compressor.py::_summarize_tool_result_unguarded
with the same rung ladder as the C harness: B1 terminal, B2 read_file,
B3 write_file, B4 search_files, else fallback; V = the summary line.
"""
import importlib.util as ilu
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

spec = ilu.spec_from_file_location(
    "wc_live_ctx_comp", os.path.join(DEV, "agent", "context_compressor.py"))
_cc = ilu.module_from_spec(spec)
sys.modules["wc_live_ctx_comp"] = _cc
spec.loader.exec_module(_cc)

_fn = _cc._summarize_tool_result_unguarded


def trace(item):
    name, rest = item.split("\t", 1)
    args_s, content = rest.split("\t", 1)
    c = Cup("summarize_tool_result_unguarded")
    c.branch(name == "terminal")                       # B1
    c.branch(name == "read_file")                      # B2
    c.branch(name == "write_file")                     # B3
    c.branch(name == "search_files")                   # B4
    out = _fn(name, "" if args_s == "-" else args_s, content)
    c.valstr(out)                                      # V
    return c.finalize()


def run(inputs):
    return [trace(m) for m in inputs]
