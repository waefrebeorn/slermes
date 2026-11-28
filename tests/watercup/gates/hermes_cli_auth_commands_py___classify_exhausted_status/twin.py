"""twin for hermes_cli/auth_commands.py:_classify_exhausted_status."""
from watercup_twin_runtime import TwinBase


class _E:
    def __init__(self, code, reason, message):
        self.last_error_code = code
        self.last_error_reason = reason
        self.last_error_message = message


class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth_commands.py"
    PY_FN = "_classify_exhausted_status"
    C_FN = "hermes_cli_auth_commands_u_classify_exhausted_status"

    def call(self, inp, inputs, idx=0):
        parts = str(inp).split("|||")
        while len(parts) < 3:
            parts.append("")
        code_s, reason, message = parts[0], parts[1], parts[2]
        code = int(code_s) if code_s.strip() else None
        return self._fn(_E(code, reason, message))

    def norm(self, c, result):
        label, retry = result
        c.valstr("%s|%s" % (label, "1" if retry else "0"))


_t = _Twin(); trace = _t.trace; run = _t.run
