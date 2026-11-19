"""twin for hermes_cli/config.py:_preserve_env_ref_templates (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "_preserve_env_ref_templates"
    C_FN = "_preserve_env_ref_templates"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
