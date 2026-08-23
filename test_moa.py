#!/usr/bin/env python3
"""Test Mixture of Agents with real API calls."""

import asyncio
import json
import time
import os
from tools.mixture_of_agents_tool import mixture_of_agents_tool, MoAMode

# Check which API keys are available
print("API Key Status:")
for key in ['NVIDIA_API_KEY', 'NVIDIA_CLOUD_API_KEY', 'OPENROUTER_API_KEY', 'NOUS_API_KEY']:
    val = os.getenv(key, '')
    print(f"  {key}: {'SET (' + str(len(val)) + ' chars)' if val else 'NOT SET'}")

# Quick test with a complex reasoning problem
async def test_moa():
    prompt = '''Solve this step by step: A train leaves Chicago at 60 mph heading east. Another train leaves New York at 80 mph heading west. The distance between Chicago and New York is 790 miles. When and where do they meet? Provide the exact time after departure and distance from Chicago.'''
    
    print('\n=== Testing STANDARD mode ===')
    start = time.time()
    result = await mixture_of_agents_tool(prompt, MoAMode.STANDARD)
    elapsed = time.time() - start
    data = json.loads(result)
    print(f'Time: {elapsed:.1f}s | Success: {data.get("success")} | Refs used: {data.get("references_used")}/{data.get("references_attempted")}')
    print(f'Aggregator: {data.get("aggregator")} on {data.get("aggregator_provider")}')
    print(f'Response preview: {data.get("response", "")[:300]}...')
    print(f'Provider health: {json.dumps(data.get("provider_health", {}), indent=2)}')
    
    print('\n=== Testing DEVIL_ADVOCATE mode ===')
    start = time.time()
    result = await mixture_of_agents_tool(prompt, MoAMode.DEVIL_ADVOCATE)
    elapsed = time.time() - start
    data = json.loads(result)
    print(f'Time: {elapsed:.1f}s | Success: {data.get("success")} | Refs used: {data.get("references_used")}')
    print(f'Response preview: {data.get("response", "")[:300]}...')
    
    print('\n=== Testing TREPIDATION mode ===')
    start = time.time()
    result = await mixture_of_agents_tool(prompt, MoAMode.TREPIDATION)
    elapsed = time.time() - start
    data = json.loads(result)
    print(f'Time: {elapsed:.1f}s | Success: {data.get("success")} | Refs used: {data.get("references_used")}')
    print(f'Response preview: {data.get("response", "")[:300]}...')
    
    print('\n=== Testing TOKEN_MAXX mode ===')
    start = time.time()
    result = await mixture_of_agents_tool(prompt, MoAMode.TOKEN_MAXX)
    elapsed = time.time() - start
    data = json.loads(result)
    print(f'Time: {elapsed:.1f}s | Success: {data.get("success")} | Refs used: {data.get("references_used")}')
    print(f'Response preview: {data.get("response", "")[:300]}...')

asyncio.run(test_moa())