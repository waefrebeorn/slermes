#!/usr/bin/env python3
"""Oracle for the pure core of gateway/platform_activation.py (live module).

The file/env/pool scanners are monkeypatched so the real activation_predicate
closure runs on the same evidence sets the C port receives.
"""
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import gateway.platform_activation as PA  # noqa: E402


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE platform_activation: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "yamlblocks":
            yaml = c["in"]  # already decoded
            want = set(PA._yaml_block_names(yaml))
            got = set(c["out"])
            if got != want:
                mism += 1
                print(f"MISMATCH {c['case']}: C={sorted(got)} PY={sorted(want)}")
        elif fam == "matches":
            want = PA._matches(c["name"], c["cands"])
            if bool(c["ret"]) != bool(want):
                mism += 1
                print(f"MISMATCH {c['case']}: C={c['ret']} PY={want} "
                      f"name={c['name']!r} cands={c['cands']}")
        elif fam == "activation":
            blocks = set(c["blocks"])
            pools = set(c["pools"])
            upper_env = set(c["env"])
            declared = list(c["decl"])

            def fake_yaml_block_names(cfg):
                return blocks

            def fake_env_names(home):
                return upper_env  # already uppercase; .upper() is identity

            def fake_pool_names(home):
                return pools

            def fake_legacy(home):
                return set()

            PA._yaml_block_names = fake_yaml_block_names
            PA._env_names = fake_env_names
            PA._credential_pool_names = fake_pool_names
            PA._legacy_gateway_json_names = fake_legacy
            reg = SimpleNamespace(activation_env=lambda n: declared)
            wp = PA.activation_predicate(yaml_cfg={}, platform_names=[],
                                         home=Path("C:/Users/eman5/slermes/.testbin"),
                                         registry=reg)
            want = bool(wp(c["name"]))
            if bool(c["ret"]) != want:
                mism += 1
                print(f"MISMATCH {c['case']}: C={c['ret']} PY={want} name={c['name']!r}")

    print(f"ORACLE platform_activation: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
