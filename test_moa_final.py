#!/usr/bin/env python3
"""Test the Mixture of Agents tool with available API keys."""

import asyncio
import json
import os
import sys

sys.path.insert(0, '/home/wubu/hermes-agent-dev')

# Set the NOUS_API_KEY since it's the only one available
os.environ["NOUS_API_KEY"] = os.getenv("NOUS_API_KEY", "")

from tools.mixture_of_agents_tool import (
    mixture_of_agents_tool,
    mixture_of_agents_math,
    MoAMode,
    MOA_PROVIDERS,
)

async def test_moa():
    print("Testing Mixture of Agents tool...")
    print(f"Available providers: {len(MOA_PROVIDERS)}")
    for p in MOA_PROVIDERS:
        print(f"  - {p.name}: {len(p.free_models)} free models, priority={p.priority}, auth={p.requires_auth}")
    
    # Test with a simple prompt
    test_prompt = "Explain the difference between a list and a tuple in Python in 3 sentences."
    
    print(f"\n--- Testing mode: standard ---")
    try:
        result = await mixture_of_agents_tool(test_prompt, MoAMode.STANDARD)
        parsed = json.loads(result)
        print(f"Success: {parsed.get('success', False)}")
        if not parsed.get('success'):
            print(f"Error: {parsed.get('error')}")
        else:
            print(f"Reference responses: {len(parsed.get('reference_responses', []))}")
            agg = parsed.get('aggregator_response', '')
            print(f"Aggregator response preview: {agg[:200]}...")
            print(f"Provider health: {parsed.get('provider_health', {})}")
    except Exception as e:
        print(f"Exception: {e}")
        import traceback
        traceback.print_exc()
    
    print(f"\n--- Testing mode: devil_advocate ---")
    try:
        result = await mixture_of_agents_tool(test_prompt, MoAMode.DEVIL_ADVOCATE)
        parsed = json.loads(result)
        print(f"Success: {parsed.get('success', False)}")
        if parsed.get('success'):
            print(f"Reference responses: {len(parsed.get('reference_responses', []))}")
            agg = parsed.get('aggregator_response', '')
            print(f"Aggregator response preview: {agg[:200]}...")
    except Exception as e:
        print(f"Exception: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    asyncio.run(test_moa())