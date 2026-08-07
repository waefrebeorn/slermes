"""Oracle for hermes_cli/approvals_suggest.py parse_apply_indices.

Tests the parsing logic in isolation — imports the function and calls it
directly. The C port uses -1 for ValueError equivalents (None/error → null).
"""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT")
if DEV_ROOT and DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from hermes_cli.approvals_suggest import parse_apply_indices

cases_file = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__),
    "oracle", "fixtures", "approvals_suggest_parse_indices", "cases.in",
)

with open(cases_file) as f:
    cases = json.load(f)

for c in cases:
    spec = c.get("value", "")
    total = c.get("total", 10)
    try:
        result = parse_apply_indices(spec, total)
        print(result)
    except ValueError:
        print("null")
