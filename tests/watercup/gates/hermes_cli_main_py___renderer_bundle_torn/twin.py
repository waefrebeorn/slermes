"""twin for hermes_cli/main.py:_renderer_bundle_torn (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_renderer_bundle_torn"
    C_FN = "mn_renderer_bundle_torn"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
