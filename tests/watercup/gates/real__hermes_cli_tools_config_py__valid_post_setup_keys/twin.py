"""twin for hermes_cli/tools_config.py:valid_post_setup_keys (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "valid_post_setup_keys"
    C_FN = "valid_post_setup_keys"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
