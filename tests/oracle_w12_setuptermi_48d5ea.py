#!/usr/bin/env python3
"""Oracle for the w12_setuptermi_48d5ea port of hermes_cli/setup_terminal.py.

Loads the LIVE module from the quarry (sys.path insert + parent-package
bootstrap + sys.modules registration).  Each case swaps in a recording
``_setup`` namespace so the wizard's injected interactions can be compared.
"""
import importlib
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
importlib.import_module("hermes_cli")            # parent-package bootstrap
_SPEC = importlib.util.spec_from_file_location(
    "_w12_setuptermi_live", QUARRY + "/hermes_cli/setup_terminal.py")
live = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = live                    # sys.modules registration
_SPEC.loader.exec_module(live)


def want(r):
    fn = r.get("func")
    if fn == "report_binary":
        rec = []
        live._setup = types.SimpleNamespace(
            print_info=lambda msg: rec.append({"kind": "info", "text": msg}),
            print_warning=lambda msg: rec.append({"kind": "warning", "text": msg}),
        )
        live._report_binary(r.get("found"), r.get("missing"),
                            r.get("install_hint"), r.get("found_prefix", "Found: "))
        return rec
    if fn == "existing_secret_keeps":
        rec = []
        env = r.get("env_value")
        ans = bool(r.get("prompt_answer", False))
        live._setup = types.SimpleNamespace(
            get_env_value=lambda ev: env,
            print_info=lambda msg: rec.append(msg),
            prompt_yes_no=lambda q, d: ans,
        )
        keeps = live._existing_secret_keeps(r.get("env_var", "VAR"),
                                            r.get("label", "Label"),
                                            r.get("question", "Q"))
        return {"keeps": bool(keeps), "info": rec[0] if rec else None}
    if fn == "prompt_secret_env":
        rec = []
        value = r.get("value")
        live._setup = types.SimpleNamespace(
            prompt=lambda label, password=False: value,
            save_env_value=lambda ev, v: rec.append(
                {"call": "save_env_value", "env_var": ev, "value": v}),
            print_success=lambda msg: rec.append(
                {"call": "print_success", "text": msg}),
        )
        live._prompt_secret_env(r.get("label", "Label"),
                                r.get("env_var", "VAR"),
                                confirm_msg=r.get("confirm_msg", ""))
        return rec
    return None


def main():
    n = mism = 0
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
        n += 1
        got = r.get("out")
        try:
            expected = want(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case %d: live raised %r" % (r.get("func"), n, exc))
            continue
        if got != expected:
            mism += 1
            if mism <= 12:
                print("MISMATCH %s case %d C=%r PY=%r" % (r.get("func"), n, got, expected))
    print("ORACLE w12_setuptermi_48d5ea: %d cases, %d mismatches" % (n, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
