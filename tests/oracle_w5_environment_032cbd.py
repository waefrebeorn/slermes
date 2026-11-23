#!/usr/bin/env python3
"""Oracle for upstream Hermes pm/environment.py (w5_environment_032cbd gate).

Compares the pure failure-classification helpers against the live module.
pm.progress is stubbed (it would drag in IO-bound progress sinks) and
pm.build_compilers.missing_compiler_remedy is injected from the fixture so the
build-failure branch is deterministic.
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

prog = types.ModuleType("pm.progress")
prog.LiveTail = object
prog.TextSink = object
prog.verbose_output = lambda: False
sys.modules["pm.progress"] = prog

spec = importlib.util.spec_from_file_location("w5_environment_live", Q + "/pm/environment.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w5_environment_live"] = live
spec.loader.exec_module(live)

holder = {"remedy": ""}
bc = types.ModuleType("pm.build_compilers")
bc.missing_compiler_remedy = lambda output: holder["remedy"]
sys.modules["pm.build_compilers"] = bc

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
    rc = int(r.get("returncode", 0))
    try:
        if func == "bytecode":
            result = types.SimpleNamespace(returncode=rc,
                                           stderr=r.get("stderr", ""),
                                           stdout=r.get("stdout", ""))
            py = live._is_bytecode_compile_failure(result)
        elif func == "classify":
            holder["remedy"] = r.get("build_remedy", "")
            e = live.classify_uv_failure(r.get("stage", ""), rc, r.get("output", ""))
            py = {"kind": type(e).__name__, "package": e.package,
                  "cause": e.cause, "remedy": e.remedy}
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

print(f"ORACLE w5_environment_032cbd: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
