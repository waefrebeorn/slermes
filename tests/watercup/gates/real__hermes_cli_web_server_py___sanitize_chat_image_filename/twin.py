"""twin for hermes_cli/web_server.py:_sanitize_chat_image_filename (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_sanitize_chat_image_filename"
    C_FN = "_sanitize_chat_image_filename"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
