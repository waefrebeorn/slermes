"""twin for tools/approval.py:_deobfuscate_shell_word_for_detection (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "_deobfuscate_shell_word_for_detection"
    C_FN = "_deobfuscate_shell_word_for_detection"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
