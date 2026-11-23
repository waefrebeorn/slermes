#!/usr/bin/env python3
'''Differential oracle for gate w14_legacyloca_18b83d (session_rules/legacy_local.py).'''
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_legacyloca_18b83d"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = sys.modules.get(".".join(parts[:i]))
        if parent is not None and not hasattr(parent, parts[i]):
            setattr(parent, parts[i], mod)
    return mod


for _pkg in ("persistence", "persistence.authority", "persistence.core"):
    if _pkg not in sys.modules:
        _seed(_pkg)


class AuthorityRefusal(Exception):
    def __init__(self, reason):
        super().__init__(reason)
        self.reason = reason


class OwnerRule:
    def __init__(self, host_subject, login_providers, messaging_labels):
        self.host_subject = host_subject
        self.login_providers = frozenset(login_providers)
        self.messaging_labels = frozenset(messaging_labels)


_seed("persistence.authority.vocabulary",
      LEGACY_CREATION_SOURCES={"desktop": "gui"},
      LEGACY_LOCAL_SOURCES=frozenset({"cli","tui","gui","desktop","oneshot","tool","acp"}),
      NOT_CLIENT_LABELS=frozenset({"cron","subagent","kanban","bot_room","a2a","webhook","api_server",
                                   "import","recovered","curator","batch","homeassistant","webui","local","unknown"}),
      OwnerRule=OwnerRule)
_seed("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)

SRC = QUARRY + "/session_rules/legacy_local.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def rule_of(r):
    d = r.get("rule")
    if d is None:
        return None
    return OwnerRule(d.get("host_subject", ""), d.get("login_providers", []), d.get("messaging_labels", []))


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    f = r.get("func")
    rule = rule_of(r)
    try:
        if f == "parse": return live.parse_authenticated_subject(r.get("subject"))
        if f == "login_label": return live.login_label(r.get("subject"))
        if f == "same_owner": return live.same_owner(r.get("stored"), r.get("subject"), rule)
        if f == "owners_agree": return live.owners_agree(r.get("first"), r.get("second"), rule)
        if f == "binding_names": return live.binding_names(r.get("saved"), r.get("subject"), rule)
        if f == "bindings_agree": return live.bindings_agree(r.get("first"), r.get("second"), rule)
        if f == "client_label": return live.client_label(r.get("source"), rule)
        if f == "legacy_history_row": return live.legacy_history_row(r.get("row") or {}, rule)
        if f == "legacy_creation_source": return live.legacy_creation_source(r.get("row") or {}, rule)
        if f == "legacy_account_label": return live.legacy_account_label(r.get("row") or {})
        if f == "login_owned_by": return live.login_owned_by(r.get("row") or {}, r.get("subject"), rule)
        if f == "validate_legacy_row":
            return live.validate_legacy_row(r.get("own_profile"), r.get("row") or {},
                                            subject=r.get("subject"), rule=rule)
    except AuthorityRefusal as exc:
        return {"error": exc.reason}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
