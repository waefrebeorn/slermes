#!/usr/bin/env python3
"""Differential oracle for w17_streamcons_bd185c (gateway/stream_consumer_think.py).

Loads the LIVE agent/think_scrubber.py for the real tag tuples and orphan-close
regex, AST-extracts the LIVE mixin helpers into a registered namespace, and
drives them with a scripted self. sys.modules registration + parent-package
bootstrap make the quarry import work in this environment.
"""
import ast, builtins, copy, importlib.util, json, sys, types, __future__
try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/gateway/stream_consumer_think.py"
TSRC = QUARRY + "/agent/think_scrubber.py"
NAME = "w17_streamcons_bd185c"
ANN = __future__.annotations.compiler_flag

# Parent-package bootstrap for the quarry 'agent' package.
from pathlib import Path
agent_pkg = types.ModuleType("agent"); agent_pkg.__path__ = [str(Path(QUARRY) / "agent")]
sys.modules["agent"] = agent_pkg

spec = importlib.util.spec_from_file_location("w17_live_think_scrubber", TSRC)
TS = importlib.util.module_from_spec(spec); sys.modules["w17_live_think_scrubber"] = TS
spec.loader.exec_module(TS)

TREE = ast.parse(open(SRC, encoding="utf-8").read())
MOD = types.ModuleType("w17_live_streamthink"); sys.modules["w17_live_streamthink"] = MOD
NS = MOD.__dict__
NS["__builtins__"] = builtins
NS["THINK_OPEN_TAGS"] = TS.THINK_OPEN_TAGS
NS["THINK_CLOSE_TAGS"] = TS.THINK_CLOSE_TAGS
NS["_Scrubber"] = TS.StreamingThinkScrubber
WANTED = {"_at_block_boundary", "_earliest_open_tag", "_strip_orphan_close_tags"}
for node in ast.walk(TREE):
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        n2 = copy.deepcopy(node)
        n2.decorator_list = []      # extract the raw function, not a staticmethod object
        exec(compile(ast.Module(body=[n2], type_ignores=[]), SRC, "exec",
                     flags=ANN, dont_inherit=True), NS)
for name in WANTED:
    if name not in NS or not callable(NS[name]):
        raise SystemExit("ORACLE %s: missing live %s" % (NAME, name))


def mk_self(r):
    s = types.SimpleNamespace(_accumulated=r.get("accumulated", ""),
                              _OPEN_THINK_TAGS=TS.THINK_OPEN_TAGS)
    s._at_block_boundary = lambda b, i: NS["_at_block_boundary"](s, b, i)
    return s


def compute(r):
    f = r.get("func")
    if f == "at_block_boundary":
        return bool(NS["_at_block_boundary"](mk_self(r), r.get("buf", ""), r.get("idx", 0)))
    if f == "earliest_open_tag":
        b = r.get("buf", "")
        return list(NS["_earliest_open_tag"](mk_self(r), b, b.lower()))
    if f == "strip_orphan_close_tags":
        return NS["_strip_orphan_close_tags"](r.get("text", ""))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except BaseException as ex:
        py = {"__raise__": type(ex).__name__}
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE %s: %d cases, %d mismatches" % (NAME, cases, mism))
sys.exit(1 if (mism or cases == 0) else 0)
