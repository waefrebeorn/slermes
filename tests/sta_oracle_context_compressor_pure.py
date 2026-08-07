"""Oracle for agent/context_compressor.py pure helpers."""
import json, os, sys

DEV_ROOT = "/home/wubu/hermes-agent-dev"
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)
# Ensure the dev tree (with hermes_cli.__version__) shadows any stub.
sys.path.remove(DEV_ROOT) if False else None
sys.path.insert(0, DEV_ROOT)

# Avoid picking up stub hermes_cli from slermes/tests/
for p in list(sys.path):
    if 'slermes/tests' in p and 'hermes_cli' in p:
        sys.path.remove(p)

# Provide a stub version attr if import still fails on auxiliary deps
import importlib
try:
    import hermes_cli
    if not hasattr(hermes_cli, '__version__'):
        hermes_cli.__version__ = "0.dev"
except Exception:
    sys.modules['hermes_cli'] = type(sys)('hermes_cli')
    sys.modules['hermes_cli'].__version__ = "0.dev"

from agent.context_compressor import (
    _safe_int, _skill_pruned_marker, _extract_pruned_skill_names,
    _is_image_part, _content_has_images, _strip_images_from_content,
    _strip_image_parts_from_parts, _truncate_tool_call_args_json,
    _append_text_to_content, _template_visible_role,
    _fresh_compaction_message_copy,
)

OPS = {
    "safe_int": lambda a: _safe_int(a),
    "skill_pruned_marker": lambda a: _skill_pruned_marker(a),
    "extract_pruned_skill_names": lambda a: _extract_pruned_skill_names(a),
    "is_image_part": lambda a: _is_image_part(_parse_arg(a)),
    "content_has_images": lambda a: _content_has_images(_parse_arg(a)),
    "strip_images_from_content": lambda a: _strip_images_from_content(_parse_arg(a)),
    "strip_image_parts_from_parts": lambda a: _strip_image_parts_from_parts(_parse_arg(a)),
    "truncate_tool_call_args_json": lambda a: _truncate_tool_call_args_json(json.loads(a)["args"], json.loads(a)["head"]),
    "append_text_to_content": lambda a: _append_text_to_content(json.loads(a)["content"], json.loads(a)["text"], prepend=json.loads(a).get("prepend", False)),
    "template_visible_role": lambda a: _template_visible_role(_parse_arg(a)),
    "fresh_compaction_message_copy": lambda a: _fresh_compaction_message_copy(_parse_arg(a)),
}

def _parse_arg(a):
    """Parse arg as JSON, falling back to raw string if not JSON."""
    if not a:
        return None
    try:
        return json.loads(a)
    except (json.JSONDecodeError, TypeError):
        return a

def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_context_compressor_pure.py <cases.in>\n")
        return 1
    text = open(sys.argv[1]).read()
    cases = json.loads(text)
    for c in cases:
        op = c.get("op", "")
        arg = c.get("arg", "")
        fn = OPS.get(op)
        if fn is None:
            print("UNKNOWN_OP")
            continue
        try:
            result = fn(arg)
            print(json.dumps(result, sort_keys=True, ensure_ascii=False))
        except Exception as e:
            print(f"ERROR: {e}", file=sys.stderr)
            print("null")
    return 0

if __name__ == "__main__":
    sys.exit(main())
