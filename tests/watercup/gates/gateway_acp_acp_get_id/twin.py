"""Twin for gateway_acp_acp_get_id: acp_get_id from src/acp/server.c.

Mirrors Python acp_adapter/server.py ACPServer.get_id (no args, returns "acp-server-0").
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from watercup_py import Cup

def trace(input):
    c = Cup("acp_get_id")
    c.valstr("acp-server-0")
    return c.finalize()

def run(inputs):
    return [trace(inp) for inp in inputs]
