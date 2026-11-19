"""twin for hermes_cli/web_routers/tools.py:run_toolset_post_setup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/tools.py"
    PY_FN = "run_toolset_post_setup"
    C_FN = "run_toolset_post_setup"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
