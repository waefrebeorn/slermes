"""twin for hermes_cli/cli_billing_mixin.py:_billing_poll_charge."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_hermes_cli_cli_billing_mixin_py", os.path.join(DEV, "hermes_cli/cli_billing_mixin.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_hermes_cli_cli_billing_mixin_py"] = _pb
_spec.loader.exec_module(_pb)


def trace(inp):
    c = Cup("_billing_poll_charge")
    try:
        result = _pb._billing_poll_charge(inp)
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
