"""twin for hermes_cli/default_soul.py:is_legacy_template_soul (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/default_soul.py"
    PY_FN = "is_legacy_template_soul"
    C_FN = "hermes_cli_default_soul_is_legacy"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
