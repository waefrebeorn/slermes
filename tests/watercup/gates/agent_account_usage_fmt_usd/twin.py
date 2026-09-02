"""Twin for agent_account_usage_fmt_usd: _fmt_usd.

Mirrors Python agent/account_usage.py:_fmt_usd exactly.
Python _fmt_usd:
  - Accepts numeric types (int, float). Raises TypeError on None.
  - float('nan') -> '$nan'  (NOT an error in Python!)
  - String inputs: float() converts, then _fmt_usd on the float.
    'NaN' -> ValueError (float('NaN') works but _fmt_usd rejects str type)
    'hello' -> ValueError
    '' -> ValueError
    '1234.56' -> float=1234.56 -> _fmt_usd(1234.56) -> '$1,234.56'
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from watercup_py import Cup
from agent.account_usage import _fmt_usd

def trace(input):
    c = Cup("account_usage_fmt_usd")
    if input is None or input == "NULL":
        # Python _fmt_usd(None) -> TypeError
        c.valstr("None")
        return c.finalize()
    try:
        v = float(input)
    except (ValueError, TypeError):
        # float() failed -> ValueError (Python raises on non-numeric strings)
        c.valstr("None")
        return c.finalize()
    # At this point v is a float. Call _fmt_usd with the float.
    # Python _fmt_usd checks isinstance(v, (int, float)) - float passes.
    # But _fmt_usd also checks it's not a bool and is finite.
    try:
        rv = _fmt_usd(v)
        c.valstr(str(rv))
    except Exception:
        c.valstr("None")
    return c.finalize()

def run(inputs):
    return [trace(inp) for inp in inputs]
