#!/usr/bin/env python3
"""Oracle for the anthropic message-conversion FLOW port
(src/agent/anthropic_convert_flow2.c).

Drives the LIVE quarry defs of the 15 UNCLAIMED agent/anthropic_message_convert.py
functions with SCRIPTED seams: every CLAIMED sibling (the 19 transport-flow leaves
plus _carry_cache_control / _split_blank_text_blocks / _sanitize_tool_id /
_normalize_tool_input_schema / _convert_content_part_to_anthropic /
_apply_assistant_cache_control_to_last_cacheable_block) and the foreign policy
calls (bedrock_adapter.reasoning_minted_outside_claude, anthropic_thinking_policy
route/replays, image_eviction_policy limit/retire, logger.warning) is a recording
stub fed the SAME scripts the C harness feeds acv2_call. Compares the DECISION
SEQUENCE (ordered "trace" of {ev,args,ret}), the canonical return ("ret") and the
out-state ("state": {"result": ...} for the seven in-place mutators, echoed EVEN
ON A RAISE; {} otherwise). The PORTED functions call each other REAL (AST-extracted
into one registered sys.modules namespace, py3.14), so internal composition
(convert_messages_to_anthropic -> _convert_assistant_message ->
_convert_content_to_anthropic -> _fix_blank_text_blocks_in_list -> ...) is
exercised. The heavy import chain is never imported.

Fail-closed: 0 cases => exit 1; MISSING seam script => RuntimeError("no script
for <name>"); a scripted null is a real None result. Every per-case exception
rides as {"__raise__": cls, "msg": str(exc)} (BaseException catch, so SystemExit/
KeyboardInterrupt/GeneratorExit propagate). Exception classes resolve through
getattr(builtins, ...) — the FULL builtin hierarchy, no hand-rolled parent table.
Scripted raise markers round-trip bare (identity match) so multi-arg constructors
(KeyError) do not mangle.
Summary: 'ORACLE anthropic_convert_flow2: N cases, M mismatches'.

Representation contract (both sides, see the port header):
- None -> null; tuple/set -> array (sets sorted by str()); integral floats collapse
  to ints; dict key order mirrors live insertion order.
- _THINKING_TYPES / _CACHEABLE_TYPES are seeded as TUPLES (== membership, matching
  the C array model); the {"image"} set literal projects sorted to ["image"].
- apply_assistant_cache_control / carry_cache_control are STATE-EFFECT seams:
  recorded AND their documented mutation applied identically by the C port and
  this oracle's ApplyCCSeam / CarryCCSeam; args snapshot the carrier BEFORE the
  effect. carry_cache_control's scripted ret is the post-effect out dict.
- assistant_block_lists is an INDEX-REF seam: the scripted ret records [i, m]
  snapshots, but ABLSeam rebinds m to the live result[i] (generator identity) so
  in-place mutations land on the state window — exactly what the C port does.
- None-valued seam arg keys dropped on both sides; "" kept.
"""
import __future__
import ast
import builtins
import copy
import json
import os
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "anthropic_message_convert.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
# scrubbed-env safety: the quarry chain can reach Path.home()
os.environ.setdefault("HOME", str(_HERE.parents[2]))
os.environ.setdefault("USERPROFILE", str(_HERE.parents[2]))
os.environ.setdefault("LOCALAPPDATA", str(_HERE.parents[2] / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

AMC_SRC = str(QUARRY / "agent" / "anthropic_message_convert.py")
NAME = "anthropic_convert_flow2"

PORTED = [
    "_extract_preserved_thinking_blocks",
    "convert_tools_to_anthropic",
    "_convert_content_to_anthropic",
    "_content_parts_to_anthropic_blocks",
    "_convert_assistant_message",
    "_convert_tool_message_to_result",
    "_convert_user_message",
    "_strip_orphaned_tool_blocks",
    "_merge_consecutive_roles",
    "_manage_thinking_signatures",
    "_evict_old_screenshots",
    "_ensure_leading_user_turn",
    "_fix_blank_text_blocks_in_list",
    "_scrub_blank_text_blocks",
    "convert_messages_to_anthropic",
]
# claimed siblings seamed (live name -> seam name)
SEAMED_SIBLINGS = {
    "_block_type": "block_type",
    "_has_block_type": "has_block_type",
    "_text_block": "text_block",
    "_strip_thinking": "strip_thinking",
    "_block_ids": "block_ids",
    "_assistant_block_lists": "assistant_block_lists",
    "_parse_tool_args": "parse_tool_args",
    "_tool_use_block": "tool_use_block",
    "_concat_content": "concat_content",
    "_keep_valid_thinking": "keep_valid_thinking",
    "_current_tool_loop_start": "current_tool_loop_start",
    "_image_source_data": "image_source_data",
    "_convert_system_content": "convert_system_content",
    "assistant_replay_carrier": "assistant_replay_carrier",
    "_tool_result_content": "tool_result_content",
    "_carry_cache_control": "carry_cache_control",
    "_split_blank_text_blocks": "split_blank_text_blocks",
    "_sanitize_tool_id": "sanitize_tool_id",
    "_normalize_tool_input_schema": "normalize_tool_input_schema",
    "_convert_content_part_to_anthropic": "convert_content_part_to_anthropic",
    "_apply_assistant_cache_control_to_last_cacheable_block": "apply_assistant_cache_control",
}
# foreign policy seams (module-level imports + the in-function bedrock import)
FOREIGN_SEAMS = {
    "anthropic_thinking_route": "anthropic_thinking_route",
    "route_replays_prior_thinking": "route_replays_prior_thinking",
    "images_all_within_many_image_limit": "images_all_within_many_image_limit",
    "outbound_image_retire_count": "outbound_image_retire_count",
}
WANTED = set(PORTED)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


# ── AST extraction ──────────────────────────────────────────────────────────
_AMC_TEXT = Path(AMC_SRC).read_text(encoding="utf-8")
_AMC_TREE = ast.parse(_AMC_TEXT)
DEF_CODE = {}
for _node in _AMC_TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, AMC_SRC)
_missing = set(PORTED) - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


# ── canonical projection ────────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


# ── per-case context ────────────────────────────────────────────────────────
CUR = {"scripts": {}, "trace": [], "raises": [], "idx": {}, "stwin": {}}


def make_exc(marker):
    kind = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    # FULL builtin hierarchy via getattr(builtins, ...); unknown kinds become a
    # fresh Exception subclass (scripted custom classes round-trip by name).
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(str(kind), (Exception,), {})
    exc = cls(msg)
    CUR["raises"].append((exc, marker))
    return exc


def raise_spec(e):
    for exc, marker in CUR["raises"]:
        if e is exc:
            return marker
    return {"__raise__": type(e).__name__, "msg": str(e)}


def materialize(name, sc):
    # C universal seam returns a fresh json_copy per call; mirror with deepcopy so
    # in-place mutation of a seam result never aliases the fixture script.
    if isinstance(sc, (dict, list)):
        return copy.deepcopy(sc)
    return sc


def record(name, args):
    args = {k: norm(v) for k, v in args.items() if v is not None}
    io = CUR["scripts"]
    trace = CUR["trace"]
    if name not in io:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = io[name]
        if isinstance(sc, dict) and "$seq" in sc:
            seq = sc["$seq"]
            i = CUR["idx"].get(name, 0)
            if i >= len(seq):
                r = {"__raise__": "RuntimeError", "msg": "script exhausted: " + name}
            else:
                CUR["idx"][name] = i + 1
                r = seq[i]
        else:
            r = sc
    if isinstance(r, dict) and isinstance(r.get("__raise__"), str):
        trace.append({"ev": name, "args": args, "ret": r})
        raise make_exc(r)
    result = materialize(name, r)
    trace.append({"ev": name, "args": args, "ret": norm(r)})
    return result


SEAM_ARGS = {
    "block_type": lambda a, k: {"b": a[0]},
    "has_block_type": lambda a, k: {"blocks": a[0], "types": a[1]},
    "text_block": lambda a, k: {"text": a[0]},
    "strip_thinking": lambda a, k: {"blocks": a[0]},
    "block_ids": lambda a, k: {"blocks": a[0], "btype": a[1], "key": a[2]},
    "assistant_block_lists": lambda a, k: {"result": a[0]},
    "parse_tool_args": lambda a, k: {"raw": a[0]},
    "tool_use_block": lambda a, k: {"tool_id": a[0], "name": a[1], "tool_input": a[2]},
    "concat_content": lambda a, k: {"prev": a[0], "curr": a[1]},
    "keep_valid_thinking": lambda a, k: {"content": a[0], "signature_dead": a[1]},
    "current_tool_loop_start": lambda a, k: {"result": a[0]},
    "image_source_data": lambda a, k: {"block": a[0]},
    "convert_system_content": lambda a, k: {"content": a[0]},
    "assistant_replay_carrier": lambda a, k: {"m": a[0]},
    "tool_result_content": lambda a, k: {"m": a[0]},
    "carry_cache_control": lambda a, k: {"out": a[0], "b": a[1], "copy": k.get("copy", False)},
    "split_blank_text_blocks": lambda a, k: {"blocks": a[0]},
    "sanitize_tool_id": lambda a, k: {"tool_id": a[0]},
    "normalize_tool_input_schema": lambda a, k: {"schema": a[0]},
    "convert_content_part_to_anthropic": lambda a, k: {"part": a[0]},
    "apply_assistant_cache_control": lambda a, k: {"blocks": a[0], "cache_control": a[1]},
    "reasoning_minted_outside_claude": lambda a, k: {"message": a[0]},
    "anthropic_thinking_route": lambda a, k: {"base_url": a[0], "model": a[1]},
    "route_replays_prior_thinking": lambda a, k: {"route": a[0], "model": a[1]},
    "images_all_within_many_image_limit": lambda a, k: {"datas": a[0]},
    "outbound_image_retire_count": lambda a, k: {
        "counts": a[0], "reserved_count": a[1],
        "carrier_bytes_newest_first": k["carrier_bytes_newest_first"],
        "reserved_bytes": k["reserved_bytes"], "many_image_safe": k["many_image_safe"]},
    "logger.warning": lambda a, k: {"fmt": a[0], "args": list(a[1:])},
}


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


class ApplyCCSeam:
    """_apply_assistant_cache_control_to_last_cacheable_block: recorded seam PLUS
    the identical state effect the C port applies (acv2_apply_cc_effect). Args
    snapshot blocks BEFORE the effect (record norms first)."""
    def __call__(self, blocks, cache_control):
        r = record("apply_assistant_cache_control",
                   {"blocks": blocks, "cache_control": cache_control})
        if isinstance(cache_control, dict):
            for block in reversed(blocks):
                bt = block.get("type") if isinstance(block, dict) else None
                if bt in ("text", "tool_use"):
                    if isinstance(block, dict):
                        block.setdefault("cache_control", dict(cache_control))
                    break
        return r


class CarryCCSeam:
    """_carry_cache_control(out, b, copy=): recorded seam PLUS the identical state
    effect (acv2_carry_cc_effect). The live return is the mutated out; the script
    records it (convert_tools_to_anthropic appends the return)."""
    def __call__(self, out, b, copy=False):
        r = record("carry_cache_control", {"out": out, "b": b, "copy": copy})
        cc = b.get("cache_control") if isinstance(b, dict) else None
        if isinstance(cc, dict):
            out["cache_control"] = dict(cc) if copy else cc
        return r


class ABLSeam:
    """_assistant_block_lists: INDEX-REF seam. The scripted ret records [i, m]
    snapshots for the trace; the live generator yields (i, result[i]) with m the
    SAME object the caller mutates, so rebind m to the live result[i]."""
    def __call__(self, result):
        r = record("assistant_block_lists", {"result": result})
        return [(pair[0], result[pair[0]]) for pair in r]


class BlockIdsSeam:
    """_block_ids returns a SET; the ported callers do set arithmetic (-, |=, in).
    The script/trace carry the sorted-array projection; resurrect a real set for
    the live drive (key values are hashable str/null in the fixture domain)."""
    def __call__(self, blocks, btype, key):
        r = record("block_ids", {"blocks": blocks, "btype": btype, "key": key})
        return set(r)


# ── stub modules for the in-function imports ────────────────────────────────
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    agent = _mod("agent")
    agent.__path__ = []
    brd = _mod("agent.bedrock_adapter",
               reasoning_minted_outside_claude=SeamFn("reasoning_minted_outside_claude"))
    agent.bedrock_adapter = brd


# ── base namespace (registered module) ──────────────────────────────────────
_BASE_NS = {
    "__name__": "acv2_live_convert",
    "__builtins__": builtins,
    "json": json, "copy": copy,
    "logger": types.SimpleNamespace(warning=SeamFn("logger.warning")),
    "Any": object, "Dict": dict, "List": list, "Optional": object, "Tuple": tuple,
    # constants (tuples for == membership, matching the C array model)
    "_THINKING_TYPES": ("thinking", "redacted_thinking"),
    "_CACHEABLE_TYPES": ("text", "tool_use"),
    "_EMPTY_TEXT_PLACEHOLDER": "(empty)",
}
_MOD = types.ModuleType("acv2_live_convert")
_MOD.__dict__.update(_BASE_NS)
sys.modules["acv2_live_convert"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
# install claimed-sibling seams AFTER exec (call-time global lookup)
for _live, _seam in SEAMED_SIBLINGS.items():
    if _seam == "apply_assistant_cache_control":
        _BASE_NS[_live] = ApplyCCSeam()
    elif _seam == "carry_cache_control":
        _BASE_NS[_live] = CarryCCSeam()
    elif _seam == "assistant_block_lists":
        _BASE_NS[_live] = ABLSeam()
    elif _seam == "block_ids":
        _BASE_NS[_live] = BlockIdsSeam()
    else:
        _BASE_NS[_live] = SeamFn(_seam)
for _live, _seam in FOREIGN_SEAMS.items():
    _BASE_NS[_live] = SeamFn(_seam)
install_stubs()


def build_ns(rec):
    inp = rec["in"]
    CUR["scripts"] = inp.get("io") or {}
    CUR["trace"] = []
    CUR["raises"] = []
    CUR["idx"] = {}
    CUR["stwin"] = {}
    return _BASE_NS


# ── live driver ─────────────────────────────────────────────────────────────
STATEFUL = {
    "_convert_tool_message_to_result", "_strip_orphaned_tool_blocks",
    "_merge_consecutive_roles", "_manage_thinking_signatures",
    "_evict_old_screenshots", "_ensure_leading_user_turn",
    "_scrub_blank_text_blocks",
}


def drive(rec, ns):
    func = rec["func"]
    inp = rec["in"]
    g = inp.get
    if func == "_extract_preserved_thinking_blocks":
        return ns[func](g("message"))
    if func == "convert_tools_to_anthropic":
        return ns[func](g("tools"))
    if func == "_convert_content_to_anthropic":
        return ns[func](g("content"))
    if func == "_content_parts_to_anthropic_blocks":
        return ns[func](g("parts"))
    if func == "_convert_assistant_message":
        return ns[func](g("m"))
    if func == "_convert_user_message":
        return ns[func](g("content"))
    if func == "_fix_blank_text_blocks_in_list":
        return ns[func](g("blocks"), placeholder_text=g("placeholder_text", "(empty)"),
                        msg_index=g("msg_index", 0), role=g("role"),
                        location=g("location", "content"))
    if func == "convert_messages_to_anthropic":
        return ns[func](g("messages"), g("base_url"), g("model"))
    if func in STATEFUL:
        result = copy.deepcopy(g("result"))
        CUR["stwin"] = {"result": result}
        if func == "_convert_tool_message_to_result":
            return ns[func](result, g("m"))
        if func == "_manage_thinking_signatures":
            return ns[func](result, g("base_url"), g("model"))
        return ns[func](result)
    raise KeyError(f"unknown func {func}")


def run_case(rec):
    ns = build_ns(rec)
    try:
        ret = drive(rec, ns)
        return {"ret": norm(ret), "trace": CUR["trace"], "state": norm(CUR["stwin"])}
    except BaseException as e:  # noqa: BLE001 - fail-closed per case; state echo on raise
        return {"ret": raise_spec(e), "trace": CUR["trace"], "state": norm(CUR["stwin"])}


def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        try:
            out = run_case(rec)
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": {"__raise__": type(e).__name__, "msg": str(e)}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 12:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test: drive every fixture, flag binding/seeding failures ───────────
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    bad = 0
    seam_names = set(SEAM_ARGS)
    ported_names = set(PORTED)
    seen_funcs = set()
    for i, rec in enumerate(fixtures):
        seen_funcs.add(rec["func"])
        try:
            out = run_case(rec)
            spec = out["ret"]
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("msg", "")
                if any(("'" + nm + "'") in msg for nm in (ported_names | seam_names)):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError naming ported/seam: {msg}")
            if isinstance(spec, dict) and spec.get("__raise__") == "NameError":
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: NameError (seeding gap): {spec.get('msg')}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    missing_funcs = set(PORTED) - seen_funcs
    if missing_funcs:
        bad += 1
        print(f"SELFTEST: funcs with no fixture: {sorted(missing_funcs)}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "anthropic_convert_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").split("\n") if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()