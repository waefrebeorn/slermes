#!/usr/bin/env python3
"""Differential oracle for tools/terminal_tool_background.py (gate
w23_terminalto_ce074e).

``spawn_background_process`` is the ``terminal(background=true)`` launch path:
it resolves the cwd, spawns through the process registry, stamps gateway
routing, decides whether this session can be notified at all, registers the
completion watcher and returns the JSON result the model sees.  Its seams are
all outside the module, so the fixture scripts them:

  * ``tools.process_registry.process_registry`` -- the registry (spawn_local /
    spawn_via_env, pending_watchers, arm_heartbeat);
  * ``tools.terminal_tool``'s ``_resolve_command_cwd`` /
    ``_resolve_notification_flag_conflict`` / ``_redact_terminal_error_text``;
  * ``tools.environments.local._msys_to_windows_path`` and
    ``tools.terminal_tool_result._cwd_fallback_note``;
  * ``gateway.session_context`` (async_delivery_supported, get_session_env);
  * ``tools.environments.sandbox_scope.hosted_scope``;
  * ``agent.delegation_context.is_delegated_child_context``;
  * ``gateway.run._gateway_runner_ref`` -- the live gateway loop, which either
    arms the watcher or lets it wait in ``pending_watchers``.

Everything else -- the hint ladder, the CI-poller detector, the async-support
downgrade, the conflict note, the subagent/heartbeat rules, the exact result
keys and the note texts -- is the live code.
"""
import importlib.util
import json
import sys
import types
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tools" / "terminal_tool_background.py"
GATE = "w23_terminalto_ce074e"
SITECOPY = "C:/Users/eman5/hermes-agent/sitecopy"

for p in (str(QUARRY), SITECOPY):
    if p not in sys.path:
        sys.path.insert(0, p)

_spec = importlib.util.spec_from_file_location("ttb_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["ttb_live"] = live
_spec.loader.exec_module(live)

WANTED = {"ci_poller", "routing", "async_support", "watcher", "spawn", "yield"}

ROUTING = [a for a, _ in live._ROUTING_FIELDS]
PROC_ATTRS = ["watcher_platform"] + ROUTING


class Proc:
    """The process-registry session object, built from fixture facts."""

    def __init__(self, world):
        self.id = world.get("proc_id")
        self.pid = world.get("proc_pid")
        self.cwd = world.get("used_cwd")
        self.notify_on_complete = world.get("notify_on_complete", True)
        for attr in ["watcher_platform"] + ROUTING:
            if attr in world:
                setattr(self, attr, world[attr])

    def facts(self):
        return {attr: getattr(self, attr) for attr in PROC_ATTRS if hasattr(self, attr)}


class Registry:
    def __init__(self, world):
        self.world = world
        self.pending_watchers = []
        self.spawned = []
        self.session = None

    def _maybe_fail(self):
        err = self.world.get("spawn_error")
        if err:
            raise RuntimeError(err)

    def spawn_local(self, **kwargs):
        self._maybe_fail()
        self.spawned.append(dict(kind="local", **kwargs))
        return Proc(self.world)

    def spawn_via_env(self, **kwargs):
        self._maybe_fail()
        self.spawned.append(dict(kind="env", **kwargs))
        return Proc(self.world)

    def adopt_local(self, proc, **kwargs):
        self.spawned.append(dict(kind="adopt", **kwargs))
        self.session = Proc(self.world)
        return self.session

    def arm_heartbeat(self, proc, seconds):
        return self.world.get("heartbeat_ret", seconds)


class Runner:
    def __init__(self, armed):
        self.armed = armed
        self.watchers = []

    def arm_process_watcher(self, watcher):
        self.watchers.append(watcher)
        return bool(self.armed)


@contextmanager
def stubs(world, registry, runner):
    """Install every seam the module imports lazily."""

    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        return m

    platform = world.get("watcher_platform", "")
    envmap = {
        "HERMES_SESSION_PLATFORM": platform,
        "HERMES_SESSION_CHAT_ID": world.get("watcher_chat_id", ""),
        "HERMES_SESSION_USER_ID": world.get("watcher_user_id", ""),
        "HERMES_SESSION_USER_NAME": world.get("watcher_user_name", ""),
        "HERMES_SESSION_THREAD_ID": world.get("watcher_thread_id", ""),
        "HERMES_SESSION_MESSAGE_ID": world.get("watcher_message_id", ""),
        "HERMES_SESSION_ID": world.get("parent_session_id", ""),
    }

    def get_session_env(name, default=""):
        return envmap.get(name, default)

    def resolve_cwd(**_kwargs):
        return world.get("effective_cwd")

    def conflict(**kwargs):
        watch = kwargs.get("watch_patterns")
        if "conflict_watch" in world:
            watch = world.get("conflict_watch")
        return watch, world.get("conflict_note")

    def msys(path):
        return world.get("msys_path") or path

    installed = {
        "tools.process_registry": mod("tools.process_registry", process_registry=registry),
        "tools.terminal_tool": mod("tools.terminal_tool",
                                   _resolve_command_cwd=resolve_cwd,
                                   _resolve_notification_flag_conflict=conflict,
                                   _redact_terminal_error_text=lambda s: s),
        "tools.terminal_tool_result": mod("tools.terminal_tool_result",
                                          _cwd_fallback_note=lambda req: world.get("cwd_fallback_note", "")),
        "tools.environments.local": mod("tools.environments.local", _msys_to_windows_path=msys),
        "gateway.session_context": mod("gateway.session_context",
                                       async_delivery_supported=lambda: bool(world.get("async_supported")),
                                       get_session_env=get_session_env),
        "tools.environments.sandbox_scope": mod(
            "tools.environments.sandbox_scope",
            hosted_scope=lambda: object() if world.get("hosted") else None),
        "agent.delegation_context": mod("agent.delegation_context",
                                        is_delegated_child_context=lambda: bool(world.get("delegated"))),
        "gateway.run": mod("gateway.run",
                           _gateway_runner_ref=(lambda: runner) if runner is not None else (lambda: None)),
    }
    saved = {name: sys.modules.get(name) for name in installed}
    sys.modules.update(installed)
    try:
        yield envmap
    finally:
        for name, old in saved.items():
            if old is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = old


def do_routing(req):
    proc = Proc({})
    envmap = {
        "HERMES_SESSION_PLATFORM": req.get("platform", ""),
        "HERMES_SESSION_CHAT_ID": req.get("chat_id", ""),
        "HERMES_SESSION_USER_ID": req.get("user_id", ""),
        "HERMES_SESSION_USER_NAME": req.get("user_name", ""),
        "HERMES_SESSION_THREAD_ID": req.get("thread_id", ""),
        "HERMES_SESSION_MESSAGE_ID": req.get("message_id", ""),
        "HERMES_SESSION_ID": req.get("session_id", ""),
    }
    live._stamp_gateway_routing(proc, lambda name, default="": envmap.get(name, default))
    return proc.facts() or None


def do_async_support(req):
    world = dict(req.get("world") or {})
    registry = Registry(world)
    result = {}
    proc = Proc(world)
    with stubs(world, registry, None):
        notify, watch = live._apply_async_support(
            proc, result, req.get("notify_on_complete"), req.get("watch_patterns"))
    return {"notify": notify, "watch": watch, "result": result, "routing": proc.facts() or None}


def do_watcher(req):
    world = dict(req.get("world") or {})
    registry = Registry(world)
    runner = Runner(req.get("armed", False))
    proc = Proc(world)
    with stubs(world, registry, runner):
        live._register_completion_watcher(registry, proc, req.get("session_key"))
    return {"watcher": (runner.watchers or registry.pending_watchers or [None])[0],
            "armed": not registry.pending_watchers,
            "pending": len(registry.pending_watchers),
            "interval": getattr(proc, "watcher_interval", None)}


def do_spawn(req):
    world = dict(req.get("world") or {})
    registry = Registry(world)
    runner = Runner(world.get("arm_watcher", True))
    env = types.SimpleNamespace(env={"INHERITED": "1"}, host_cwd=world.get("env_host_cwd"))
    with stubs(world, registry, runner):
        text = live.spawn_background_process(
            command=req.get("command"), env=env, env_type=req.get("env_type", "local"),
            effective_task_id=req.get("effective_task_id", "t0"),
            task_id=req.get("task_id"), session_key=req.get("session_key", "sk"),
            workdir=req.get("workdir"), cwd=req.get("cwd", "/w"),
            effective_pty=bool(req.get("effective_pty", False)),
            cwd_key=req.get("cwd_key"),
            notify_on_complete=bool(req.get("notify_on_complete", False)),
            watch_patterns=req.get("watch_patterns"),
            approval_note=req.get("approval_note"),
            completion_output_chars=req.get("completion_output_chars", 0),
            pty_disabled_reason=req.get("pty_disabled_reason"),
            heartbeat_seconds=req.get("heartbeat_seconds", 0),
            persist_on_release=bool(req.get("persist_on_release", False)),
            mounted_host=req.get("mounted_host"),
            auto_backgrounded=bool(req.get("auto_backgrounded", False)),
        )
    return json.loads(text)


def do_yield(req):
    world = dict(req.get("world") or {})
    registry = Registry(world)
    runner = Runner(world.get("arm_watcher", True))
    with stubs(world, registry, runner):
        handler = live.yield_to_background_handler(
            command=req.get("command"), env_type=req.get("env_type", "local"),
            cwd=req.get("cwd"), effective_task_id=req.get("effective_task_id", "t0"),
            task_id=req.get("task_id"), session_key=req.get("session_key", "sk"))
        if handler is None:
            return None
        handler(object(), req.get("output_so_far", ""))
    return {"adopted": dict(registry.spawned[0]) if registry.spawned else None,
            "pending": len(registry.pending_watchers),
            "watcher": (runner.watchers or registry.pending_watchers or [None])[0],
            "notify_on_complete": getattr(registry.session, "notify_on_complete", None)}


def expected(req):
    func = req.get("func")
    if func == "ci_poller":
        return live._looks_like_homebrew_ci_poller(req.get("command", ""))
    if func == "routing":
        return do_routing(req)
    if func == "async_support":
        return do_async_support(req)
    if func == "watcher":
        return do_watcher(req)
    if func == "spawn":
        return do_spawn(req)
    if func == "yield":
        return do_yield(req)
    raise AssertionError("unknown func %r" % (func,))


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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
