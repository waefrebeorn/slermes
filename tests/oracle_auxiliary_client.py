#!/usr/bin/env python3
"""oracle_auxiliary_client.py -- diffs the C port against the real Python.

Reads the harness JSONL on stdin. The Python reference is extracted with ast
from agent/auxiliary_client.py (the module itself cannot be imported here:
heavy deps), plus agent/model_metadata.py for is_codex_context_variant.
"""
import ast
import json
import re
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

WANTED = {
    "agent/auxiliary_client.py": [
        "_bare_model", "_is_kimi_model", "_is_arcee_trinity_thinking",
        "_codex_route_bare_model", "_is_codex_spark",
        "_is_openai_default_temperature_only", "_is_codex_gpt54_or_gpt55",
        "_compression_threshold_for_model", "_model_recency_key",
        "_CODEX_GPT54_GPT55_COMPACTION_THRESHOLD",
        "_CODEX_SPARK_COMPACTION_THRESHOLD",
        "_field", "_anthropic_event_has_content",
        "_OR_HEADERS_BASE", "build_or_headers",
    ],
    "agent/model_metadata.py": [
        "_bare_codex_slug", "is_codex_900k_base", "_codex_variant_base",
        "is_codex_context_variant", "CODEX_CONTEXT_VARIANT_SUFFIX",
        "_CODEX_900K_SNAPSHOT_BASES", "_CODEX_900K_ELIGIBLE_BASES",
        "_CODEX_900K_SNAPSHOT_RE",
    ],
    "utils.py": ["base_url_hostname", "_parse_base_url", "_hostname_of",
                 "base_url_host_matches"],
    "hermes_cli/version_info.py": ["hermes_user_agent"],
}

import os
from urllib.parse import urlparse

ns = {"re": re, "Optional": object, "Any": object, "os": os, "urlparse": urlparse,
      "logger": types.SimpleNamespace(), "__name__": "oracle_aux_extracted"}

picked = set()
for rel, names in WANTED.items():
    with open(f"{QUARRY}/{rel}", encoding="utf-8") as f:
        text = f.read()
    tree = ast.parse(text)
    want = set(names)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in want:
            exec(compile(ast.get_source_segment(text, node), rel, "exec"), ns)
            picked.add(node.name)
        elif isinstance(node, ast.Assign):
            got = {t.id for t in node.targets if isinstance(t, ast.Name)}
            if got & want:
                exec(compile(ast.get_source_segment(text, node), rel, "exec"), ns)
                picked |= (got & want)
        elif isinstance(node, ast.AnnAssign):
            if isinstance(node.target, ast.Name) and node.target.id in want:
                exec(compile(ast.get_source_segment(text, node), rel, "exec"), ns)
                picked.add(node.target.id)

missing = set().union(*[set(v) for v in WANTED.values()]) - picked
if missing:
    print(f"ORACLE auxiliary_client: FATAL, source not found: {sorted(missing)}")
    raise SystemExit(1)

# _is_codex_gpt54_or_gpt55 imports is_codex_context_variant from
# agent.model_metadata, and importing that module for real pulls ruamel. The
# functions are already extracted into `ns`, so satisfy the import from a stub.
_mm = types.ModuleType("agent.model_metadata")
_mm.is_codex_context_variant = ns["is_codex_context_variant"]
sys.modules.setdefault("agent", types.ModuleType("agent"))
sys.modules["agent.model_metadata"] = _mm

# hermes_user_agent reads get_version_info().base_version. The C port takes the
# base version explicitly, so stub the lookup to return the harness's value.
_CUR_BASE = [""]


class _VI:
    @property
    def base_version(self):
        return _CUR_BASE[0]


ns["get_version_info"] = lambda: _VI()

# build_or_headers falls back to load_config_readonly() when or_config is None;
# every case passes an explicit config, so the fallback is never taken.
ns["load_config_readonly"] = lambda: {}


def recency_key(model_id):
    """Canonical form of _model_recency_key's tuple: t:<text> / n:<float %.6f>."""
    parts = [p for p in re.split(r"(\d+(?:\.\d+)?)", model_id.lower()) if p]
    out = []
    for i, p in enumerate(parts):
        if i % 2:
            out.append("n:%.6f" % float(p))
        else:
            out.append("t:" + p)
    return "|".join(out)


cases = mismatches = shown = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        c = json.loads(line)
    except json.JSONDecodeError:
        continue
    fn = c["fn"]
    arg = c.get("arg", "")
    prov = c.get("prov", "")
    cases += 1

    if fn == "bare":
        want = ns["_bare_model"](arg)
    elif fn == "kimi":
        want = bool(ns["_is_kimi_model"](arg))
    elif fn == "trinity":
        want = bool(ns["_is_arcee_trinity_thinking"](arg))
    elif fn == "codexbare":
        want = ns["_codex_route_bare_model"](arg, prov)
    elif fn == "spark":
        want = bool(ns["_is_codex_spark"](arg, prov))
    elif fn == "gpt5455":
        want = bool(ns["_is_codex_gpt54_or_gpt55"](arg, prov))
    elif fn == "tempdefault":
        want = bool(ns["_is_openai_default_temperature_only"](arg))
    elif fn == "threshold":
        got_th = c["out"]
        th = ns["_compression_threshold_for_model"](arg, prov,
                                                   allow_codex_gpt55_autoraise=c["allow"])
        want = -1.0 if th is None else th
        if abs(got_th - want) > 1e-12:
            mismatches += 1
            if shown < 12:
                print(f"MISMATCH threshold {arg!r}/{prov!r} allow={c['allow']}: "
                      f"got {got_th} want {want}")
                shown += 1
        continue
    elif fn == "recency":
        want = recency_key(arg)
    elif fn == "hostname":
        want = ns["base_url_hostname"](arg)
    elif fn == "hostmatch":
        want = bool(ns["base_url_host_matches"](arg, c.get("domain", "")))
    elif fn == "nvidia":
        want = bool(ns["base_url_host_matches"](arg, "integrate.api.nvidia.com"))
    elif fn == "event":
        try:
            ev = json.loads(arg)
        except Exception:
            ev = None
        # _field(None, "type") is getattr(None, "type", None) -> None -> False,
        # so a JSON null event is simply "no content", not an error.
        want = bool(ns["_anthropic_event_has_content"](ev))
    elif fn == "aigateway":
        _CUR_BASE[0] = arg
        want = {"HTTP-Referer": "https://hermes-agent.nousresearch.com",
                "X-Title": "Hermes Agent",
                "User-Agent": ns["hermes_user_agent"]("HermesAgent")}
    elif fn == "orheaders":
        os.environ["HERMES_OPENROUTER_CACHE"] = c.get("env", "")
        os.environ["HERMES_OPENROUTER_CACHE_TTL"] = c.get("envttl", "")
        cfg = {}
        if c.get("cfgcache"):
            cfg["response_cache"] = True
        if c.get("cfgtll") is not None:
            cfg["response_cache_ttl"] = c["cfgtll"]
        want = ns["build_or_headers"](cfg)
    else:
        continue

    got = c["out"]
    if isinstance(want, bool):
        got = bool(got)
    else:
        # The harness emits `out` as a JSON string holding serialised JSON;
        # parse it back so objects compare as objects, not as strings.
        if isinstance(got, str) and got != "MISSING":
            try:
                got = json.loads(got)
            except json.JSONDecodeError:
                pass
    if got != want:
        mismatches += 1
        if shown < 12:
            print(f"MISMATCH {fn} {json.dumps(c, sort_keys=True)}: got {got!r} want {want!r}")
            shown += 1

print(f"ORACLE auxiliary_client: {cases} cases, {mismatches} mismatches")
raise SystemExit(1 if mismatches else 0)
