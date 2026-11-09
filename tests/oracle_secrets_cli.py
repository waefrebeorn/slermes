#!/usr/bin/env python3
"""Oracle for port_secrets_cli.c -- drives the LIVE secrets_cli.py /
_secrets_common.py defs.

The harness echoes its inputs from C. The live functions are ast-extracted
because importing hermes_cli.secrets_cli pulls in rich / subprocess.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\secrets_cli.py"
COMMON = r"C:\Users\eman5\hermes-agent\hermes_cli\_secrets_common.py"


def extract():
    ns = {}
    # section_cfg lives in _secrets_common
    try:
        csrc = open(COMMON, encoding="utf-8").read()
        for node in ast.parse(csrc).body:
            if isinstance(node, ast.FunctionDef) and node.name == "section_cfg":
                exec(compile(ast.Module(body=[node], type_ignores=[]), COMMON, "exec"), ns)
    except Exception:
        pass
    if "section_cfg" not in ns:
        ns["section_cfg"] = lambda cfg, section: (cfg.get("secrets") or {}).get(section) or {}

    src = open(SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in ("_bw_cfg", "_fetch_status",
                                                              "_missing_noninteractive_flags"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        cases += 1

        if fn == "section_cfg":
            label = c.get("case")
            cfgs = {
                "present": {"secrets": {"bitwarden": {"token": "x"}}},
                "empty-object": {"secrets": {}},
                "null-secrets": {"secrets": None},
                "no-secrets": {},
                "section-null": {"secrets": {"bitwarden": None}},
            }
            want = ns["section_cfg"](cfgs.get(label, {}), "bitwarden")
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH section_cfg case={label} C={got!r} PY={want!r}")

        elif fn == "_fetch_status":
            label = c.get("case")
            # rebuild the env the harness had set for this case
            env = {}
            if label == "already-set":
                env["BWS_ACCESS_TOKEN"] = "tok"
            import os
            ns.setdefault("os", os)
            key = c["key"]
            token_env = c.get("token_env") or "BWS_ACCESS_TOKEN"
            # _fetch_status reads os.environ directly; patch it for the replay
            old = dict(os.environ)
            os.environ.pop("BWS_ACCESS_TOKEN", None)
            os.environ.update(env)
            try:
                want = ns["_fetch_status"](key, token_env)
            finally:
                os.environ.clear(); os.environ.update(old)
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _fetch_status case={label} C={got!r} PY={want!r}")

        elif fn == "_missing_noninteractive_flags":
            label = c.get("case")
            import os
            ns.setdefault("os", os)
            cases_map = {
                "all-set": ("tok", "https://x", "proj", None),
                "none": (None, None, None, None),
                "blank-token": ("   ", "https://x", "proj", None),
                "env-server-url": ("tok", "", "proj", "https://env"),
                "env-fallback-empty": ("tok", "  ", "proj", ""),
                "only-token": ("tok", None, None, None),
            }
            tok, url, proj, env_url = cases_map.get(label, (None, None, None, None))
            old = os.environ.get("BWS_SERVER_URL")
            if env_url is None:
                os.environ.pop("BWS_SERVER_URL", None)
            else:
                os.environ["BWS_SERVER_URL"] = env_url

            class Args:
                access_token = tok
                server_url = url
                project_id = proj
            try:
                want = ns["_missing_noninteractive_flags"](Args)
            finally:
                if old is None: os.environ.pop("BWS_SERVER_URL", None)
                else: os.environ["BWS_SERVER_URL"] = old
            got = c.get("out") or []
            if got != want:
                mism += 1
                print(f"MISMATCH _missing_noninteractive_flags case={label} C={got!r} PY={want!r}")

    print(f"ORACLE secrets_cli: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
