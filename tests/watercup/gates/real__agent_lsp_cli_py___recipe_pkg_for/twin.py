"""twin for agent/lsp/cli.py:_recipe_pkg_for (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/cli.py"
    PY_FN = "_recipe_pkg_for"
    C_FN = "_recipe_pkg_for"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
