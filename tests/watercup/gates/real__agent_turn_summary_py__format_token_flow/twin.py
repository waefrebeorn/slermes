"""twin: format_token_flow(output_tokens) -> spinner text."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/turn_summary.py"; PY_FN = "format_token_flow"
    C_FN = "agent_turn_summary_u_format_token_flow"
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
