#!/usr/bin/env python3
"""Oracle for port_gemini_native_adapter.c -- drives the LIVE
agent/gemini_native_adapter.py defs. The httpx / asyncio half is not imported;
the pure defs are ast-extracted together with the module constants they read.
"""
import ast
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\agent\gemini_native_adapter.py"
WANTED = ["bare_gemini_model_id", "_gemini_version",
          "gemini_requires_tool_call_ids", "gemini_uses_thinking_level",
          "gemini_sampling_deprecated", "is_vertex_express_base_url",
          "normalize_gemini_base_url", "is_native_gemini_base_url",
          "gemini_accepts_parameters_json_schema", "is_free_tier_quota_error",
          "is_standard_key_auth_error", "_coerce_content_to_text", "_text_of"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re, "json": json, "Any": object, "Optional": object,
          "bool": bool, "str": str, "int": int}
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


def main():
    ns = extract()
    cases = 0
    mism = 0

    bares = {
        "plain": "gemini-3-pro", "google": "google/gemini-3-pro",
        "gemini": "gemini/gemini-3-pro", "upper": "GOOGLE/gemini-3-pro",
        "padded": "  gemini-3-pro  ", "empty": "", "null": None,
        "prefix-only": "google/",
    }
    versions = {
        "v4-argon": "gemini-4-argon", "v3-8": "google/gemini-3.8-flash",
        "v3": "gemini-3-pro", "latest": "gemini-flash-latest",
        "exp": "gemini-exp-1206", "non-gemini": "gpt-4o", "empty": "",
        "v2-5": "gemini-2.5-pro",
    }
    gates = {
        "v3": "gemini-3-pro", "v2": "gemini-2.5-pro", "v4": "gemini-4-argon",
        "other": "gpt-4o", "latest": "gemini-flash-latest",
        "v3-6": "gemini-3.6-pro", "v3-5": "gemini-3.5-pro",
    }
    vertex = {
        "express": "https://aiplatform.googleapis.com/v1beta",
        "oauth": "https://aiplatform.googleapis.com/endpoints/openai",
        "generative": "https://generativelanguage.googleapis.com/v1beta",
        "empty": "",
    }
    native = {
        "generative": "https://generativelanguage.googleapis.com/v1beta",
        "openai": "https://generativelanguage.googleapis.com/openai",
        "express": "https://aiplatform.googleapis.com/v1beta",
        "proxy": "http://localhost:4000/gemini",
    }
    params = {
        "v1beta": "https://generativelanguage.googleapis.com/v1beta",
        "v1": "https://generativelanguage.googleapis.com/v1",
        "v1alpha": "https://generativelanguage.googleapis.com/v1alpha",
        "other": "http://localhost:4000/gemini",
    }
    normals = {
        "default": "", "generative": "https://generativelanguage.googleapis.com",
        "has-v1beta": "https://generativelanguage.googleapis.com/v1beta",
        "openai": "https://generativelanguage.googleapis.com/openai",
        "proxy": "http://localhost:4000/gemini",
        "slashes": "https://generativelanguage.googleapis.com///",
        "express": "https://aiplatform.googleapis.com",
        "express-v": "https://aiplatform.googleapis.com/v1beta1",
        "express-p": "https://aiplatform.googleapis.com/projects/p/locations/l",
        "publishers": "https://aiplatform.googleapis.com/v1beta1/publishers/google",
        "extra-path": "https://generativelanguage.googleapis.com/v1beta/extra",
    }
    free_tier = {
        "hit": "429 free_tier quota exceeded", "upper": "FREE_TIER exhausted",
        "miss": "quota exceeded", "empty": "", "null": None,
    }
    std_key = {
        "v401": (401, "expected OAuth 2 access token", "", ""),
        "v401r": (401, "nope", "ACCESS_TOKEN_TYPE_UNSUPPORTED", ""),
        "v400": (400, "API key not valid", "", "AIzaSyX"),
        "v400-not-aiza": (400, "API key not valid", "", "AQ.foo"),
        "v403": (403, "API key not valid", "", "AIzaSyX"),
    }
    coerces = {
        "two": ["hello", "world"], "one": ["only"], "empty": [],
    }

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
            if fn == "bare_gemini_model_id":
                want = ns["bare_gemini_model_id"](bares[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH bare_gemini_model_id case={label} C={got!r} PY={want!r}")

            elif fn == "_gemini_version":
                want = list(ns["_gemini_version"](versions[label]))
                got = c.get("out") or []
                if got != want:
                    mism += 1
                    print(f"MISMATCH _gemini_version case={label} C={got!r} PY={want!r}")

            elif fn in ("gemini_requires_tool_call_ids", "gemini_uses_thinking_level",
                        "gemini_sampling_deprecated"):
                want = bool(ns[fn](gates[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH {fn} case={label} C={got!r} PY={want!r}")

            elif fn == "is_vertex_express_base_url":
                want = bool(ns[fn](vertex[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH is_vertex_express_base_url case={label} C={got!r} PY={want!r}")

            elif fn == "is_native_gemini_base_url":
                want = bool(ns[fn](native[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH is_native_gemini_base_url case={label} C={got!r} PY={want!r}")

            elif fn == "gemini_accepts_parameters_json_schema":
                want = bool(ns[fn](params[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH gemini_accepts_parameters_json_schema case={label} C={got!r} PY={want!r}")

            elif fn == "normalize_gemini_base_url":
                want = ns[fn](normals[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH normalize_gemini_base_url case={label} C={got!r} PY={want!r}")

            elif fn == "is_free_tier_quota_error":
                want = bool(ns[fn](free_tier[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH is_free_tier_quota_error case={label} C={got!r} PY={want!r}")

            elif fn == "is_standard_key_auth_error":
                st, msg, reason, key = std_key[label]
                want = bool(ns[fn](st, msg, reason, api_key=key))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH is_standard_key_auth_error case={label} C={got!r} PY={want!r}")

            elif fn == "_coerce_content_to_text":
                want = ns[fn](coerces[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _coerce_content_to_text case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE gemini_native_adapter: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
