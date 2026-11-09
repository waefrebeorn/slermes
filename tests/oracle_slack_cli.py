#!/usr/bin/env python3
"""Oracle for port_slack_cli.c -- drives the LIVE slack_cli.py:_build_full_manifest.

The live function is ast-extracted. Its `slack_app_manifest()` import is inside
the function body and needs the command registry, so the oracle stubs it with a
`features.slash_commands` list matching the harness's flat triple list - the
part the C port implements.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\slack_cli.py"

# mirrors tests/t_port_slack_cli.c's SLASHES
SLASHES = [
    ("hermes", "Control the Hermes agent", ""),
    ("status", "Show the current status", "[section]"),
    ("model", "Switch the active model", "<name>"),
]


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name == "_build_full_manifest":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def stub_manifest(request_url="https://hermes-agent.local/slack/commands"):
    return {"features": {"slash_commands": [
        {"command": "/" + n, "description": d, "should_escape": False, "url": request_url,
         **({"usage_hint": u} if u else {})}
        for n, d, u in SLASHES
    ]}}


def main():
    ns = extract()
    fn = ns.get("_build_full_manifest")
    if fn is None:
        print("ORACLE slack_cli: _build_full_manifest not found")
        return 2
    # the in-function import cannot resolve here; patch the module it names
    sys.modules.setdefault("hermes_cli.commands_platforms", type("M", (), {
        "slack_app_manifest": staticmethod(stub_manifest)})())

    cases = 0
    mism = 0
    cfgs = {
        "assistant-default": ("Hermes", "Your Hermes agent on Slack", True, None, None),
        "none": ("Hermes", "Your Hermes agent on Slack", False, None, None),
        "agent": ("Hermes", "Your Hermes agent on Slack", True, "agent", None),
        "assistant-upper": ("Hermes", "Your Hermes agent on Slack", True, "  ASSISTANT  ", None),
        "long-desc": ("Hermes", "Short", True, "assistant", "A much longer description."),
        "name-capped": ("This bot name is deliberately much longer than thirty five chars",
                        "Desc", True, "assistant", None),
        "desc-capped": ("Hermes",
                        "This description is deliberately much longer than one hundred and forty "
                        "characters so that the truncation path is actually exercised by this test "
                        "case and not merely asserted.", True, "assistant", None),
        "empty-desc": ("Hermes", "", True, "assistant", None),
        "null-desc": ("Hermes", None, True, "assistant", None),
        "bad-mode": ("Hermes", "Desc", True, "bogus", None),
    }

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        bot, desc, inc, mode, long_desc = cfgs.get(c.get("case"), ("", "", True, None, None))

        try:
            want = fn(bot, desc, include_assistant=inc, messaging_experience=mode,
                      long_description=long_desc)
            want_err = 0
        except ValueError:
            want, want_err = None, 1

        got_err = c.get("err", 0)
        got = c.get("out")
        if got_err != want_err:
            mism += 1
            print(f"MISMATCH _build_full_manifest case={c.get('case')} "
                  f"err C={got_err!r} PY={want_err!r}")
            continue
        if want_err:
            continue
        # compare PARSED structures: json.dumps adds ", " / ": " separators that
        # the C serializer does not, so a raw string compare is always a mismatch
        try:
            got_obj = json.loads(got) if got else None
        except json.JSONDecodeError as e:
            mism += 1
            print(f"MISMATCH _build_full_manifest case={c.get('case')} C is not valid JSON: {e}")
            continue
        if got_obj != want:
            mism += 1
            print(f"MISMATCH _build_full_manifest case={c.get('case')}\n"
                  f"  C={json.dumps(got_obj, sort_keys=True)[:200]!r}\n"
                  f"  PY={json.dumps(want, sort_keys=True)[:200]!r}")

    print(f"ORACLE slack_cli: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
