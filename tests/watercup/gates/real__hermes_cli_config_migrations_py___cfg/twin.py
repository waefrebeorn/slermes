"""twin for hermes_cli/config_migrations.py:_cfg (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config_migrations.py"
    PY_FN = "_cfg"
    C_FN = "_cfg"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
