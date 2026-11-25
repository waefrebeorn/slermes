#!/usr/bin/env python3
"""Differential oracle for tui_gateway/contracts/sessions.py (gate w34_sessions_67685b).

The live module is imported as a package module so its relative imports resolve.
Nothing connects, spawns or persists: what is compared is what the declarations
decide.

  * the method catalog this module registers (name -> params / result model);
  * the model inventory and every declared field of every model (kind,
    requiredness, default, default_factory, alias, min_length, accepted keys);
  * the params envelope (``registry.validate_params``: an unknown key answers
    the 4000 text with the key path) plus the required-key set;
  * how a params model names the session (``session_id`` vs the
    ``session_key`` alias) and the closed ``WireEnum`` sets.

Every expected value is read out of the live pydantic models, so the C tables
are compared against the declaration itself, not against a restatement of it.
"""
import json
import sys
import typing
from enum import StrEnum
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_sessions_67685b"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
# The oracle's own directory, so helper modules beside it import by bare name.
sys.path.append(str(Path(__file__).resolve().parent))

# Real pydantic is the truth here (the declarations ARE pydantic models), so no
# seam stand-in is installed: the stub finder would fabricate pydantic
# submodules and break the models it is supposed to compare against.
import pydantic  # noqa: E402

from tui_gateway.contracts import registry  # noqa: E402
from tui_gateway.contracts import sessions as live  # noqa: E402

WANTED = {"catalog", "models", "fields", "required", "validate",
          "id_requirement", "enum", "accepts"}

MOD = live.__name__

MODELS = {}
for _name, _obj in vars(live).items():
    if isinstance(_obj, type) and issubclass(_obj, pydantic.BaseModel) and _obj.__module__ == MOD:
        MODELS[_name] = _obj

METHODS = {n: c for n, c in registry.METHODS.items() if c.params.__module__ == MOD}

ENUMS = {}
for _name, _obj in vars(live).items():
    if isinstance(_obj, type) and issubclass(_obj, StrEnum) and _obj.__module__ == MOD:
        ENUMS[_name] = sorted(m.value for m in _obj)


# ── the coarse annotation kind the C tables carry ────────────────────────────
# A convention shared by the port and this oracle; it never decides a value, it
# only says which JSON shape a field holds.

def kind(ann):
    if ann is None or ann is type(None):
        return "none"
    origin, args = typing.get_origin(ann), typing.get_args(ann)
    if origin is typing.Union:
        if type(None) in args:
            rest = [a for a in args if a is not type(None)]
            if len(rest) == 1:
                return "opt(" + kind(rest[0]) + ")"
            return "opt(union(" + ",".join(kind(a) for a in rest) + "))"
        return "union(" + ",".join(kind(a) for a in args) + ")"
    if origin is list:
        return "list(" + kind(args[0]) + ")"
    if origin is dict:
        return "dict(" + kind(args[0]) + "," + kind(args[1]) + ")"
    if origin is typing.Literal:
        return "literal"
    if ann is str:
        return "str"
    if ann is int:
        return "int"
    if ann is float:
        return "float"
    if ann is bool:
        return "bool"
    if ann is typing.Any:
        return "json"
    if isinstance(ann, type):
        if issubclass(ann, StrEnum):
            return "enum"
        if issubclass(ann, pydantic.BaseModel):
            return "model"
    return "other"


def min_length_of(fi):
    for meta in fi.metadata:
        if getattr(meta, "min_length", None) is not None:
            return int(meta.min_length)
    return None


def alias_of(name, fi):
    return fi.alias if fi.alias and fi.alias != name else None


def spellings(name, fi, populate_by_name):
    """The input keys that fill this field."""
    alias = alias_of(name, fi)
    out = []
    if alias is None or populate_by_name:
        out.append(name)
    if alias:
        out.append(alias)
    return out


def accepted_keys(model):
    pbn = bool(model.model_config.get("populate_by_name"))
    keys = []
    for name, fi in model.model_fields.items():
        keys.extend(spellings(name, fi, pbn))
    return sorted(keys)


def fields(name):
    model = MODELS.get(name)
    if model is None:
        return None
    pbn = bool(model.model_config.get("populate_by_name"))
    rows = []
    for fname, fi in model.model_fields.items():
        row = {
            "name": fname,
            "kind": kind(fi.annotation),
            "required": fi.is_required(),
            "factory": (not fi.is_required()) and fi.default_factory is not None,
            "alias": alias_of(fname, fi),
            "min_length": min_length_of(fi),
        }
        if not fi.is_required() and fi.default_factory is None:
            row["default"] = json.loads(json.dumps(fi.default, sort_keys=True))
        rows.append(row)
    return {
        "model": name,
        "extra": "allow" if model.model_config.get("extra") == "allow" else "forbid",
        "populate_by_name": pbn,
        "keys": accepted_keys(model),
        "fields": rows,
    }


def required(name):
    model = MODELS.get(name)
    if model is None:
        return None
    return sorted(f for f, fi in model.model_fields.items() if fi.is_required())


def validate(method, params):
    contract = METHODS.get(method)
    if contract is None:
        return {"found": False, "unknown_keys": [], "missing_keys": [],
                "error": None, "shape_ok": False}
    model = contract.params
    given = params if isinstance(params, dict) else {}
    keys = accepted_keys(model)
    pbn = bool(model.model_config.get("populate_by_name"))
    unknown = [k for k in given if k not in keys]
    missing = []
    for fname, fi in model.model_fields.items():
        if fi.is_required() and not any(s in given for s in spellings(fname, fi, pbn)):
            missing.append(fname)
    # The gateway's own envelope: the first undeclared key, as the 4000 text ships it.
    _, error = registry.validate_params(contract, given)
    return {
        "found": True,
        "unknown_keys": unknown,
        "missing_keys": missing,
        "error": error,
        "shape_ok": not unknown and not missing,
    }


def id_requirement(method):
    contract = METHODS.get(method)
    if contract is None:
        return None
    model = contract.params
    sid = model.model_fields.get("session_id")
    key = model.model_fields.get("session_key")
    if sid is not None and sid.is_required():
        requirement = "session_id"
    elif sid is not None and key is not None:
        requirement = "either"
    elif sid is not None:
        requirement = "optional"
    else:
        requirement = "none"
    return {
        "model": model.__name__,
        "has_session_id": sid is not None,
        "session_id_required": bool(sid is not None and sid.is_required()),
        "has_session_key": key is not None,
        "requirement": requirement,
    }


def expected(r):
    f = r.get("func")
    if f == "catalog":
        return sorted(
            ({"method": n, "params": c.params.__name__, "result": c.result.__name__}
             for n, c in METHODS.items()),
            key=lambda m: m["method"])
    if f == "models":
        return sorted(MODELS)
    if f == "fields":
        return fields(r.get("model"))
    if f == "required":
        return required(r.get("model"))
    if f == "validate":
        return validate(r.get("method"), r.get("params"))
    if f == "id_requirement":
        return id_requirement(r.get("method"))
    if f == "enum":
        return ENUMS.get(r.get("name"))
    if f == "accepts":
        contract = METHODS.get(r.get("method"))
        if contract is None:
            return None
        return bool(r.get("key") in accepted_keys(contract.params))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
