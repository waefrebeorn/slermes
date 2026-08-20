"""sta_oracle_plugins_toolsets.py — oracle for the hermes_cli/plugins.py and
hermes_cli/tools_config.py pure-logic ports.

Replays the SAME fixture directives the C harness (tests/t_port_plugins_toolsets.c)
consumes, calling the LIVE Python functions, and emits one JSON record per
directive in the identical shape. The runner diffs the two streams.
"""
import json
import os
import sys
import types


def _bind_live_python():
    """Put an IMPORTABLE live Hermes source tree on sys.path.

    Two trees exist on this machine and they are not interchangeable: the
    developer tree (~/hermes-agent-dev) and the installed tree
    (~/.hermes/hermes-agent). A tree whose hermes_cli/plugins.py imports a
    symbol its own hermes_constants.py does not export is internally stale and
    cannot be the oracle's twin, so probe each candidate and bind the first one
    that actually imports. Fail loudly rather than silently oracle-ing nothing.
    """
    # The runner overrides $HOME for isolation, so expanduser("~") points at the
    # temp home. Resolve the REAL account home from the password database.
    import pwd as _pwd
    try:
        real_home = _pwd.getpwuid(os.getuid()).pw_dir
    except Exception:
        real_home = os.path.expanduser("~")
    # Under the runner's isolated HOME, $HOME/.hermes/hermes-agent is a symlink
    # back to the developer tree; resolve both real candidates explicitly.
    candidates = [
        os.environ.get("ORACLE_PY_ROOT") or "",
        os.path.join(real_home, ".hermes", "hermes-agent"),
        os.path.join(real_home, "hermes-agent-dev"),
        os.path.realpath(os.path.join(os.environ.get("HOME", real_home),
                                      ".hermes", "hermes-agent")),
    ]
    errors = []
    for root in candidates:
        if not root or not os.path.isdir(root):
            continue
        if not os.path.exists(os.path.join(root, "hermes_cli", "plugins.py")):
            continue
        sys.path.insert(0, root)
        for mod in [m for m in list(sys.modules)
                    if m == "hermes_cli" or m.startswith("hermes_cli.")
                    or m in ("hermes_constants", "utils")]:
            del sys.modules[mod]
        try:
            import hermes_cli.plugins  # noqa: F401
            import hermes_cli.tools_config  # noqa: F401
            return root
        except Exception as exc:      # stale/broken tree — try the next one
            errors.append(f"{root}: {exc}")
            sys.path.remove(root)
    raise SystemExit("no importable live Hermes tree found:\n  " +
                     "\n  ".join(errors))


_bind_live_python()

from hermes_cli import plugins as P            # noqa: E402
from hermes_cli import tools_config as TC      # noqa: E402


def out(op, arg, value):
    print(json.dumps({"op": op, "arg": arg, "value": value}))


def load_order(spec):
    """resolve_plugin_load_order over manifests built from
    "key:name:dep1,dep2;key2:name2:" — real PluginManifest instances so the
    live graphlib path runs unmodified."""
    manifests = {}
    for chunk in spec.split(";"):
        if not chunk:
            continue
        parts = chunk.split(":")
        if len(parts) < 2:
            continue
        key, name = parts[0], parts[1]
        reqs = [r for r in (parts[2].split(",") if len(parts) > 2 else []) if r]
        manifests[key] = P.PluginManifest(
            name=name, key=key,
            requires_plugins=[{"id": r} for r in reqs],
        )
    return list(P.resolve_plugin_load_order(manifests))


def scan_directory(rel, source):
    """PluginManager._scan_directory returns List[PluginManifest]."""
    from pathlib import Path
    fixdir = os.environ.get("ORACLE_FIXDIR", ".")
    mgr = P.PluginManager()
    manifests = mgr._scan_directory(Path(fixdir) / rel, source)
    rows = []
    for m in manifests:
        rows.append({
            "key": getattr(m, "key", "") or "",
            "name": getattr(m, "name", "") or "",
            "source": getattr(m, "source", "") or "",
            "kind": getattr(m, "kind", "") or "",
            "portable": bool(getattr(m, "portable", False)),
        })
    return rows


def skill_dirs(rel):
    fixdir = os.environ.get("ORACLE_FIXDIR", ".")
    base = os.path.join(fixdir, rel)
    if not os.path.isdir(base):
        return []
    found = []
    for entry in os.listdir(base):
        d = os.path.join(base, entry)
        if os.path.isdir(d) and os.path.exists(os.path.join(d, "SKILL.md")):
            found.append(entry)
    return sorted(found)


def cache_roundtrip(keys_csv, mcp_csv):
    """Drive the REAL zero-arg _persist_plugin_toolset_keys by supplying its
    two data sources, then read the cache back through
    _read_plugin_keys_cache. No re-implementation of the write path."""
    keys = [k for k in keys_csv.split(",") if k]
    mcp = [m for m in mcp_csv.split(",") if m]

    orig_toolsets = P.get_plugin_toolsets
    orig_manager = P.get_plugin_manager
    try:
        P.get_plugin_toolsets = lambda: [(k, "", "") for k in keys]
        fake = types.SimpleNamespace(
            get_portable_mcp_servers=lambda: {m: {} for m in mcp}
        )
        P.get_plugin_manager = lambda: fake
        P._persist_plugin_toolset_keys()
    finally:
        P.get_plugin_toolsets = orig_toolsets
        P.get_plugin_manager = orig_manager

    blob = P._read_plugin_keys_cache()
    if blob is None:
        return None
    # Compact serialization matching C's json_serialize (no spaces).
    return json.dumps(blob, separators=(",", ":"))


def apply_toolset_change(config_json, spec):
    parts = spec.split(":")
    platform = parts[0]
    action = parts[1] if len(parts) > 1 else "enable"
    names = [n for n in (parts[2].split(",") if len(parts) > 2 else []) if n]
    config = json.loads(config_json)
    TC._apply_toolset_change(config, platform, names, action)
    # Compact serialization matching C's json_serialize (no spaces after : or ,).
    return json.dumps(config, separators=(",", ":"))


def restorable_dep(name):
    got = TC.restorable_python_tool_dependency(name)
    if not got:
        return None
    module, args = got
    return {"module": module, "install_args": list(args)}


def format_sections(spec):
    """format_system_prompt_sections reads item.id / item.content."""
    sections = []
    for chunk in spec.split("|"):
        if "=" not in chunk:
            continue
        sid, body = chunk.split("=", 1)
        sections.append(types.SimpleNamespace(id=sid, content=body))
    return P.format_system_prompt_sections(sections)


def main():
    fixture = sys.argv[1]
    with open(fixture, encoding="utf-8") as fh:
        for line in fh:
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            op = fields[0]
            a1 = fields[1] if len(fields) > 1 else ""
            a2 = fields[2] if len(fields) > 2 else ""

            if op == "valid_section_id":
                out(op, a1, bool(P.is_valid_system_prompt_section_id(a1)))
            elif op == "format_section":
                out(op, a1, P.format_system_prompt_section(a1, a2))
            elif op == "format_sections":
                out(op, a1, format_sections(a1))
            elif op == "detect_kind":
                out(op, a1, P._detect_kind_from_source(a1))
            elif op == "skill_namespace":
                out(op, a1, P._portable_skill_namespace(a1))
            elif op == "load_order":
                out(op, a1, load_order(a1))
            elif op == "web_caps":
                out(op, a1, list(TC.web_provider_capabilities(a1)))
            elif op == "checklist_keys":
                out(op, a1, sorted(TC._checklist_toolset_keys(a1)))
            elif op == "known_platforms":
                out(op, a1, sorted(TC._known_tool_platforms()))
            elif op == "effective_toolsets":
                rows = []
                for key, label, desc in TC._get_effective_configurable_toolsets():
                    rows.append({"key": key, "label": label,
                                 "description": desc})
                out(op, a1, rows)
            elif op == "apply_toolset_change":
                out(op, a2, apply_toolset_change(a1, a2))
            elif op == "reconfigure_enabled":
                out(op, a1, bool(TC._toolset_enabled_for_reconfigure(
                    a1, json.loads(a2))))
            elif op == "post_setup_keys":
                out(op, a1, sorted(TC.valid_post_setup_keys()))
            elif op == "restorable_dep":
                out(op, a1, restorable_dep(a1))
            elif op == "detect_active_provider":
                providers = json.loads(a1)
                out(op, a1, TC._detect_active_provider_index(providers, {}))
            elif op == "scan_directory":
                out(op, a1, scan_directory(a1, a2))
            elif op == "skill_dirs":
                out(op, a1, skill_dirs(a1))
            elif op == "cache_roundtrip":
                out(op, a1, cache_roundtrip(a1, a2))
            else:
                out("UNKNOWN_OP", op, None)


if __name__ == "__main__":
    main()
