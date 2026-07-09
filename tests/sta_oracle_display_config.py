"""
sta_oracle_display_config.py — oracle for port_gateway_display_config (_normalise).

The C caller (resolve_display_setting) only invokes _normalise for JSON_STRING
values, so we feed the LIVE Python _normalise a *string* value (matching the C
contract) and compare the C's returned string to Python's logical result,
rendered as a string the JSON layer would carry:
  - Python returns a bool (True/False) -> render "true"/"false"
  - Python returns a string -> use it directly
"""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from gateway.display_config import _normalise

def py_to_c_str(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    return str(v)

lines = [l for l in sys.stdin if l.strip().startswith("{")]
mism = 0
for ln in lines:
    rec = json.loads(ln)
    setting = rec["setting"]; value = rec["value"]; got = rec["out"]
    exp = py_to_c_str(_normalise(setting, value))
    if got != exp:
        mism += 1
        print(f"MISMATCH setting={setting!r} value={value!r} got={got!r} exp={exp!r}")
print(f"DISPLAY_CONFIG oracle: {len(lines)} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
