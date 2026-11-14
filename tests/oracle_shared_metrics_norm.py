#!/usr/bin/env python3
"""Oracle for the normalisation half of
hermes_cli/observability/shared_metrics_contract.py.

The harness (t_port_shared_metrics_norm.c) emits one JSON line per case with a
"fn" discriminator. String results come back as {"str": ...} so an empty string
stays distinguishable from a missing value; client_resource comes back as a raw
JSON object.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from hermes_cli.observability import shared_metrics_contract as smc   # noqa: E402

SETS = {
    "install_methods": smc.CLIENT_INSTALL_METHODS,
    "skill_provenances": smc.SKILL_PROVENANCES,
    "tool_categories": smc.TOOL_CATEGORIES,
    "model_error_classes": smc.MODEL_ERROR_CLASSES,
}


def main() -> int:
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        try:
            if fn == "allowlisted":
                want = smc._allowlisted(i.get("normalized"), SETS[i["set"]])
            elif fn == "client_install_method":
                want = smc.client_install_method(i.get("value"))
            elif fn == "skill_provenance":
                want = smc.skill_provenance(i.get("value"))
            elif fn == "metric_identifier":
                want = smc._metric_identifier(i.get("value"),
                                              max_length=i["max_length"])
            elif fn == "client_resource":
                want = smc.client_resource(
                    i.get("hermes_version"), os_name=i.get("os_name"),
                    architecture=i.get("architecture"),
                    install_method=i.get("install_method"))
            elif fn == "client_resource_is_valid":
                want = smc.client_resource_is_valid(i.get("resource"))
            elif fn == "tool_category":
                want = smc.tool_category(i.get("kwargs") or {})
            elif fn == "tool_outcome":
                want = smc.tool_outcome(i.get("kwargs") or {})
            elif fn == "model_error_class":
                want = smc.model_error_class(i.get("kwargs") or {})
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        raw = c["out"]
        got = raw["str"] if isinstance(raw, dict) and set(raw) == {"str"} else raw
        if got != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:300]} "
                  f"PY={want!r}")
    if total == 0:
        print("ORACLE shared_metrics_norm: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE shared_metrics_norm: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
