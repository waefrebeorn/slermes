"""twin for gateway/platforms/qqbot/keyboards.py:parse_update_prompt_button_data (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/keyboards.py"
    PY_FN = "parse_update_prompt_button_data"
    C_FN = "cli_gateway_platforms_qqbot_keyboards_parse_update_prompt_button_data"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
