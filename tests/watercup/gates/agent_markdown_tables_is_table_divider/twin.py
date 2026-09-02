"""Twin for agent_markdown_tables_is_table_divider: is_table_divider from agent/markdown_tables.py."""
import sys, os, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_md", os.path.join(DEV, "agent/markdown_tables.py"))
_mod = ilu.module_from_spec(_spec)
sys.modules["wc_live_md"] = _mod
try:
    _spec.loader.exec_module(_mod)
    LIVE = _mod.is_table_divider
except Exception:
    LIVE = None

def trace(input_str):
    c = Cup("is_table_divider")
    if input_str == "NULL":
        c.valstr("false")
        return c.finalize()
    if LIVE:
        try:
            result = LIVE(input_str)
        except Exception:
            result = False
    else:
        result = False
    c.valstr(str(result).lower())
    return c.finalize()

def run(inputs):
    return [trace(inp) for inp in inputs]
