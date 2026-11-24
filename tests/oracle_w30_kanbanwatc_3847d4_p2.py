#!/usr/bin/env python3
"""Differential oracle for gateway/kanban_watchers_notifier.py -- gate w30_kanbanwatc_3847d4_p2.

Everything under comparison is what the notifier decides about a *subscription*
without touching the world: which profiles/platforms a gateway pre-filters
(``_Collector.__init__``), ``_platform_names``, route authorization
(``_adapter_for_subscription``), ``_chat_type_of``, ``_wake_scope_id``, the two
warn-once dedupe decisions, ``build_wake_text`` plus the diagnostic/non-diagnostic
wake split, the send-failure counter step (``delivery_failed``), and the
post-delivery tail of ``deliver``.

Seams that stay in the caller and never run here:
  * the DB claim / cursor writes -- ``delivery_failed`` is driven with a runner
    that only records which of unsub/rewind it asked for;
  * ``profile_routes[].matches()`` -- each route answers the two queries the
    live code makes (tight then relaxed) from the fixture's flags;
  * ``_multiplex_profile_homes`` -- ``config._runtime_profile_homes`` is the
    served list, so no profile directory is read;
  * ``session_owned_by_profile`` -- one boolean;
  * the adapter registry, ``adapter.scope_id_for_chat``, and the logger.
"""
import asyncio
import importlib.util
import json
import logging
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_kanbanwatc_3847d4_p2"
TRUTH = QUARRY / "gateway" / "kanban_watchers_notifier.py"

os.environ["USERPROFILE"] = os.environ.get("USERPROFILE") or str(REPO.parent)
os.environ["HOME"] = str(REPO.parent)
os.environ["HERMES_LANGUAGE"] = "en"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))  # stub helper only
from hermes_seam_stubs import install as _install_seams  # noqa: E402
_install_seams()

_spec = importlib.util.spec_from_file_location("kwn_live_%s" % GATE, TRUTH)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

from agent import i18n  # noqa: E402

i18n.reset_language_cache()
if i18n.get_language() != "en":
    raise SystemExit("oracle needs the en catalog, got %r" % (i18n.get_language(),))

logging.getLogger("gateway.run").setLevel(logging.CRITICAL)


class Ev:
    def __init__(self, d):
        d = d if isinstance(d, dict) else {}
        self.kind = d.get("kind") or ""
        self.payload = d.get("payload")
        self.id = d.get("id")


def mk_task(d):
    if not isinstance(d, dict):
        return None
    return SimpleNamespace(
        title=d.get("title"), assignee=d.get("assignee"), result=d.get("result"),
        task_id=d.get("task_id"), status=d.get("status"), session_id=d.get("session_id"),
    )


def notif(req, events=None):
    sub = req.get("sub") if isinstance(req.get("sub"), dict) else {}
    return live._KanbanNotification(
        None,
        {"sub": sub, "task": mk_task(req.get("task")), "board": req.get("board"),
         "events": list(events or [])},
        platform_cls=None, sub_fail_counts={},
    )


class FakeEnum:
    """A Platform-like enum member: only ``.value`` is meaningful."""

    def __init__(self, value):
        self.value = value

    def __str__(self):
        return "Platform.%s" % str(self.value).upper()


def plat_for(entry):
    if isinstance(entry, dict):
        return FakeEnum(entry.get("value"))
    return entry


class FakeRoute:
    """Route whose two match queries answer from the fixture's flags."""

    def __init__(self, d):
        self.profile = d.get("profile")
        self.enabled = bool(d.get("enabled", True))
        self.platform = plat_for(d.get("platform"))
        self.chat_id = d.get("chat_id")
        self.guild_id = d.get("guild_id")
        self.user_id = d.get("user_id")
        self._plain = bool(d.get("plain", False))
        self._relaxed = bool(d.get("relaxed", False))
        self._calls = 0

    def matches(self, platform, guild_id=None, chat_id=None, thread_id=None,
                parent_chat_id=None, user_id=None):
        self._calls += 1
        return self._plain if self._calls == 1 else self._relaxed


class FakeRunner:
    def __init__(self, req):
        self._profile_adapters = {
            name: {plat_for(p): 1 for p in (plist or [])}
            for name, plist in (req.get("profile_adapters") or {}).items()
        }
        self.adapters = {}
        self.config = SimpleNamespace(
            multiplex_profiles=bool(req.get("multiplex")),
            profile_routes=[FakeRoute(r) for r in (req.get("routes") or [])],
            _runtime_profile_homes=[(n, Path("/tmp") / n) for n in (req.get("served") or [])],
        )
        self._kanban_notifier_profile = req.get("notifier_profile")
        self._primary_profile_name = req.get("primary_profile")
        self._adapter = None

    def _owns_kanban_dispatcher_lock(self):
        return False

    def _active_profile_name(self):
        return self._primary_profile_name

    def _authorization_adapter(self, platform, profile):
        return self._adapter


def f_platform_names(req):
    mapping = {}
    for entry in req.get("mapping") or []:
        mapping[plat_for(entry)] = 1
    return sorted(live._platform_names(mapping))


def f_collect_scope(req):
    runner = FakeRunner(req)
    runner.adapters = {plat_for(p): 1 for p in (req.get("adapters") or [])}
    c = live._Collector(runner, None, notifier_profile=req.get("notifier_profile"),
                        gc_due=False, gc_retention_days=0)
    return {"notifier_profiles": sorted(c.notifier_profiles,
                                        key=lambda v: (v is not None, str(v))),
            "active_platforms": sorted(c.active_platforms)}


def f_chat_type_of(req):
    md = req.get("metadata")
    md = md if isinstance(md, dict) else {}
    return live._chat_type_of(req.get("sub") or {}, md)


def f_wake_scope_id(req):
    sub = req.get("sub") if isinstance(req.get("sub"), dict) else {}
    resolver = req.get("resolver")
    adapter = SimpleNamespace()
    if isinstance(resolver, dict) and resolver.get("present"):
        adapter.scope_id_for_chat = lambda chat_id: resolver.get("value")
    return live._wake_scope_id(adapter, sub)


def warn_case(req, registry, fn):
    sub = req.get("sub") or {}
    platform_obj = plat_for(req.get("platform"))
    registry.clear()
    for row in req.get("warned") or []:
        registry.add(tuple(row))
    key = (sub.get("task_id"), getattr(platform_obj, "value", platform_obj),
           sub.get("chat_id"), sub.get("thread_id") or "")
    before = key in registry
    fn(sub, platform_obj)
    after = key in registry
    return {"warn": (not before) and after, "key": list(key) if after else None}


def f_anchorless_warn(req):
    return warn_case(req, live._ANCHORLESS_WARNED, live._warn_anchorless_thread_sub_once)


def f_unroutable_warn(req):
    return warn_case(req, live._UNROUTABLE_WARNED,
                     lambda sub, plat: live._warn_unroutable_sub_once(
                         sub, plat, "kanban notifier: %s on %s chat %s: %s", "pinned"))


def f_resolve_adapter(req):
    raw_plat = req.get("platform")
    plat = raw_plat if isinstance(raw_plat, FakeEnum) else FakeEnum(raw_plat)
    primary_obj = SimpleNamespace(tag="primary")
    kind = req.get("adapter") or "absent"
    adapter_obj = {"absent": None, "other": SimpleNamespace(tag="other")}.get(kind, primary_obj)
    runner = FakeRunner(req)
    runner._adapter = adapter_obj
    runner.adapters = {plat: primary_obj} if req.get("has_primary", True) else {}
    owner = req.get("owner_profile")
    resolved_profile = owner or req.get("notifier_profile") or req.get("primary_profile")
    runner._profile_adapters = {
        resolved_profile: {plat_for(p): 1 for p in (req.get("own_platforms") or [])}
    }
    warned = []
    live._warn_unroutable_sub_once = (
        lambda sub, platform, message, *extra: warned.append((sub, platform, extra)))
    live.session_owned_by_profile = (
        lambda config, profile, chat: bool(req.get("api_server_owned", False)))
    sub = req.get("sub") if isinstance(req.get("sub"), dict) else {}
    got = live._adapter_for_subscription(runner, plat, sub, owner)
    result = "absent" if got is None else ("primary" if got is primary_obj else "other")
    warn = None
    if warned:
        _, platform_obj, extra = warned[0]
        warn = {
            "key": [sub.get("task_id"), getattr(platform_obj, "value", platform_obj),
                    sub.get("chat_id"), sub.get("thread_id") or ""],
            "profile": extra[0] if extra else None,
            "route_profile": extra[1] if len(extra) > 1 else None,
        }
    return {"adapter": result, "warn": warn}


def f_wake_plan(req):
    raw = req.get("events") if isinstance(req.get("events"), list) else []
    events = [Ev(e) for e in raw]
    n = notif(req, events)
    n.is_push_adapter = bool(req.get("is_push_adapter", True))
    original = n.d["events"]
    if req.get("split"):
        groups = ([ev for ev in original if live.diagnostic_event(ev)],
                  [ev for ev in original if not live.diagnostic_event(ev)])
    else:
        groups = (original,)
    payloads = []
    for evs in groups:
        if not evs:
            continue
        n.d = {**n.d, "events": list(evs)}
        n.wake_handoff = n.wake_review_detail = ""
        for ev in evs:
            n.format_event(ev)
        n.build_wake_text()
        if n.wake_kinds:
            payloads.append({"synth": n.synth, "diagnostic": n.wake_diagnostic,
                             "kinds": [k for k in live._WAKE_KINDS if k in n.wake_kinds]})
    n.d = {**n.d, "events": original}
    n.runner = SimpleNamespace(config=SimpleNamespace(multiplex_profiles=bool(req.get("multiplex"))))
    return {"groups": payloads, "synth": n.synth, "session_key": n.session_key,
            "wake_kinds": [k for k in live._WAKE_KINDS if k in n.wake_kinds],
            "wake_diagnostic": getattr(n, "wake_diagnostic", None),
            "served_profile": n._served_wake_profile()}


def f_failure_step(req):
    sub = req.get("sub") if isinstance(req.get("sub"), dict) else {}
    n = live._KanbanNotification(
        None, {"sub": sub, "task": None, "board": "main", "events": [],
               "cursor": 7, "old_cursor": 3},
        platform_cls=None, sub_fail_counts={})
    count = int(req.get("count", 0))
    if count:
        n.sub_fail_counts[n.sub_key] = count
    actions = []
    n.runner = SimpleNamespace(
        _kanban_unsub=lambda *a, **k: actions.append("unsub"),
        _kanban_rewind=lambda *a, **k: actions.append("rewind"),
    )
    asyncio.run(n.delivery_failed(
        "kanban notifier: send failed for %s on %s (attempt %d/%d): %s",
        (n.task_id, n.platform_str),
        "kanban notifier: dropping subscription %s on %s after %d consecutive send failures",
        RuntimeError("boom"), False))
    return {"fails": count + 1, "action": actions[-1] if actions else None,
            "count_after": n.sub_fail_counts.get(n.sub_key)}


def f_after_delivery(req):
    task = mk_task(req.get("task"))
    is_push = bool(req.get("is_push", True))
    return {"advance": True, "clear_failures": not is_push,
            "unsub": bool(task and task.status == "archived")}


WANTED = {
    "platform_names", "collect_scope", "chat_type_of", "wake_scope_id",
    "anchorless_warn", "unroutable_warn", "resolve_adapter", "wake_plan",
    "failure_step", "after_delivery",
}

FUNCS = {
    "platform_names": f_platform_names,
    "collect_scope": f_collect_scope,
    "chat_type_of": f_chat_type_of,
    "wake_scope_id": f_wake_scope_id,
    "anchorless_warn": f_anchorless_warn,
    "unroutable_warn": f_unroutable_warn,
    "resolve_adapter": f_resolve_adapter,
    "wake_plan": f_wake_plan,
    "failure_step": f_failure_step,
    "after_delivery": f_after_delivery,
}


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
            py = FUNCS[r["func"]](r)
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
