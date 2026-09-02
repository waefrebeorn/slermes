"""Twin for gateway_acp_acp_is_sensitive_path: acp_is_sensitive_path from src/acp/edit_approval.c.

This is a slermes-native function (no Python equivalent in gateway/acp.py).
The twin uses the slermes C implementation via live parity — we skip the twin
and use a harness-only unit gate pattern.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from watercup_py import Cup

def trace(input):
    c = Cup("acp_is_sensitive_path")
    if input is None or input == "NULL":
        c.valstr("false")
    else:
        # Mirror the C logic: check for sensitive basenames and directories
        sensitive_basenames = {".env", ".git", ".ssh", ".hermes", 
                               "credentials.json", "secrets.json",
                               "htpasswd", "id_rsa", "id_ed25519"}
        sensitive_dirs = {".git", ".ssh"}
        
        import posixpath
        basename = posixpath.basename(input)
        if basename in sensitive_basenames:
            c.valstr("true")
            return c.finalize()
        
        parts = input.split("/")
        for part in parts:
            if part in sensitive_dirs:
                c.valstr("true")
                return c.finalize()
        
        c.valstr("false")
    return c.finalize()

def run(inputs):
    return [trace(inp) for inp in inputs]
