"""twin for (policy — consumer of tool_classify_error; self-contained parity gate):dispatch_with_retry."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_(policy — consumer of tool_classify_error; self_contained parity gate)", os.path.join(DEV, "(policy — consumer of tool_classify_error; self-contained parity gate)"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_(policy — consumer of tool_classify_error; self_contained parity gate)"] = _pb
_spec.loader.exec_module(_pb)


def trace(inp):
    c = Cup("dispatch_with_retry")
    try:
        result = _pb.dispatch_with_retry(inp)
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
