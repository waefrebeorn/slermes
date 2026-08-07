"""Oracle for hermes_cli.agent_import pure helpers."""
import json, sys, os, importlib.util
from pathlib import Path

DEV_ROOT = "/home/wubu/hermes-agent-dev"
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

try:
    import yaml
except ImportError:
    yaml = None

from hermes_cli.agent_import import (
    normalize_text, is_secret_key, claude_rule_to_command_pattern,
    extract_markdown_entries, sanitize_mcp_env,
    parse_existing_memory_entries, merge_entries,
)

# Load the module without triggering the CLI import side-effects.
# We need default_source_dir, backup_memory_file, load_yaml_file,
# dump_yaml_file, detect_agents — which use Path.home() / filesystem.
_spec = importlib.util.spec_from_file_location(
    "agent_import_mod", os.path.join(DEV_ROOT, "hermes_cli/agent_import.py"))
agent_import_mod = importlib.util.module_from_spec(_spec)
if _spec and _spec.loader:
    _spec.loader.exec_module(agent_import_mod)

_ENTRY_DELIMITER = agent_import_mod.ENTRY_DELIMITER
_MEMORY_CHAR_LIMIT = agent_import_mod.MEMORY_CHAR_LIMIT

# Deterministic home for Path.home() calls.
# The runner substitutes @SBX@ → $TMPH in fixtures and sets SLERMES_HOME.
_DETERMINISTIC_HOME = os.environ.get("SLERMES_HOME") or os.environ.get("HOME") or str(Path.home())

def _home():
    return Path(_DETERMINISTIC_HOME)


def _default_source_dir(agent):
    """Port of default_source_dir using deterministic home."""
    dirs = agent_import_mod._AGENT_DEFAULT_DIRS
    return _home() / dirs[agent]


def _detect_agents():
    """Port of detect_agents using deterministic home."""
    result = []
    for a in agent_import_mod.SUPPORTED_AGENTS:
        d = _home() / agent_import_mod._AGENT_DEFAULT_DIRS[a]
        if d.is_dir():
            result.append(a)
    return result


def _backup_path(path_str, unix_ts=1700000000):
    """Port of backup_memory_file: builds <path>.bak.<ts>, returns None
    if the source file doesn't exist."""
    p = Path(path_str)
    if not p.exists():
        return None
    backup = p.with_suffix(p.suffix + f".bak.{unix_ts}")
    return str(backup)


cases_file = sys.argv[1] if len(sys.argv) > 1 else "cases.in"
with open(cases_file) as f:
    cases = json.load(f)

for c in cases:
    op = c.get("op", "")
    arg = c.get("arg", "")
    if op == "normalize_text":
        print(json.dumps(normalize_text(arg)))
    elif op == "is_secret_key":
        print(json.dumps(is_secret_key(arg)))
    elif op == "claude_rule_to_command_pattern":
        print(json.dumps(claude_rule_to_command_pattern(arg)))
    elif op == "extract_markdown_entries":
        print(json.dumps(extract_markdown_entries(arg)))
    elif op == "sanitize_mcp_env":
        env = json.loads(arg if isinstance(arg, str) else json.dumps(arg))
        kept, stripped = sanitize_mcp_env(env)
        print(json.dumps({"kept": kept, "stripped": sorted(stripped)}))
    elif op == "parse_existing_memory_entries":
        import tempfile
        with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False, encoding="utf-8") as f:
            f.write(arg)
            path = f.name
        entries = parse_existing_memory_entries(Path(path))
        os.unlink(path)
        print(json.dumps(entries))
    elif op == "merge_entries":
        spec = arg if isinstance(arg, dict) else json.loads(arg)
        merged, stats = merge_entries(
            spec.get("existing", []), spec.get("incoming", []), spec["limit"])
        print(json.dumps({"merged": merged, "stats": stats}))
    elif op == "load_yaml_file":
        try:
            data = yaml.safe_load(arg)
        except Exception:
            data = {}
        print(json.dumps(data if isinstance(data, dict) else {}))
    elif op == "dump_yaml_file":
        d = json.loads(arg) if isinstance(arg, str) else arg
        print(json.dumps(yaml.safe_dump(d, default_flow_style=False, sort_keys=False,
                                        allow_unicode=True).rstrip("\n")))
    elif op == "default_source_dir":
        print(json.dumps(str(_default_source_dir(arg))))
    elif op == "detect_agents":
        print(json.dumps(_detect_agents()))
    elif op == "backup_memory_file":
        print(json.dumps(_backup_path(arg)))
