"""Oracle for tools/computer_use/cua_backend.py _wsl_windows_path_to_posix."""
import json, os, sys, re
from pathlib import PureWindowsPath

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT")
if DEV_ROOT and DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

cases_file = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__),
    "oracle", "fixtures", "cua_backend_helpers", "cases.in",
)

with open(cases_file) as f:
    cases = json.load(f)

for c in cases:
    path = c.get("value", "")
    is_wsl = c.get("is_wsl", False)
    if not re.match(r"^[A-Za-z]:[\\/]", path or ""):
        result = path
    elif not is_wsl:
        result = path
    else:
        win = PureWindowsPath(path)
        drive = (win.drive or "").rstrip(":").lower()
        if not drive:
            result = path
        else:
            result = os.path.join("/mnt", drive, *(str(p) for p in win.parts[1:]))
    print(result)
