"""Twin for cli_accent_hex: _accent_hex."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), os.pardir))
from watercup_py import Cup

def trace(input):
    c = Cup("cl2_accent_hex")
    from cli import _accent_hex as live_fn
    rv = live_fn()
    c.valstr(rv if rv else "None")
    return c.finalize()

def run(inputs):
    return [trace(inp) for inp in inputs]
