#!/usr/bin/env python3
"""Differential oracle for tui_gateway/contracts/tools_mcp_plugins.py
(gate w32_toolsmcppl_39bf88).

The upstream module is a declaration set: 23 ``method(...)`` registrations,
each pinning a JSON-RPC name to a pydantic Params model (``extra="forbid"``)
and a Result model, plus 13 ``WireEnum`` closed sets.  It is imported as a
package module (its relative imports need the package context) so that its
registrations land in ``contracts.registry.METHODS``; pydantic's own
validation is then the differential reference for what the C port decides:

  * the method -> (params, result) index and the enum sets;
  * one schema descriptor per params field;
  * the shaped error list for unknown / missing / mistyped keys;
  * validated params with every declared default filled in;
  * the same top-level checks on the result side;
  * the profile-scoped predicate.

Error ``type`` values are normalised to a small closed vocabulary so the
comparison is about the DECISION (which key, which kind of fault), not about
pydantic's prose or about its lax-coercion mode (a declared seam: the C side
judges JSON-native types only).
"""
import json
import sys
import typing
from enum import Enum
from pathlib import Path

# This gate observes pydantic's own validation errors, so it needs the REAL
# package -- not the repo's permissive seam stubs. gate_batch runs oracles in one
# interpreter and restores sys.modules between them, but a stub left behind by a
# neighbouring gate makes `ConfigDict(...) | dict` a TypeError and hides every
# validation error. Drop stand-ins, keep the real module, and remove any stub
# import hook, then import.
for _name in [n for n in list(sys.modules)
              if n == "pydantic" or n.startswith("pydantic.")]:
    _mod = sys.modules.get(_name)
    _file = str(getattr(_mod, "__file__", "") or "")
    if not _file or "hermes_seam_stubs" in _file:
        del sys.modules[_name]
sys.meta_path[:] = [f for f in sys.meta_path
                    if type(f).__name__ != "_StubFinder"]

from pydantic import BaseModel, ValidationError  # noqa: E402

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_toolsmcppl_39bf88"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway.contracts import registry  # noqa: E402
from tui_gateway.contracts import tools_mcp_plugins as live  # noqa: E402

# The methods whose Params model this module owns.  mcp.catalog /
# mcp.servers.list / mcp.servers.status borrow common.ProfileParams and belong
# to the common.py gate.
METHODS = {name: c for name, c in registry.METHODS.items() if c.params.__module__ == live.__name__}

WANTED = {"methods", "enums", "params_spec", "validate_params",
          "normalize_params", "validate_result", "profile_scoped"}

# pydantic error type -> the port's normalised code
CODE = {
    "extra_forbidden": "extra",
    "missing": "missing",
    "enum": "enum",
    "string_type": "type_str",
    "int_type": "type_int",
    "int_parsing": "type_int",
    "float_type": "type_float",
    "float_parsing": "type_float",
    "bool_type": "type_bool",
    "bool_parsing": "type_bool",
    "list_type": "type_list",
    "dict_type": "type_dict",
    "model_type": "type_object",
    "model_attributes_type": "type_object",
}


def is_nullable(ann):
    if typing.get_origin(ann) is typing.Union:
        return any(a is type(None) for a in typing.get_args(ann))
    return False


def unwrap(ann):
    if typing.get_origin(ann) is typing.Union:
        args = [a for a in typing.get_args(ann) if a is not type(None)]
        return args[0] if len(args) == 1 else ann
    return ann


def tagof(ann):
    a = unwrap(ann)
    if isinstance(a, type) and issubclass(a, Enum):
        return "enum", a.__name__
    if a is str:
        return "str", None
    if a is int:
        return "int", None
    if a is bool:
        return "bool", None
    if a is float:
        return "float", None
    if a is typing.Any:
        return "any", None
    origin = typing.get_origin(a)
    if origin is list:
        return "list", None
    if origin is dict:
        return "dict", None
    if isinstance(a, type) and issubclass(a, BaseModel):
        return "model", a.__name__
    raise AssertionError("unmapped annotation %r" % (a,))


def default_of(field):
    if field.is_required():
        return False, None
    value = field.get_default(call_default_factory=True)
    if isinstance(value, Enum):
        value = value.value
    return True, value


def spec_of(model):
    out = {}
    for name, field in model.model_fields.items():
        tag, ref = tagof(field.annotation)
        has_default, default = default_of(field)
        choices = None
        if tag == "enum":
            enum_cls = getattr(live, ref)
            choices = [m.value for m in enum_cls]
        out[name] = {
            "required": field.is_required(),
            "nullable": is_nullable(field.annotation),
            "tag": tag,
            "ref": ref,
            "choices": choices,
            "has_default": has_default,
            "default": default,
        }
    return out


def errors_of(model, payload, root):
    try:
        model.model_validate(payload)
    except ValidationError as exc:
        rows = []
        for err in exc.errors():
            loc = ".".join(str(part) for part in err.get("loc", ())) or root
            rows.append({"loc": loc, "type": CODE.get(err.get("type"), "other:" + str(err.get("type")))})
        rows.sort(key=lambda r: (r["loc"], r["type"]))
        return rows
    return []


def methods_index():
    return {name: {"params": c.params.__name__, "result": c.result.__name__}
            for name, c in METHODS.items()}


def enums():
    out = {}
    for name in dir(live):
        obj = getattr(live, name)
        if isinstance(obj, type) and issubclass(obj, Enum) and obj.__module__ == live.__name__:
            out[name] = [member.value for member in obj]
    return out


def profile_scoped():
    return sorted(name for name, c in METHODS.items() if "profile" in c.params.model_fields)


def expected(req):
    func = req.get("func")
    method = req.get("method")
    if func == "methods":
        return methods_index()
    if func == "enums":
        return enums()
    if func == "profile_scoped":
        return profile_scoped()
    if func == "params_spec":
        if method not in METHODS:
            return "UNKNOWN_METHOD"
        return spec_of(METHODS[method].params)
    if func == "validate_params":
        if method not in METHODS:
            return "UNKNOWN_METHOD"
        rows = errors_of(METHODS[method].params, req.get("params"), "params")
        return {"ok": not rows, "errors": rows}
    if func == "normalize_params":
        if method not in METHODS:
            return "UNKNOWN_METHOD"
        model = METHODS[method].params
        rows = errors_of(model, req.get("params"), "params")
        if rows:
            return {"ok": False, "errors": rows}
        return {"ok": True, "params": model.model_validate(req.get("params")).model_dump(mode="json")}
    if func == "validate_result":
        if method not in METHODS:
            return "UNKNOWN_METHOD"
        rows = errors_of(METHODS[method].result, req.get("result"), "result")
        return {"ok": not rows, "errors": rows}
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            want = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if want != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), want, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
