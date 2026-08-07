"""Oracle for hermes_cli.agent_import pure helpers."""
import json, sys, os, tempfile
from pathlib import Path

DEV_ROOT = "/home/wubu/hermes-agent-dev"
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from hermes_cli.agent_import import (
    normalize_text, is_secret_key, claude_rule_to_command_pattern,
    extract_markdown_entries, sanitize_mcp_env,
    parse_existing_memory_entries, merge_entries,
)

cases_file = sys.argv[1] if len(sys.argv) > 1 else "cases.in"
with open(cases_file) as f:
    cases = json.load(f)

for c in cases:
    op = c.get("op", "")
    if op == "normalize_text":
        print(json.dumps(normalize_text(c.get("arg", ""))))
    elif op == "is_secret_key":
        print(json.dumps(is_secret_key(c.get("arg", ""))))
    elif op == "claude_rule_to_command_pattern":
        print(json.dumps(claude_rule_to_command_pattern(c.get("arg", ""))))
    elif op == "extract_markdown_entries":
        print(json.dumps(extract_markdown_entries(c.get("arg", ""))))
    elif op == "sanitize_mcp_env":
        env = json.loads(c.get("arg", "{}"))
        kept, stripped = sanitize_mcp_env(env)
        print(json.dumps({"kept": kept, "stripped": sorted(stripped)}))
    elif op == "parse_existing_memory_entries":
        with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False, encoding="utf-8") as f:
            f.write(c.get("arg", ""))
            path = f.name
        entries = parse_existing_memory_entries(Path(path))
        os.unlink(path)
        print(json.dumps(entries))
    elif op == "merge_entries":
        spec = c.get("arg", {})
        merged, stats = merge_entries(
            spec.get("existing", []), spec.get("incoming", []), spec["limit"])
        print(json.dumps({"merged": merged, "stats": stats}))
