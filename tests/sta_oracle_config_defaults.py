#!/usr/bin/env python3
"""sta_oracle_config_defaults.py — Python twin for the pure-data tables in
hermes_cli/config_defaults.py, mirrored by src/hermes_cli/config_defaults.c.

Reads a JSON fixture (argv[1]) with a single "ops" list. Ops:

  {"kind":"full_default"}                       -> verbatim canonical JSON text
  {"kind":"full_env"}
  {"kind":"keys_at","path":"database"}          -> object keys, insertion order
  {"kind":"keys_env_at","path":"NOUS_BASE_URL"} -> OPTIONAL_ENV_VARS sub-keys
  {"kind":"type_at","path":"agent.max_turns"}   -> leaf/nod type name
  {"kind":"str_at","path":"model"}              -> string leaf (or null)
  {"kind":"count_default_keys"}
  {"kind":"count_env_vars"}
  {"kind":"fresh_parse_check"}

Emits one JSON line per op (same order) so the C harness output can be
diffed byte-for-byte.
"""
import json
import sys

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from hermes_cli.config_defaults import DEFAULT_CONFIG, OPTIONAL_ENV_VARS


_MISSING = object()


def nav(root, path):
    if path is None or path == "":
        return root
    cur = root
    for seg in str(path).split("."):
        if cur is _MISSING:
            return _MISSING
        if isinstance(cur, list):
            try:
                idx = int(seg)
            except ValueError:
                return _MISSING
            if 0 <= idx < len(cur):
                cur = cur[idx]
            else:
                return _MISSING
        elif isinstance(cur, dict):
            if seg in cur:
                cur = cur[seg]
            else:
                return _MISSING
        else:
            return _MISSING
    return cur


def type_name(n):
    if n is _MISSING:
        return "missing"
    if isinstance(n, dict):
        return "object"
    if isinstance(n, list):
        return "array"
    if isinstance(n, bool):
        return "true" if n else "false"
    if n is None:
        return "null"
    if isinstance(n, (int, float)):
        return "number"
    if isinstance(n, str):
        return "string"
    return "unknown"


def dumps(obj):
    # matches libjson json_serialize output: compact, raw UTF-8, no spaces
    return json.dumps(obj, ensure_ascii=False, separators=(",", ":"))


def out_line(o):
    print(json.dumps(o, ensure_ascii=False, separators=(",", ":")))


def main():
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        fx = json.load(f)
    for c in fx.get("ops", []):
        kind = c.get("kind", "")
        o = {}
        if kind == "full_default":
            o["out"] = dumps(DEFAULT_CONFIG)
        elif kind == "full_env":
            o["out"] = dumps(OPTIONAL_ENV_VARS)
        elif kind == "keys_at":
            t = nav(DEFAULT_CONFIG, c.get("path", ""))
            o["out"] = list(t.keys()) if isinstance(t, dict) else []
        elif kind == "keys_env_at":
            t = nav(OPTIONAL_ENV_VARS, c.get("path", ""))
            o["out"] = list(t.keys()) if isinstance(t, dict) else []
        elif kind == "type_at":
            t = nav(DEFAULT_CONFIG, c.get("path", ""))
            o["out"] = type_name(t)
        elif kind == "str_at":
            t = nav(DEFAULT_CONFIG, c.get("path", ""))
            o["out"] = t if isinstance(t, str) else None
        elif kind == "count_default_keys":
            o["out"] = len(DEFAULT_CONFIG)
        elif kind == "count_env_vars":
            o["out"] = len(OPTIONAL_ENV_VARS)
        elif kind == "fresh_parse_check":
            a = json.loads(dumps(DEFAULT_CONFIG))
            b = json.loads(dumps(DEFAULT_CONFIG))
            a["__probe__"] = 1
            independent = "__probe__" not in b
            del a["__probe__"]
            restored = dumps(a) == dumps(b)
            o["independent"] = independent
            o["restored"] = restored
        out_line(o)


if __name__ == "__main__":
    main()
