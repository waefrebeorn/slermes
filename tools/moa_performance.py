#!/usr/bin/env python3
"""
MoA Performance & Consistency Module

Adds:
- Connection pooling with persistent sessions
- Response caching with project awareness
- Thread pool reuse for parallel queries
- Project-level context/memory for long-goal consistency
- Persistent research cache
- Consistent model selection per project/session
"""

from __future__ import annotations

import asyncio
import json
import os
import time
import hashlib
import sqlite3
import threading
from dataclasses import dataclass, field
from typing import Any, Optional
from pathlib import Path
from contextlib import asynccontextmanager

import aiohttp


# ─── Project Context & Memory ─────────────────────────────────────────

@dataclass
class ProjectContext:
    """Project-level context for long-goal consistency."""
    project_id: str
    project_name: str
    working_dir: str
    goal: str = ""
    key_decisions: list[str] = field(default_factory=list)
    preferred_models: dict[str, str] = field(default_factory=dict)  # mode -> model_id
    session_history: list[dict] = field(default_factory=list)
    created_at: float = field(default_factory=time.time)
    updated_at: float = field(default_factory=time.time)
    
    def to_dict(self) -> dict:
        return {
            "project_id": self.project_id,
            "project_name": self.project_name,
            "working_dir": self.working_dir,
            "goal": self.goal,
            "key_decisions": self.key_decisions,
            "preferred_models": self.preferred_models,
            "session_history": self.session_history,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> "ProjectContext":
        return cls(**data)


class ProjectContextManager:
    """Manages project contexts with persistent storage."""
    
    def __init__(self, base_path: str = None):
        if base_path is None:
            base_path = os.path.join(os.path.expanduser("~"), ".hermes", "moa_projects")
        self.base_path = Path(base_path)
        self.base_path.mkdir(parents=True, exist_ok=True)
        self.db_path = self.base_path / "projects.db"
        self._init_db()
        self._cache: dict[str, ProjectContext] = {}
        self._lock = threading.RLock()
    
    def _init_db(self):
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS projects (
                    project_id TEXT PRIMARY KEY,
                    project_name TEXT NOT NULL,
                    working_dir TEXT NOT NULL,
                    goal TEXT,
                    key_decisions TEXT,  -- JSON array
                    preferred_models TEXT,  -- JSON object
                    session_history TEXT,  -- JSON array
                    created_at REAL,
                    updated_at REAL
                )
            """)
            conn.execute("""
                CREATE TABLE IF NOT EXISTS project_sessions (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    project_id TEXT,
                    mode TEXT,
                    prompt TEXT,
                    result_summary TEXT,
                    models_used TEXT,  -- JSON array
                    duration_seconds REAL,
                    timestamp REAL,
                    FOREIGN KEY (project_id) REFERENCES projects (project_id)
                )
            """)
            conn.execute("""
                CREATE INDEX IF NOT EXISTS idx_sessions_project ON project_sessions (project_id)
            """)
    
    def get_or_create_project(self, working_dir: Optional[str] = None, project_name: Optional[str] = None) -> ProjectContext:
        """Get existing project for working dir or create new one."""
        if working_dir is None:
            working_dir = os.getcwd()
        working_dir = os.path.abspath(working_dir)
        
        # Try to find existing project
        project_id = hashlib.sha256(working_dir.encode()).hexdigest()[:16]
        
        with self._lock:
            if project_id in self._cache:
                return self._cache[project_id]
            
            # Load from DB
            with sqlite3.connect(self.db_path) as conn:
                row = conn.execute(
                    "SELECT * FROM projects WHERE project_id = ?", (project_id,)
                ).fetchone()
                
                if row:
                    ctx = ProjectContext(
                        project_id=row[0],
                        project_name=row[1],
                        working_dir=row[2],
                        goal=row[3] or "",
                        key_decisions=json.loads(row[4] or "[]"),
                        preferred_models=json.loads(row[5] or "{}"),
                        session_history=json.loads(row[6] or "[]"),
                        created_at=row[7],
                        updated_at=row[8],
                    )
                else:
                    # Create new
                    if project_name is None:
                        project_name = os.path.basename(working_dir)
                    ctx = ProjectContext(
                        project_id=project_id,
                        project_name=project_name,
                        working_dir=working_dir,
                    )
                    self._save_project(ctx)
            
            self._cache[project_id] = ctx
            return ctx
    
    def _save_project(self, ctx: ProjectContext):
        ctx.updated_at = time.time()
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                INSERT OR REPLACE INTO projects 
                (project_id, project_name, working_dir, goal, key_decisions, preferred_models, session_history, created_at, updated_at)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                ctx.project_id, ctx.project_name, ctx.working_dir, ctx.goal,
                json.dumps(ctx.key_decisions), json.dumps(ctx.preferred_models),
                json.dumps(ctx.session_history), ctx.created_at, ctx.updated_at
            ))
    
    def update_project(self, ctx: ProjectContext):
        """Update project context and persist."""
        with self._lock:
            self._save_project(ctx)
            self._cache[ctx.project_id] = ctx
    
    def record_session(self, project_id: str, mode: str, prompt: str, 
                       result_summary: str, models_used: list[str], duration: float):
        """Record a MoA session for the project."""
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                INSERT INTO project_sessions 
                (project_id, mode, prompt, result_summary, models_used, duration_seconds, timestamp)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            """, (project_id, mode, prompt[:500], result_summary[:500], 
                  json.dumps(models_used), duration, time.time()))
    
    def get_project_history(self, project_id: str, limit: int = 20) -> list[dict]:
        """Get recent session history for a project."""
        with sqlite3.connect(self.db_path) as conn:
            rows = conn.execute("""
                SELECT mode, prompt, result_summary, models_used, duration_seconds, timestamp
                FROM project_sessions
                WHERE project_id = ?
                ORDER BY timestamp DESC
                LIMIT ?
            """, (project_id, limit)).fetchall()
            
            return [
                {
                    "mode": r[0], "prompt": r[1], "result_summary": r[2],
                    "models_used": json.loads(r[3]), "duration": r[4], "timestamp": r[5]
                }
                for r in rows
            ]
    
    def get_preferred_model(self, project_id: str, mode: str) -> Optional[str]:
        """Get preferred model for a mode in this project."""
        with self._lock:
            if project_id in self._cache:
                return self._cache[project_id].preferred_models.get(mode)
        return None
    
    def set_preferred_model(self, project_id: str, mode: str, model_id: str):
        """Set preferred model for a mode in this project."""
        with self._lock:
            if project_id in self._cache:
                self._cache[project_id].preferred_models[mode] = model_id
                self._save_project(self._cache[project_id])


# ─── Persistent Research Cache ────────────────────────────────────────

class PersistentResearchCache:
    """SQLite-backed research cache with project awareness."""
    
    def __init__(self, base_path: Optional[str] = None):
        if base_path is None:
            base_path = os.path.join(os.path.expanduser("~"), ".hermes", "moa_research")
        self.base_path = Path(base_path)
        self.base_path.mkdir(parents=True, exist_ok=True)
        self.db_path = self.base_path / "research_cache.db"
        self._init_db()
    
    def _init_db(self):
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS research_cache (
                    cache_key TEXT PRIMARY KEY,
                    query TEXT NOT NULL,
                    intent TEXT NOT NULL,
                    project_id TEXT,
                    findings TEXT,  -- JSON array
                    model_scores TEXT,  -- JSON object
                    confidence REAL,
                    created_at REAL,
                    expires_at REAL
                )
            """)
            conn.execute("""
                CREATE INDEX IF NOT EXISTS idx_research_project ON research_cache (project_id)
            """)
            conn.execute("""
                CREATE INDEX IF NOT EXISTS idx_research_expires ON research_cache (expires_at)
            """)
    
    def _make_key(self, query: str, intent: str, project_id: Optional[str] = None) -> str:
        combined = f"{project_id or 'global'}:{intent}:{query}"
        return hashlib.sha256(combined.encode()).hexdigest()[:32]
    
    def get(self, query: str, intent: str, project_id: Optional[str] = None, ttl_seconds: int = 3600) -> Optional[dict]:
        """Get cached research if not expired."""
        key = self._make_key(query, intent, project_id)
        now = time.time()
        
        with sqlite3.connect(self.db_path) as conn:
            row = conn.execute("""
                SELECT findings, model_scores, confidence, created_at
                FROM research_cache
                WHERE cache_key = ? AND expires_at > ?
            """, (key, now)).fetchone()
            
            if row:
                return {
                    "findings": json.loads(row[0] or "[]"),
                    "model_scores": json.loads(row[1] or "{}"),
                    "confidence": row[2],
                    "created_at": row[3],
                }
        return None
    
    def set(self, query: str, intent: str, findings: list, model_scores: dict, 
            confidence: float, project_id: Optional[str] = None, ttl_seconds: int = 3600):
        """Store research results."""
        key = self._make_key(query, intent, project_id)
        now = time.time()
        
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                INSERT OR REPLACE INTO research_cache
                (cache_key, query, intent, project_id, findings, model_scores, confidence, created_at, expires_at)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                key, query, intent, project_id,
                json.dumps(findings), json.dumps(model_scores),
                confidence, now, now + ttl_seconds
            ))
    
    def clear_expired(self):
        """Remove expired entries."""
        now = time.time()
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("DELETE FROM research_cache WHERE expires_at < ?", (now,))


# ─── Optimized HTTP Client with Connection Pooling ────────────────────

class OptimizedMoAHttpClient:
    """HTTP client with connection pooling, session reuse, and smart batching."""
    
    def __init__(self, 
                 timeout: int = 300,
                 max_connections: int = 20,
                 max_keepalive: int = 10,
                 keepalive_timeout: int = 30):
        self.timeout = aiohttp.ClientTimeout(total=timeout)
        self.connector: Optional[aiohttp.TCPConnector] = None
        self.session: Optional[aiohttp.ClientSession] = None
        self.max_connections = max_connections
        self.max_keepalive = max_keepalive
        self.keepalive_timeout = keepalive_timeout
        self._lock = asyncio.Lock()
    
    async def _ensure_session(self):
        """Lazy session creation with connection pooling."""
        if self.session is None or self.session.closed:
            async with self._lock:
                if self.session is None or self.session.closed:
                    self.connector = aiohttp.TCPConnector(
                        limit=self.max_connections,
                        limit_per_host=self.max_keepalive,
                        keepalive_timeout=self.keepalive_timeout,
                        enable_cleanup_closed=True,
                        force_close=False,
                    )
                    self.session = aiohttp.ClientSession(
                        timeout=self.timeout,
                        connector=self.connector,
                        headers={"User-Agent": "Hermes-MoA/1.0"}
                    )
    
    async def __aenter__(self):
        await self._ensure_session()
        return self
    
    async def __aexit__(self, *args):
        if self.session and not self.session.closed:
            await self.session.close()
        if self.connector:
            await self.connector.close()
    
    def _build_auth_header(self, provider) -> Optional[str]:
        api_key = os.getenv(provider.api_key_env)
        if not api_key:
            return None
        return f"Bearer {api_key}"
    
    async def call_model(
        self,
        provider,
        ref,
        system_prompt: str,
        user_prompt: str,
    ) -> Optional[str]:
        if not await provider_health.is_healthy(provider.name):
            return None
        
        await self._ensure_session()
        
        messages = []
        if system_prompt:
            messages.append({"role": "system", "content": system_prompt})
        messages.append({"role": "user", "content": user_prompt})
        
        payload = {
            "model": ref.model,
            "messages": messages,
            "temperature": ref.temperature,
            "max_tokens": ref.max_tokens,
            "stream": False,
        }
        
        if ref.reasoning_effort != "none":
            payload["reasoning"] = {"effort": ref.reasoning_effort, "enabled": True}
        
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json",
        }
        auth = self._build_auth_header(provider)
        if auth:
            headers["Authorization"] = auth
        
        url = f"{provider.base_url}/chat/completions"
        
        # Retry with exponential backoff
        max_retries = 3
        for attempt in range(max_retries):
            try:
                async with self.session.post(url, json=payload, headers=headers) as resp:
                    if resp.status == 200:
                        data = await resp.json()
                        content = self._extract_content(data)
                        if content:
                            await provider_health.record_success(provider.name)
                            return content
                    elif resp.status == 401:
                        print(f"[MOA] {provider.name} auth failed - check {provider.api_key_env}")
                        await provider_health.record_failure(provider.name)
                        return None
                    elif resp.status == 429:
                        wait = 2 * (attempt + 1)
                        print(f"[MOA] {provider.name} rate limited (attempt {attempt + 1}/{max_retries}), waiting {wait}s")
                        if attempt < max_retries - 1:
                            await asyncio.sleep(wait)
                        continue
                    else:
                        text = await resp.text()
                        print(f"[MOA] {provider.name} HTTP {resp.status}: {text[:200]}")
            except asyncio.TimeoutError:
                print(f"[MOA] {provider.name} timeout (attempt {attempt + 1}/{max_retries})")
            except Exception as e:
                print(f"[MOA] {provider.name} error: {e}")
            
            await provider_health.record_failure(provider.name)
            if attempt < max_retries - 1:
                await asyncio.sleep(1 * (attempt + 1))
        
        return None
    
    async def call_model_batch(
        self,
        requests: list[tuple],
        max_parallel: int = 8,
    ) -> list[Optional[str]]:
        """Execute multiple model calls in parallel with semaphore limiting."""
        await self._ensure_session()
        
        semaphore = asyncio.Semaphore(max_parallel)
        
        async def call_one(provider, ref, system_prompt, user_prompt):
            async with semaphore:
                return await self.call_model(provider, ref, system_prompt, user_prompt)
        
        tasks = [
            call_one(provider, ref, system_prompt, user_prompt)
            for provider, ref, system_prompt, user_prompt in requests
        ]
        
        results = await asyncio.gather(*tasks, return_exceptions=True)
        return [
            r if isinstance(r, str) else None
            for r in results
        ]
    
    def _extract_content(self, data: dict) -> Optional[str]:
        try:
            choices = data.get("choices", [])
            if choices:
                msg = choices[0].get("message", {})
                content = msg.get("content", "")
                if content:
                    return content
                # Handle reasoning content if present
                reasoning = msg.get("reasoning", {})
                if isinstance(reasoning, dict) and reasoning.get("content"):
                    return reasoning["content"]
            return None
        except Exception:
            return None


# ─── Response Cache ──────────────────────────────────────────────────

class MoAResponseCache:
    """Caches MoA responses for repeat queries with project awareness."""
    
    def __init__(self, base_path: Optional[str] = None, max_entries: int = 1000):
        if base_path is None:
            base_path = os.path.join(os.path.expanduser("~"), ".hermes", "moa_cache")
        self.base_path = Path(base_path)
        self.base_path.mkdir(parents=True, exist_ok=True)
        self.db_path = self.base_path / "response_cache.db"
        self.max_entries = max_entries
        self._init_db()
    
    def _init_db(self):
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS response_cache (
                    cache_key TEXT PRIMARY KEY,
                    prompt_hash TEXT NOT NULL,
                    mode TEXT NOT NULL,
                    project_id TEXT,
                    response TEXT NOT NULL,
                    models_used TEXT,  -- JSON array
                    created_at REAL,
                    expires_at REAL,
                    hit_count INTEGER DEFAULT 0
                )
            """)
            conn.execute("""
                CREATE INDEX IF NOT EXISTS idx_cache_prompt ON response_cache (prompt_hash, mode)
            """)
            conn.execute("""
                CREATE INDEX IF NOT EXISTS idx_cache_expires ON response_cache (expires_at)
            """)
    
    def _make_key(self, prompt: str, mode: str, project_id: Optional[str] = None, 
                  model_overrides: Optional[dict] = None) -> str:
        content = f"{project_id or 'global'}:{mode}:{prompt}"
        if model_overrides:
            content += f":{json.dumps(model_overrides, sort_keys=True)}"
        return hashlib.sha256(content.encode()).hexdigest()[:32]
    
    def get(self, prompt: str, mode: str, project_id: Optional[str] = None, 
            model_overrides: Optional[dict] = None, ttl_seconds: int = 86400) -> Optional[dict]:
        key = self._make_key(prompt, mode, project_id, model_overrides)
        now = time.time()
        
        with sqlite3.connect(self.db_path) as conn:
            row = conn.execute("""
                SELECT response, models_used, created_at, hit_count
                FROM response_cache
                WHERE cache_key = ? AND expires_at > ?
            """, (key, now)).fetchone()
            
            if row:
                conn.execute(
                    "UPDATE response_cache SET hit_count = hit_count + 1 WHERE cache_key = ?",
                    (key,)
                )
                return {
                    "response": row[0],
                    "models_used": json.loads(row[1] or "[]"),
                    "created_at": row[2],
                    "hit_count": row[3] + 1,
                }
        return None
    
    def set(self, prompt: str, mode: str, response: str, models_used: list[str],
            project_id: Optional[str] = None, model_overrides: Optional[dict] = None, ttl_seconds: int = 86400):
        key = self._make_key(prompt, mode, project_id, model_overrides)
        now = time.time()
        
        with sqlite3.connect(self.db_path) as conn:
            # Check size limit
            count = conn.execute("SELECT COUNT(*) FROM response_cache").fetchone()[0]
            if count >= self.max_entries:
                # Remove oldest entries
                conn.execute("""
                    DELETE FROM response_cache 
                    WHERE cache_key IN (
                        SELECT cache_key FROM response_cache 
                        ORDER BY hit_count ASC, created_at ASC 
                        LIMIT ?
                    )
                """, (count - self.max_entries + 10,))
            
            conn.execute("""
                INSERT OR REPLACE INTO response_cache
                (cache_key, prompt_hash, mode, project_id, response, models_used, created_at, expires_at, hit_count)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0)
            """, (
                key,
                hashlib.sha256(prompt.encode()).hexdigest()[:16],
                mode,
                project_id,
                response,
                json.dumps(models_used),
                now,
                now + ttl_seconds,
            ))


# ─── Consistent Model Selector ───────────────────────────────────────

class ConsistentModelSelector:
    """Selects models consistently per project/session based on history."""
    
    def __init__(self, project_manager: ProjectContextManager):
        self.project_manager = project_manager
    
    def get_model_for_mode(self, project_id: str, mode: str, 
                           available_models: list) -> Optional[object]:
        """Get preferred model for mode, or best available."""
        # Check project preference
        preferred = self.project_manager.get_preferred_model(project_id, mode)
        if preferred:
            # Find matching model in available
            for m in available_models:
                if f"{m.provider}:{m.model}" == preferred:
                    return m
        
        # Return highest-tier available model
        if available_models:
            return min(available_models, key=lambda m: m.benchmark_tier)
        return None
    
    def record_successful_model(self, project_id: str, mode: str, model):
        """Record a successful model for future preference."""
        model_id = f"{model.provider}:{model.model}"
        self.project_manager.set_preferred_model(project_id, mode, model_id)


# ─── Global Instances ─────────────────────────────────────────────────

_project_manager: Optional[ProjectContextManager] = None
_research_cache: Optional[PersistentResearchCache] = None
_response_cache: Optional["MoAResponseCache"] = None
_model_selector: Optional["ConsistentModelSelector"] = None
_http_client: Optional[OptimizedMoAHttpClient] = None


def get_project_manager() -> ProjectContextManager:
    global _project_manager
    if _project_manager is None:
        _project_manager = ProjectContextManager()
    return _project_manager


def get_research_cache() -> PersistentResearchCache:
    global _research_cache
    if _research_cache is None:
        _research_cache = PersistentResearchCache()
    return _research_cache


def get_response_cache() -> "MoAResponseCache":
    global _response_cache
    if _response_cache is None:
        _response_cache = MoAResponseCache()
    return _response_cache


def get_model_selector() -> "ConsistentModelSelector":
    global _model_selector
    if _model_selector is None:
        _model_selector = ConsistentModelSelector(get_project_manager())
    return _model_selector


def get_http_client() -> OptimizedMoAHttpClient:
    global _http_client
    if _http_client is None:
        _http_client = OptimizedMoAHttpClient()
    return _http_client


async def close_global_clients():
    """Close global HTTP client on shutdown."""
    global _http_client
    if _http_client:
        await _http_client.__aexit__(None, None, None)
        _http_client = None