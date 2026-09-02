"""Twin for agent_account_usage_is_finite_num: _is_finite_num."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from watercup_py import Cup
from agent.account_usage import _is_finite_num

def trace(input):
    c = Cup("account_usage_is_finite_num")
    if input is None or input == "NULL":
        c.valstr("false")
    elif input == "NaN":
        c.valstr("false")
    else:
        try:
            v = float(input)
            rv = _is_finite_num(v)
        except (ValueError, TypeError):
            rv = _is_finite_num(input)
        c.valstr("true" if rv else "false")
    return c.finalize()

def run(inputs):
    return [trace(inp) for inp in inputs]
