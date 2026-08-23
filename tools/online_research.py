#!/usr/bin/env python3
"""
Online Research Module for Mixture of Agents (Hermes Python)

Provides web search capabilities for dynamic model selection and context enrichment.
- Multi-source web search (DuckDuckGo, Brave, Google CSE)
- Research quality scoring
- Dynamic totem pole reordering based on benchmark updates
- Caching with TTL
- Integration with MoA pipeline

Uses FREE search APIs where possible.
"""

from __future__ import annotations

import asyncio
import json
import hashlib
import os
import time
from dataclasses import dataclass, field
from typing import Any, Optional
from urllib.parse import quote_plus, urlencode

import aiohttp


@dataclass
class ResearchResult:
    """Single research result from a search query."""
    title: str
    url: str
    snippet: str
    source: str
    relevance_score: float = 0.0
    timestamp: float = field(default_factory=time.time)


@dataclass 
class ResearchQuery:
    """Research query with metadata."""
    query: str
    intent: str  # "benchmark_update", "model_comparison", "technical_fact", "general"
    num_results: int = 10
    sources: list[str] = field(default_factory=lambda: ["duckduckgo", "brave"])


@dataclass
class ResearchSummary:
    """Synthesized research summary for model selection."""
    query: str
    findings: list[ResearchResult]
    model_scores: dict[str, float]  # model_id -> research-informed score
    confidence: float
    timestamp: float
    cache_key: str


class ResearchCache:
    """Simple in-memory cache with TTL for research results."""
    
    def __init__(self, ttl_seconds: int = 3600):
        self.ttl = ttl_seconds
        self._cache: dict[str, ResearchSummary] = {}
    
    def _make_key(self, query: str, intent: str) -> str:
        return hashlib.sha256(f"{intent}:{query}".encode()).hexdigest()[:32]
    
    def get(self, query: str, intent: str) -> Optional[ResearchSummary]:
        key = self._make_key(query, intent)
        if key in self._cache:
            entry = self._cache[key]
            if time.time() - entry.timestamp < self.ttl:
                return entry
            else:
                del self._cache[key]
        return None
    
    def set(self, summary: ResearchSummary):
        self._cache[summary.cache_key] = summary
    
    def clear_expired(self):
        now = time.time()
        self._cache = {k: v for k, v in self._cache.items() if now - v.timestamp < self.ttl}


class OnlineResearcher:
    """
    Multi-source online research for MoA model selection and context enrichment.
    """
    
    def __init__(self, cache_ttl: int = 3600):
        self.cache = ResearchCache(cache_ttl)
        self.session: Optional[aiohttp.ClientSession] = None
        self.timeout = aiohttp.ClientTimeout(total=30)
    
    async def __aenter__(self):
        self.session = aiohttp.ClientSession(timeout=self.timeout)
        return self
    
    async def __aexit__(self, *args):
        if self.session:
            await self.session.close()
    
    async def search_duckduckgo(self, query: str, num_results: int = 10) -> list[ResearchResult]:
        """Search via DuckDuckGo HTML (no API key needed)."""
        url = f"https://html.duckduckgo.com/html/?q={quote_plus(query)}"
        headers = {"User-Agent": "Mozilla/5.0 (compatible; Hermes-MoA/1.0)"}
        
        try:
            async with self.session.get(url, headers=headers) as resp:
                html = await resp.text()
            
            # Parse HTML for results (simple extraction)
            results = []
            import re
            # Find result snippets
            pattern = r'<a class="result__url" href="([^"]+)".*?class="result__snippet">([^<]+)'
            for match in re.finditer(pattern, html, re.DOTALL):
                link = match.group(1)
                snippet = match.group(2)[:300]
                results.append(ResearchResult(
                    title=snippet[:80],
                    url=link,
                    snippet=snippet,
                    source="duckduckgo",
                    relevance_score=0.7
                ))
                if len(results) >= num_results:
                    break
            return results
        except Exception as e:
            print(f"[MOA-Research] DuckDuckGo search failed: {e}")
            return []
    
    async def search_brave(self, query: str, num_results: int = 10) -> list[ResearchResult]:
        """Search via Brave Search API (requires BRAVE_API_KEY)."""
        api_key = os.getenv("BRAVE_API_KEY")
        if not api_key:
            return []
        
        url = "https://api.search.brave.com/res/v1/web/search"
        params = {"q": query, "count": num_results}
        headers = {"Accept": "application/json", "X-Subscription-Token": api_key}
        
        try:
            async with self.session.get(url, params=params, headers=headers) as resp:
                if resp.status == 200:
                    data = await resp.json()
                    results = []
                    for item in data.get("web", {}).get("results", []):
                        results.append(ResearchResult(
                            title=item.get("title", ""),
                            url=item.get("url", ""),
                            snippet=item.get("description", "")[:300],
                            source="brave",
                            relevance_score=0.85
                        ))
                    return results
                else:
                    print(f"[MOA-Research] Brave search HTTP {resp.status}")
        except Exception as e:
            print(f"[MOA-Research] Brave search failed: {e}")
        return []
    
    async def search_google_cse(self, query: str, num_results: int = 10) -> list[ResearchResult]:
        """Search via Google Custom Search Engine (requires GOOGLE_CSE_KEY + GOOGLE_CSE_ID)."""
        api_key = os.getenv("GOOGLE_CSE_KEY")
        cse_id = os.getenv("GOOGLE_CSE_ID")
        if not api_key or not cse_id:
            return []
        
        url = "https://www.googleapis.com/customsearch/v1"
        params = {"key": api_key, "cx": cse_id, "q": query, "num": min(num_results, 10)}
        
        try:
            async with self.session.get(url, params=params) as resp:
                if resp.status == 200:
                    data = await resp.json()
                    results = []
                    for item in data.get("items", []):
                        results.append(ResearchResult(
                            title=item.get("title", ""),
                            url=item.get("link", ""),
                            snippet=item.get("snippet", "")[:300],
                            source="google_cse",
                            relevance_score=0.9
                        ))
                    return results
        except Exception as e:
            print(f"[MOA-Research] Google CSE search failed: {e}")
        return []
    
    async def research(self, query: str, intent: str = "general", num_results: int = 10, 
                       sources: list[str] = None) -> ResearchSummary:
        """
        Conduct multi-source research and synthesize findings.
        """
        if sources is None:
            sources = ["duckduckgo", "brave"]
        
        # Check cache
        cached = self.cache.get(query, intent)
        if cached:
            print(f"[MOA-Research] Cache hit for: {query[:60]}...")
            return cached
        
        print(f"[MOA-Research] Searching for: {query[:80]}...")
        
        # Parallel search across sources
        all_results = []
        
        tasks = []
        if "duckduckgo" in sources:
            tasks.append(self.search_duckduckgo(query, num_results))
        if "brave" in sources:
            tasks.append(self.search_brave(query, num_results))
        if "google_cse" in sources:
            tasks.append(self.search_google_cse(query, num_results))
        
        search_results = await asyncio.gather(*tasks, return_exceptions=True)
        
        for i, results in enumerate(search_results):
            if isinstance(results, list):
                all_results.extend(results)
            elif isinstance(results, Exception):
                print(f"[MOA-Research] Source {sources[i]} failed: {results}")
        
        # Deduplicate by URL
        seen_urls = set()
        unique_results = []
        for r in all_results:
            if r.url not in seen_urls:
                seen_urls.add(r.url)
                unique_results.append(r)
        
        # Score and rank results
        for r in unique_results:
            r.relevance_score = self._score_relevance(r, query, intent)
        
        unique_results.sort(key=lambda x: x.relevance_score, reverse=True)
        unique_results = unique_results[:num_results]
        
        # Synthesize model scores from research
        model_scores = self._extract_model_scores(unique_results, query)
        
        summary = ResearchSummary(
            query=query,
            findings=unique_results,
            model_scores=model_scores,
            confidence=min(1.0, len(unique_results) / 5.0),
            timestamp=time.time(),
            cache_key=self.cache._make_key(query, intent)
        )
        
        self.cache.set(summary)
        print(f"[MOA-Research] Found {len(unique_results)} results, model scores: {len(model_scores)}")
        return summary
    
    def _score_relevance(self, result: ResearchResult, query: str, intent: str) -> float:
        """Score research result relevance."""
        score = result.relevance_score
        query_lower = query.lower()
        text = f"{result.title} {result.snippet}".lower()
        
        # Boost for benchmark-related terms
        benchmark_terms = ["swe-bench", "terminal-bench", "livecodebench", "aa coding", 
                          "frontiermath", "aime", "gpqa", "hmmt", "mmlu", "ifeval"]
        for term in benchmark_terms:
            if term in text:
                score += 0.15
        
        # Boost for model names
        model_terms = ["nemotron", "kimi", "glm-5", "minimax", "qwen", "deepseek", 
                      "llama", "gemma", "phi", "mistral", "gpt-oss"]
        for term in model_terms:
            if term in text:
                score += 0.1
        
        # Boost for recent content indicators
        if any(y in text for y in ["2025", "2026", "latest", "new", "updated"]):
            score += 0.05
        
        return min(1.0, score)
    
    def _extract_model_scores(self, results: list[ResearchResult], query: str) -> dict[str, float]:
        """Extract model performance signals from research results."""
        scores = {}
        model_patterns = {
            "moonshotai/kimi-k2.6": ["kimi-k2.6", "kimi k2.6", "kimi k2"],
            "z-ai/glm-5": ["glm-5", "glm 5", "z.ai glm"],
            "minimaxai/minimax-m2.5": ["minimax-m2.5", "minimax m2.5", "minimax 2.5"],
            "nvidia/nemotron-3-ultra-550b-a55b": ["nemotron-3-ultra", "nemotron 3 ultra", "550b"],
            "nvidia/nemotron-4-340b-instruct": ["nemotron-4-340b", "nemotron 4 340b"],
            "nvidia/llama-3.1-nemotron-ultra-253b-v1": ["nemotron-ultra-253b", "nemotron ultra 253b"],
            "nvidia/nemotron-3-super-120b-a12b": ["nemotron-3-super", "nemotron 3 super", "120b"],
            "qwen/qwen3.5-397b-a17b": ["qwen3.5-397b", "qwen 3.5 397b"],
            "deepseek-ai/deepseek-v3.2": ["deepseek-v3.2", "deepseek v3.2"],
            "openai/gpt-oss-120b": ["gpt-oss-120b", "gpt oss 120b"],
            "meta/llama-3.1-405b-instruct": ["llama-3.1-405b", "llama 405b"],
        }
        
        for result in results:
            text = f"{result.title} {result.snippet}".lower()
            for model_id, patterns in model_patterns.items():
                for pattern in patterns:
                    if pattern in text:
                        # Extract any numeric scores mentioned near model name
                        import re
                        # Look for percentages, scores near model mention
                        context_start = max(0, text.find(pattern) - 100)
                        context_end = min(len(text), text.find(pattern) + len(pattern) + 100)
                        context = text[context_start:context_end]
                        
                        # Extract percentages like "58.6%", "80.2%", "89.6%"
                        percentages = re.findall(r'(\d+\.?\d*)%', context)
                        if percentages:
                            for p in percentages:
                                val = float(p)
                                if 0 <= val <= 100:
                                    # Normalize to 0-1
                                    scores[model_id] = max(scores.get(model_id, 0), val / 100.0)
                        else:
                            # Default boost for mention
                            scores[model_id] = max(scores.get(model_id, 0), 0.5)
                        break
        
        return scores


# Global researcher instance
_researcher: Optional[OnlineResearcher] = None


async def get_researcher() -> OnlineResearcher:
    global _researcher
    if _researcher is None:
        _researcher = OnlineResearcher()
        await _researcher.__aenter__()
    return _researcher


async def close_researcher():
    global _researcher
    if _researcher:
        await _researcher.__aexit__(None, None, None)
        _researcher = None


async def research_model_benchmarks(model_id: str) -> dict[str, float]:
    """Research current benchmark scores for a specific model."""
    researcher = await get_researcher()
    query = f"{model_id} benchmark SWE-Bench Terminal-Bench LiveCodeBench 2025 2026"
    summary = await researcher.research(query, intent="benchmark_update", num_results=10)
    return summary.model_scores


async def research_general(query: str, intent: str = "general") -> ResearchSummary:
    """General purpose research."""
    researcher = await get_researcher()
    return await researcher.research(query, intent=intent)


def reorder_totem_pole_by_research(current_order: list[str], 
                                    research_scores: dict[str, float],
                                    weight: float = 0.3) -> list[str]:
    """
    Reorder totem pole based on research scores.
    
    Args:
        current_order: Current model ordering (benchmark-based)
        research_scores: Model ID -> research-informed score (0-1)
        weight: How much to weight research vs original order (0-1)
    
    Returns:
        Reordered model list
    """
    # Create score map combining original position and research
    scored = []
    for i, model in enumerate(current_order):
        orig_score = 1.0 - (i / len(current_order))  # Higher = better position
        research_score = research_scores.get(model, 0.5)
        combined = (1 - weight) * orig_score + weight * research_score
        scored.append((model, combined))
    
    scored.sort(key=lambda x: x[1], reverse=True)
    return [m for m, s in scored]