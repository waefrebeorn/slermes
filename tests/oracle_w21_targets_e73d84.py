#!/usr/bin/env python3
"""Differential oracle for gateway/outbound/targets.py (gate w21_targets_e73d84).

The live module runs unchanged. The four things it reaches for are doubles:
``gateway.platform_registry.platform_registry`` (the plugin entry the fixture
describes), ``gateway.config.Platform`` (the builtin membership set),
``gateway.channel_directory.resolve_channel_name`` and
``hermes_cli.plugins.discover_plugins``. ``gateway.platforms.send_targets`` is
imported for real, because the NOT_EXPLICIT sentinel must be the same object the
live code compares against.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

from gateway.platforms.send_targets import NOT_EXPLICIT  # noqa: E402

BUILTINS = ["telegram", "discord", "slack", "signal", "sms", "whatsapp", "matrix", "xmpp",
            "photon", "email"]


class Registry:
    def __init__(self):
        self.registered = True
        self.entry = None

    def is_registered(self, name):
        return self.registered

    def get(self, name):
        return self.entry


REGISTRY = Registry()


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class _Platform:
    def __init__(self, value):
        self.value = value


_mk("gateway.platform_registry", platform_registry=REGISTRY)
_mk("gateway.config", Platform=[_Platform(v) for v in BUILTINS])
_CHANNEL = _mk("gateway.channel_directory", resolve_channel_name=lambda platform, ref: None)
_mk("hermes_cli").__path__ = [Q + "/hermes_cli"]
_mk("hermes_cli.plugins", discover_plugins=lambda *a, **k: None)

SRC = Q + "/gateway/outbound/targets.py"
_spec = importlib.util.spec_from_file_location("w21_tgt_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["w21_tgt_live"] = live
_spec.loader.exec_module(live)


def _syntax_fn(parse):
    """target_ref_syntax_fn described by the fixture's parse object."""
    kind = (parse or {}).get("kind") if isinstance(parse, dict) else "absent"
    if kind in (None, "absent"):
        return None
    if kind == "not_explicit":
        return lambda ref: NOT_EXPLICIT
    if kind == "none":
        return lambda ref: None
    if kind == "tuple":
        return lambda ref: (parse.get("chat"), parse.get("thread"))
    if kind == "raises":
        def _boom(ref):
            raise RuntimeError("parser")
        return _boom
    if kind == "invalid":
        return lambda ref: "not-a-tuple"
    raise AssertionError("bad parser kind %r" % (kind,))


def _validate_fn(spec):
    if spec in (None, "none"):
        return None
    if spec == "true":
        return lambda chat: True
    if spec == "false":
        return lambda chat: False
    if spec == "raises":
        def _boom(chat):
            raise RuntimeError("validator")
        return _boom
    if isinstance(spec, str) and spec.startswith("str:"):
        text = spec[4:]
        return lambda chat: text
    raise AssertionError("bad validate spec %r" % (spec,))


def _entry(world):
    e = world.get("entry")
    if not isinstance(e, dict):
        return None
    entry = types.SimpleNamespace(source=e.get("source", "builtin"))
    entry.target_ref_syntax_fn = _syntax_fn(e.get("parse"))
    entry.parse_target_ref_fn = _syntax_fn(e.get("parse"))
    entry.validate_target_ref_fn = _validate_fn(e.get("validate", "none"))
    return entry


class _HintModule:
    """The install hint is a seam: platform_install_hint(platform_name)."""

    def __init__(self, hint):
        self.hint = hint or ""

    def __enter__(self):
        self.mod = _mk("hermes_cli.left_core_migration",
                       platform_install_hint=lambda name: self.hint)
        return self

    def __exit__(self, *exc):
        sys.modules.pop("hermes_cli.left_core_migration", None)
        return False


def expected(r):
    f = r.get("func")
    if f == "env_probe":
        sle = os.environ.get("SLERMES_HOME") or ""
        home = os.environ.get("HOME") or ""
        return {"sler": bool(sle), "home": bool(home), "same": bool(sle) and sle == home}
    platform = r.get("platform") or ""
    ref = r.get("target_ref") or ""
    if f == "parse_ref":
        parser = _syntax_fn(r.get("parser"))
        old = live._target_syntax_fn
        live._target_syntax_fn = lambda name: parser
        try:
            return list(live._parse_target_ref(platform, ref))
        finally:
            live._target_syntax_fn = old
    if f == "unknown_platform_error":
        with _HintModule(r.get("install_hint")):
            return live.unknown_platform_error(platform)
    if f == "resolve":
        world = r.get("world") if isinstance(r.get("world"), dict) else {}
        REGISTRY.entry = _entry(world)
        REGISTRY.registered = bool(world.get("registered", REGISTRY.entry is not None))
        resolved = world.get("resolve")

        def _resolve_name(name, target):
            if world.get("resolve_raises"):
                raise RuntimeError("directory down")
            return resolved

        _CHANNEL.resolve_channel_name = _resolve_name
        with _HintModule(world.get("install_hint")):
            return list(live.resolve_send_target(
                platform, ref, pass_unresolved_references=bool(r.get("pass_unresolved"))))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"parse_ref", "resolve", "unknown_platform_error", "env_probe"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE w21_targets_e73d84: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
