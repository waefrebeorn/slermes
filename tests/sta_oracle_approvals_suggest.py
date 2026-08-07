"""Oracle for hermes_cli/approvals_suggest.py is_unsafe_class + derive_glob."""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT")
if DEV_ROOT and DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from hermes_cli.approvals_suggest import is_unsafe_class, derive_glob

cases_file = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__),
    "oracle", "fixtures", "approvals_suggest", "cases.in",
)

results = []
with open(cases_file) as f:
    cases = json.load(f)
for c in cases:
    op = c.get("op", "")
    val = c.get("value", "")
    if op == "is_unsafe_class":
        results.append(is_unsafe_class(val))
    elif op == "derive_glob":
        results.append(derive_glob(val))
    else:
        results.append(None)

# Emit results as JSON array (one per line for simple matching)
for r in results:
    if r is None:
        print("null")
    elif isinstance(r, bool):
        print("true" if r else "false")
    elif isinstance(r, str):
        print(r)
    else:
        print(r)
