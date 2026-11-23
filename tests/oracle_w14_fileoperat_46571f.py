#!/usr/bin/env python3
"""Differential oracle for tools/file_operations_lint.py (w14_fileoperat_46571f)."""
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
    return m


class LintResult:
    def __init__(self, success=True, skipped=False, output="", message=""):
        self.success = success
        self.skipped = skipped
        self.output = output
        self.message = message


class ExecuteResult:
    def __init__(self, stdout="", exit_code=0):
        self.stdout = stdout
        self.exit_code = exit_code


tools = _mod("tools")
tools.__path__ = [Q + "/tools"]
_mod("tools.file_operations_common", LintResult=LintResult, ExecuteResult=ExecuteResult)

SRC = Q + "/tools/file_operations_lint.py"
spec = importlib.util.spec_from_file_location("tools.file_operations_lint_w14_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class _Dummy:
    def __init__(self, pre, post):
        self.pre = pre
        self.post = post

    def _check_lint(self, path, content=None):
        return self.pre if content == "PRE" else self.post


def as_lr(d):
    return LintResult(success=bool(d.get("success", True)),
                      skipped=bool(d.get("skipped", False)),
                      output=d.get("output", ""), message=d.get("message", ""))


def lr_dict(lr):
    return {"success": lr.success, "skipped": lr.skipped,
            "output": lr.output, "message": lr.message}


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
        if func == "looks_like_linter_unusable":
            py = live._looks_like_linter_unusable(r.get("base_cmd", ""), r.get("output", ""))
        elif func == "check_lint_delta":
            pre = r.get("pre")
            dummy = _Dummy(as_lr(pre) if pre is not None else None, as_lr(r.get("post")))
            lr = live.LintMixin._check_lint_delta(
                dummy, "fake.py", "PRE" if pre is not None else None, "POST")
            py = lr_dict(lr)
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

print(f"ORACLE w14_fileoperat_46571f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
