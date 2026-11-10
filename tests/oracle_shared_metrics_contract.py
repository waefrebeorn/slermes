#!/usr/bin/env python3
"""Oracle for port_shared_metrics_contract.c -- drives the LIVE
hermes_cli/observability/shared_metrics_contract.py defs.

The harness passes a type tag (`kind`) plus the value so the oracle can
rebuild the Python value with the same type - bool vs int matters because
_non_negative_number rejects bool before the int check.
"""
import ast
import json
import math
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\observability\shared_metrics_contract.py"
WANTED = ["long_size_bucket", "update_duration_bucket", "version_age_bucket",
          "desktop_active_minutes_bucket", "duration_bucket", "count_bucket",
          "tool_latency_bucket", "tool_retry_bucket", "size_bucket",
          "client_os_family", "client_architecture", "_bucket",
          "_non_negative_number", "_norm"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"math": math, "Any": object, "isfinite": math.isfinite,
          "isinstance": isinstance, "float": float}
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def pyval(kind, ival, dval):
    if kind == 0:
        return "not-a-number"
    if kind == 1:
        return ival
    if kind == 2:
        return dval
    if kind == 3:
        return bool(ival)
    return None


def main():
    ns = extract()
    cases = 0
    mism = 0

    buckets = {
        "0": 0, "1": 1, "2": 2, "3_to_5": 3, "6_to_10": 6, "11_to_25": 11,
        "26_to_100": 26, "101_to_250": 101, "251_to_1000": 251,
        "gte_1001": 1001, "huge": 999999,
    }
    update = {
        "lt_30s": (1, 0, 0.0), "lt_30s-2": (1, 29999, 0.0),
        "30s_to_2m": (1, 30000, 0.0), "5m_to_15m": (1, 300000, 0.0),
        "gte_15m": (1, 900000, 0.0), "float": (2, 0, 45000.0),
        "notnum": (0, 0, 0.0), "bool": (3, 1, 0.0), "negative": (1, -5, 0.0),
    }
    version = {
        "lt_7d": (2, 0, 1000.0), "7d_to_30d": (2, 0, 7 * 86400000.0),
        "gte_90d": (2, 0, 90 * 86400000.0), "unknown": (0, 0, 0.0),
    }
    desktop = {
        "zero": (1, 0, 0.0), "notnum-zero": (0, 0, 0.0), "lt_5m": (1, 1, 0.0),
        "5m_to_30m": (1, 300000, 0.0), "2h_to_6h": (1, 7200000, 0.0),
        "gte_6h": (1, 21600000, 0.0),
    }
    duration = {"lt_1s": 0, "1s_to_5s": 1000, "2m_to_10m": 120000,
                "gte_10m": 600000, "negative": -1}
    count = {"0": 0, "1": 1, "3_to_5": 3, "6_to_10": 6, "gte_11": 11}
    size = {"0": 0, "101_to_250": 101, "gte_251": 251}
    latency = {
        "lt_100ms": ((1, 0, 0.0), (0, 0, 0.0)),
        "100ms_to_250ms": ((1, 100, 0.0), (0, 0, 0.0)),
        "10s_to_30s": ((1, 10000, 0.0), (0, 0, 0.0)),
        "gte_30s": ((1, 30000, 0.0), (0, 0, 0.0)),
        "unknown": ((0, 0, 0.0), (0, 0, 0.0)),
        "fallback": ((0, 0, 0.0), (1, 50, 0.0)),
    }
    retry = {"0": (1, 0), "3_to_5": (1, 3), "bool": (3, 1),
             "negative": (1, -1), "notnum": (0, 0)}
    osf = {"darwin": "Darwin", "linux": "Linux", "macos": "macOS",
           "windows": "Windows", "unknown": "FreeBSD", "null": None,
           "padded": "  LINUX  "}
    arch = {"amd64": "AMD64", "x64": "x64", "x86_64": "x86-64",
            "aarch64": "aarch64", "arm64": "arm64", "i386": "i386",
            "armv7": "armv7l", "unknown": "sparc", "null": None}

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        if fn.startswith("smc_"):
            fn = fn[4:]
        label = c.get("case")
        cases += 1
        got = c.get("out")
        want = None

        try:
            if fn == "long_size_bucket":
                want = ns["long_size_bucket"](buckets[label])
            elif fn == "update_duration_bucket":
                k, i, d = update[label]
                want = ns["update_duration_bucket"](pyval(k, i, d))
            elif fn == "version_age_bucket":
                k, i, d = version[label]
                want = ns["version_age_bucket"](pyval(k, i, d))
            elif fn == "desktop_active_minutes_bucket":
                k, i, d = desktop[label]
                want = ns["desktop_active_minutes_bucket"](pyval(k, i, d))
            elif fn == "duration_bucket":
                want = ns["duration_bucket"](duration[label])
            elif fn == "count_bucket":
                want = ns["count_bucket"](count[label])
            elif fn == "size_bucket":
                want = ns["size_bucket"](size[label])
            elif fn == "tool_latency_bucket":
                (k1, i1, d1), (k2, i2, d2) = latency[label]
                want = ns["tool_latency_bucket"](pyval(k1, i1, d1),
                                                 fallback_duration_ms=pyval(k2, i2, d2))
            elif fn == "tool_retry_bucket":
                k, i = retry[label]
                want = ns["tool_retry_bucket"](pyval(k, i, 0.0))
            elif fn == "client_os_family":
                want = ns["client_os_family"](osf[label])
            elif fn == "client_architecture":
                want = ns["client_architecture"](arch[label])
        except Exception as e:
            want = f"<raised {type(e).__name__}: {e}>"

        if got != want:
            mism += 1
            print(f"MISMATCH {fn} case={label} C={got!r} PY={want!r}")

    print(f"ORACLE shared_metrics_contract: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
