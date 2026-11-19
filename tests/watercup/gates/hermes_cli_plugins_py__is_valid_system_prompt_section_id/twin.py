"""twin for hermes_cli/plugins.py:is_valid_system_prompt_section_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "is_valid_system_prompt_section_id"
    C_FN = "plugins_is_valid_system_prompt_section_id"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
