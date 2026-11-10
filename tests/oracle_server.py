#!/usr/bin/env python3
"""Oracle for port_server.c -- drives the LIVE tui_gateway/server.py defs. The
dispatch / db / profile-scope / transport half is not imported; the pure defs
are ast-extracted with the externals they close over passed as parameters.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SERVER = r"C:\Users\eman5\hermes-agent\tui_gateway\server.py"
WANTED = ["_load_busy_input_mode", "_canonical_profile_request",
          "_current_profile_name", "_event_frame"]


def extract():
    ns = {"dict": dict, "str": str, "int": int, "bool": bool, "Any": object,
          "Optional": object, "frozenset": frozenset}
    src = open(SERVER, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SERVER, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SERVER, "exec"), ns)
    # the externals the pure defs close over. _current_profile_name imports
    # get_active_profile_name from hermes_cli.profiles, so stub the resolver.
    ns["_CFG"] = {}
    ns["_ACTIVE"] = None
    ns["_display_cfg"] = lambda: dict(ns["_CFG"])
    import contextlib, types
    # _current_profile_name imports INSIDE the function body, so the module
    # must exist in sys.modules or the import raises and contextlib.suppress
    # turns it into "default".
    prof = types.ModuleType("hermes_cli.profiles")
    prof.get_active_profile_name = lambda: ns["_ACTIVE"]
    ns["get_active_profile_name"] = prof.get_active_profile_name
    pkg = types.ModuleType("hermes_cli")
    pkg.profiles = prof
    sys.modules.setdefault("hermes_cli", pkg)
    sys.modules["hermes_cli.profiles"] = prof
    ns["contextlib"] = contextlib
    # _event_frame calls _contracts.check_payload(event, payload); the C port
    # does not model the contract check, so a no-op stand-in is right.
    contracts = types.SimpleNamespace(check_payload=lambda event, payload: None)
    ns["_contracts"] = contracts
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    modes = {
        "queue": "queue", "steer": "STEER", "interrupt": "interrupt",
        "bogus": "bogus", "padded": "  Queue  ", "empty": "", "null": None,
    }
    canonical = {
        "hermes-dir": ("hermes", True), "hermes-nodir": ("hermes", False),
        "HERMES": ("HERMES", False), "dot-hermes": (".hermes", False),
        "other": ("work", False), "null": (None, False),
    }
    current = {"set": "work", "empty": "", "null": None}
    frames = {
        "no-payload": ("delta", "sess-1", None),
        "with-payload": ("delta", "sess-1", "{\"a\":1}"),
        "empty-payload": ("done", "s2", ""),
        "escapes": ("q\"uote", "s3", "v\\x"),
    }

    def as_json(o):
        return json.dumps(o, ensure_ascii=False, separators=(",", ":"))

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        label = c.get("case")
        cases += 1

        try:
            if fn == "_load_busy_input_mode":
                raw = modes[label]
                ns["_CFG"] = ({"busy_input_mode": raw} if raw is not None else {})
                want = ns["_load_busy_input_mode"]()
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _load_busy_input_mode case={label} C={got!r} PY={want!r}")

            elif fn == "_canonical_profile_request":
                name, has_dir = canonical[label]
                # the live fn probes the real profiles root, which is not what
                # the port models; the contract is: hermes-ish names canonicalize
                # to "default" unless a real profile dir of that name exists.
                if name is None:
                    want = "default"
                elif name.casefold() in {".hermes", "hermes"}:
                    want = name if has_dir else "default"
                else:
                    want = ns["_canonical_profile_request"](name)
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _canonical_profile_request case={label} C={got!r} PY={want!r}")

            elif fn == "_current_profile_name":
                ns["_ACTIVE"] = current[label]
                want = ns["_current_profile_name"]()
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _current_profile_name case={label} C={got!r} PY={want!r}")

            elif fn == "_event_frame":
                ev, sid, payload = frames[label]
                want = as_json(ns["_event_frame"](ev, sid, payload))
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _event_frame case={label}\n  C={got!r}\n  PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE server: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
