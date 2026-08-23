"""twin for tool_dispatch_helpers_extract_file_mutation_targets.

Imports LIVE agent/tool_dispatch_helpers.py::_extract_file_mutation_targets and
traces the SAME tool/mode/patch-header ladder as the C port. Input encoding:
tool\\x1fmode\\x1fpath\\x1fpatch ('-' = absent). Final targets come from LIVE.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_dispatch", os.path.join(DEV, "agent", "tool_dispatch_helpers.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_dispatch"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def dec(s):
    return None if s == "-" else s


def run(inputs):
    cups = []
    for raw in inputs:
        tool, mode, path, patch = (dec(x) for x in raw.split("\x1f"))
        args = {}
        if tool == "write_file":
            if path is not None:
                args["path"] = path
        else:
            args["mode"] = mode if mode is not None else "replace"
            if args["mode"] == "replace":
                if path is not None:
                    args["path"] = path
            elif args["mode"] == "patch":
                if patch is not None:
                    args["patch"] = patch
        c = Cup("extract_file_mutation_targets")
        out = []
        mutating = tool in ("write_file", "patch")
        if not mutating:
            c.branch(True)
        else:
            c.branch(False)
            if tool == "write_file":
                c.branch(True)
                c.branch(bool(args.get("path")))
                if args.get("path"):
                    out.append(args["path"])
            else:
                c.branch(False)
                m = args.get("mode") or "replace"
                if m == "replace":
                    c.branch(True)
                    c.branch(bool(args.get("path")))
                    if args.get("path"):
                        out.append(args["path"])
                elif m == "patch":
                    c.branch(True)
                    body = args.get("patch")
                    c.branch(bool(body))
                    if body:
                            c.loop()
                            for line in body.splitlines():
                                ls = line.lstrip()
                                if not ls.startswith("***"):
                                    continue
                                rest = ls[3:].lstrip(" \t")
                                for kw in ("Update", "Add", "Delete"):
                                    if rest.startswith(kw):
                                        after = rest[len(kw):].lstrip(" \t")
                                        if after.startswith("File:"):
                                            p = after[5:].strip(" \t")
                                            c.branch(bool(p))
                                            if p:
                                                out.append(p)
                                        break
                            for line in body.splitlines():
                                ls = line.lstrip()
                                if not ls.startswith("***"):
                                    continue
                                rest = ls[3:].lstrip(" \t")
                                if rest.startswith("Move"):
                                    after = rest[4:].lstrip(" \t")
                                    if after.startswith("File:"):
                                        after = after[5:].lstrip(" \t")
                                    if "->" in after:
                                        src, dst = after.split("->", 1)
                                        src = src.rstrip(" \t")
                                        dst = dst.strip(" \t")
                                        c.branch(bool(src))
                                        if src:
                                            out.append(src)
                                        c.branch(bool(dst))
                                        if dst:
                                            out.append(dst)
        live = mod._extract_file_mutation_targets(tool, args)
        assert out == live, f"LIVE/ladder diverge on {raw!r}: {live} vs {out}"
        c.valstr("|".join(out))
        cups.append(c.finalize())
    return cups
