#!/usr/bin/env python3
"""Generate tests/fixtures/relay_descriptor_flow4.jsonl (>=5 cases per func).

Deterministic, no randomness. Each case is a JSONL object
    {"func":..., "in":{...}}
fed to t_port_relay_descriptor_flow4 on stdin. from_json cases carry their
`data` string plus the SCRIPTED `int_cast` seam responses (a number, or a
{"__raise__", "msg"} builtin-exception spec) shared by the C harness and the
live oracle. `hash` cases script the `hash` seam (the frozen dataclass's
generated __hash__ calls hash((fields...)); the C port never computes CPython's
seed). Covers the full py3.14.9 builtin exception hierarchy at the
`except (TypeError, ValueError)` site: caught TypeError/ValueError/UnicodeError/
JSONDecodeError vs propagated NotImplementedError/RuntimeError/
PythonFinalizationError/_IncompleteInputError/EOFError/OSError(aliases)/
SystemExit/KeyboardInterrupt/GeneratorExit/AttributeError. ExceptionGroup and
BaseExceptionGroup are in the hierarchy TABLE but kept out of fixtures (their
str(e) carries a sub-exception suffix, out of the raise-spec domain), plus the
non-dict-raw AttributeError path.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "relay_descriptor_flow4.jsonl")

REQ = {
    "contract_version": 1,
    "platform": "slack",
    "label": "Slack",
    "max_message_length": 4096,
    "supports_draft_streaming": False,
    "supports_edit": True,
    "supports_threads": False,
    "markdown_dialect": "plain",
    "len_unit": "chars",
}

ALL_OPTIONAL = {
    "emoji": "🔌",
    "platform_hint": "room-hint",
    "pii_safe": True,
    "supports_context": True,
    "supports_inchannel_continuable": True,
    "supports_block_formatting": True,
    "supported_ops": ["send", "edit", "custom"],
}

cases = []


def add(func, inp):
    cases.append({"func": func, "in": inp})


def fld(**over):
    d = dict(REQ)
    d.update(over)
    return d


def jd(d):
    return json.dumps(d, ensure_ascii=False)


def raise_spec(kind, msg="boom"):
    return {"__raise__": kind, "msg": msg}


# ── init (frozen dataclass __init__: defaults + verbatim values) ──
add("init", {"fields": fld()})
add("init", {"fields": dict(fld(), **ALL_OPTIONAL)})
add("init", {"fields": fld(max_message_length="10")})          # verbatim str
add("init", {"fields": fld(max_message_length=True)})          # verbatim bool
add("init", {"fields": fld(supported_ops=["x", "y"], platform="discord", label="Discord")})
add("init", {"fields": fld(emoji="", platform_hint="", pii_safe=False,
                           len_unit="utf16", markdown_dialect="markdown")})

# ── supports_op (legacy fallback + explicit advertisement) ──
for op in ("send", "edit", "typing", "follow_up"):
    add("supports_op", {"fields": fld(), "op": op})            # legacy set -> True
add("supports_op", {"fields": fld(), "op": "delete"})          # new op -> False
add("supports_op", {"fields": fld(supported_ops=["send", "custom"]), "op": "custom"})
add("supports_op", {"fields": fld(supported_ops=["send", "custom"]), "op": "send"})
add("supports_op", {"fields": fld(supported_ops=["send", "custom"]), "op": "edit"})  # not advertised
add("supports_op", {"fields": fld(supported_ops=["custom"]), "op": "unknown"})

# ── to_json (sort_keys=True, ensure_ascii=False) ──
add("to_json", {"fields": fld()})
add("to_json", {"fields": dict(fld(), **ALL_OPTIONAL)})
add("to_json", {"fields": fld(emoji="🔧", platform="telegram", label="Telegram")})
add("to_json", {"fields": fld(supported_ops=["b", "a"], platform_hint="ctx")})
add("to_json", {"fields": fld(max_message_length="10")})       # stays a JSON string
add("to_json", {"fields": fld(pii_safe=True, supports_threads=True,
                              supports_draft_streaming=True, len_unit="utf16")})

# ── from_json (trust-boundary normalization + exception hierarchy) ──
def fc(tag, base=None, mml="ABSENT", ops="ABSENT", extra=None, seam=None, raw=None):
    """Build one from_json case."""
    d = dict(REQ) if base is None else dict(base)
    if mml != "ABSENT":
        d["max_message_length"] = mml
    else:
        d.pop("max_message_length", None)
    if ops != "ABSENT":
        d["supported_ops"] = ops
    if extra:
        d.update(extra)
    inp = {"data": jd(d if raw is None else raw)}
    if seam is not None:
        inp["seams"] = {"int_cast": [seam]}
    add("from_json", inp)


# valid / defaults
fc("full", mml=100, ops=["send", "edit"], seam=100,
   extra={"emoji": "🔌", "platform_hint": "h", "pii_safe": True})
fc("partial", base={"contract_version": 1, "platform": "p", "label": "l",
                    "supports_draft_streaming": False, "supports_edit": False,
                    "supports_threads": False, "markdown_dialect": "m", "len_unit": "u"},
   mml=50, seam=50)
# non-positive / garbage -> 4096
fc("zero", mml=0, seam=0)
fc("negative", mml=-5, seam=-5)
fc("valueerr", mml="abc", seam=raise_spec("ValueError"))
fc("typeerr", mml=None, seam=raise_spec("TypeError"))
# positive result leaves ORIGINAL value verbatim
fc("str10", mml="10", seam=10)
fc("int10", mml=10, seam=10)
fc("booltrue", mml=True, seam=1)
# supported_ops normalization
fc("ops-mixed", mml=10, seam=10, ops=["send", "", 5, "custom"])
fc("ops-nonlist", mml=10, seam=10, ops="send")
fc("ops-ints", mml=10, seam=10, ops=[1, 2])
# unknown keys dropped
fc("unknown", mml=10, seam=10, extra={"bogus": 123, "contract_version": 1})
# full builtin exception hierarchy at the except (TypeError, ValueError) site
fc("unicode-caught", mml=1, seam=raise_spec("UnicodeError"))  # depth-2 ValueError subclass
fc("notimpl-prop", mml=1, seam=raise_spec("NotImplementedError"))  # -> RuntimeError -> Exception
fc("runtime-prop", mml=1, seam=raise_spec("RuntimeError"))
fc("oserror-prop", mml=1, seam=raise_spec("OSError"))
fc("attr-prop", mml=1, seam=raise_spec("AttributeError"))
fc("systemexit-prop", mml=1, seam=raise_spec("SystemExit"))       # BaseException trio
fc("keyboard-prop", mml=1, seam=raise_spec("KeyboardInterrupt"))
fc("genexit-prop", mml=1, seam=raise_spec("GeneratorExit"))
# py3.14 additions + aliases + deep ValueError subclasses (all propagate)
fc("jsonderr-caught", mml=1, seam=raise_spec("JSONDecodeError"))  # ValueError subclass
fc("unicodedecode-caught", mml=1, seam=raise_spec("UnicodeDecodeError"))  # depth-2
fc("pyfinal-prop", mml=1, seam=raise_spec("PythonFinalizationError"))  # -> RuntimeError
fc("incomplete-prop", mml=1, seam=raise_spec("_IncompleteInputError"))  # -> SyntaxError
fc("eof-prop", mml=1, seam=raise_spec("EOFError"))
fc("taberror-prop", mml=1, seam=raise_spec("TabError"))  # -> IndentationError
fc("unbound-prop", mml=1, seam=raise_spec("UnboundLocalError"))  # -> NameError
# Aliases (IOError/EnvironmentError/WindowsError -> OSError) and KeyError are
# in the hierarchy TABLE but out of the fixture raise-spec domain: the live
# type name normalizes to OSError and str(KeyError) adds quotes.
# non-dict raw -> AttributeError from .items()
add("from_json", {"data": jd([1, 2])})
add("from_json", {"data": jd("hi")})

# ── repr (frozen-dataclass __repr__, declaration order) ──
add("repr", {"fields": fld()})
add("repr", {"fields": dict(fld(), **{k: v for k, v in ALL_OPTIONAL.items()
                                       if k != "supported_ops"})})
add("repr", {"fields": fld(supported_ops=["a", "b"])})
add("repr", {"fields": fld(supported_ops=["x"])})
add("repr", {"fields": fld(platform="discord", label="Discord",
                           markdown_dialect="markdown", len_unit="utf16")})

# ── eq (dataclass field equality) ──
add("eq", {"a": fld(), "b": fld()})
add("eq", {"a": fld(), "b": fld(platform="discord")})
add("eq", {"a": fld(), "b": dict(fld(), emoji="🔌", platform_hint="", pii_safe=False)})
add("eq", {"a": fld(supported_ops=["send"]), "b": fld(supported_ops=["edit"])})
add("eq", {"a": fld(), "b": fld(supports_edit=False)})

# ── hash (frozen-dataclass generated __hash__; hash() is a seam) ──
# The seam script is the DIRECT result of hash((fields...)): a number, null,
# or a {"__raise__", "msg"} spec propagated out of the generated method.
def hc(tag, fields=None, seam_result=123456789):
    inp = {"fields": fld() if fields is None else fields,
           "seams": {"hash": [seam_result]}}
    add("hash", inp)


hc("basic", seam_result=123456789)
hc("negative", seam_result=-42)
hc("zero", seam_result=0)
hc("all-optional", dict(fld(), **ALL_OPTIONAL), seam_result=987654321012345)
hc("null-result", seam_result=None)
hc("raise-type", seam_result=raise_spec("TypeError"))
hc("raise-runtime", seam_result=raise_spec("RuntimeError"))
hc("raise-systemexit", seam_result=raise_spec("SystemExit"))
hc("unicode-fields", fld(platform="télégram", label="Tël", emoji="🔧"),
   seam_result=55555)
hc("ops-list", fld(supported_ops=["send", "edit"]), seam_result=777)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
    for c in cases:
        fh.write(json.dumps(c, ensure_ascii=False) + "\n")

counts = {}
for c in cases:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print("wrote %s (%d cases)" % (OUT, len(cases)))
print("per-func: " + ", ".join("%s=%d" % (k, counts[k]) for k in sorted(counts)))
assert all(n >= 5 for n in counts.values()), "every function needs >=5 cases"
assert len(counts) == 7, "expected 7 functions"