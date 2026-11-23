#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/subcommands/checkpoints.py."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/subcommands/checkpoints.py"
ARGKEYS = ("nargs", "default", "choices", "help")
def norm(v):
    if v is None or isinstance(v, (str, int, float, bool)): return v
    if isinstance(v, (list, tuple)): return [norm(x) for x in v]
    if callable(v): return "<callable>"
    return str(v)
def arg_shape(args, kwargs):
    d = {"name": args[0] if args else None}
    for k in ARGKEYS: d[k] = norm(kwargs[k]) if k in kwargs else None
    return d
def parser_shape(name, kwargs):
    aliases = norm(kwargs.get("aliases", []))
    return {"name": name, "help": norm(kwargs.get("help")),
            "description": norm(kwargs.get("description")),
            "aliases": aliases if aliases is not None else [],
            "arguments": [], "subparsers": None, "defaults": []}
class RecParser:
    def __init__(self, name, kwargs): self.shape = parser_shape(name, kwargs)
    def add_argument(self, *args, **kwargs): self.shape["arguments"].append(arg_shape(args, kwargs))
    def add_subparsers(self, **kwargs):
        sp = {"dest": norm(kwargs.get("dest")), "parsers": []}
        self.shape["subparsers"] = sp
        return RecSubparsers(sp)
    def set_defaults(self, **kwargs): self.shape["defaults"] = sorted(kwargs.keys())
class RecSubparsers:
    def __init__(self, sp): self.sp = sp
    def add_parser(self, name, **kwargs):
        p = RecParser(name, kwargs); self.sp["parsers"].append(p.shape); return p
class RecRoot:
    def __init__(self): self.parsers = []
    def add_parser(self, name, **kwargs):
        p = RecParser(name, kwargs); self.parsers.append(p.shape); return p
def fake(fullname, **attrs):
    m = types.ModuleType(fullname)
    for k, v in attrs.items(): setattr(m, k, v)
    return m

def live_shape():
    sys.modules["hermes_cli.checkpoints"] = fake("hermes_cli.checkpoints",
        register_cli=lambda *a, **k: None)
    root = RecRoot()
    spec = importlib.util.spec_from_file_location("scheckpoints_live", SRC)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    m.build_checkpoints_parser(root)
    return root.parsers[0]

shape = live_shape()
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    if func == "shape": py = shape
    elif func == "keys": py = sorted(shape.keys())
    elif func == "missing": py = None
    elif isinstance(func, str) and func in shape: py = shape[func]
    else: continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE clicheckpoints: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
