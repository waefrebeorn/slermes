#!/usr/bin/env bash
# sync-upstream.sh — Pull latest Python source from upstream NousResearch/hermes-agent
# Usage: ./scripts/sync-upstream.sh
#
# This script:
# 1. Fetches the latest upstream/main from NousResearch/hermes-agent
# 2. Merges into our fork (waefrebeorn/hermes-agent)
# 3. Resolves Python file conflicts by accepting upstream's version
# 4. Updates the slermes gitlink reference
# 5. Pushes to our fork (or you can do it manually)
#
# The slermes C11 repo at /home/wubu/hermes-agent-dev/slermes/ is an independent
# git repo (separate .git, remote → waefrebeorn/slermes) and is NOT affected by
# upstream Python changes. It survives as a gitlink (mode 160000).
#
# We are a special sunflower 🌻 — a C11 fork that benefits from upstream Python
# improvements by porting them, but we NEVER push C11 code back to upstream.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARENT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
RED='\033[0;31m'
NC='\033[0m'

echo ""
echo -e "${CYAN}🌻 Slermes — Sync Upstream${NC}"
echo ""

cd "$PARENT_DIR"

# ── 1. Fetch upstream ────────────────────────────────────────────────────
echo -e "${CYAN}→${NC} Fetching upstream (NousResearch/hermes-agent)..."
git fetch upstream 2>&1 | tail -3

# ── 2. Check how far behind we are ───────────────────────────────────────
BEHIND=$(git rev-list --count main..upstream/main 2>/dev/null || echo "0")
echo -e "  Behind upstream: ${CYAN}${BEHIND}${NC} commits"

if [ "$BEHIND" -eq "0" ]; then
    echo -e "  ${GREEN}✓${NC} Already up to date with upstream."
    exit 0
fi

# ── 3. Merge upstream ────────────────────────────────────────────────────
echo -e "${CYAN}→${NC} Merging upstream/main into main..."
git merge upstream/main --no-edit 2>&1 || true

# ── 4. Auto-resolve conflicts (take upstream for Python files) ────────────
CONFLICTS=$(git diff --name-only --diff-filter=U 2>/dev/null || true)
if [ -n "$CONFLICTS" ]; then
    echo -e "  ${YELLOW}⚠${NC} Conflicts in $(echo "$CONFLICTS" | wc -l) files"
    echo -e "${CYAN}→${NC} Resolving by accepting upstream's version..."
    for f in $CONFLICTS; do
        git checkout --theirs "$f" 2>/dev/null
        git add "$f"
        echo -e "  ${GREEN}✓${NC} Resolved: $f (theirs)"
    done
fi

# ── 5. Commit merge ──────────────────────────────────────────────────────
PRE_COMMIT_ALLOW_NO_CONFIG=1 git commit --no-edit 2>&1 || true

# ── 6. Update slermes gitlink ─────────────────────────────────────────────
echo -e "${CYAN}→${NC} Updating slermes gitlink reference..."
PRE_COMMIT_ALLOW_NO_CONFIG=1 git add slermes 2>/dev/null || true
PRE_COMMIT_ALLOW_NO_CONFIG=1 git commit -m "update slermes gitlink" 2>/dev/null || true

# ── 7. Push ───────────────────────────────────────────────────────────────
echo ""
echo -e "${CYAN}→${NC} Pushing to waefrebeorn/hermes-agent..."
git push origin main 2>&1 | tail -3

# ── 8. Summary ────────────────────────────────────────────────────────────
AHEAD=$(git rev-list --count upstream/main..main 2>/dev/null || echo "?")
BEHIND_NOW=$(git rev-list --count main..upstream/main 2>/dev/null || echo "0")
echo ""
echo -e "${GREEN}✓${NC} Sync complete!"
echo -e "  Ahead of upstream: ${CYAN}${AHEAD}${NC} commits (our C11 changes)"
echo -e "  Behind upstream:  ${CYAN}${BEHIND_NOW}${NC} commits (synced)"
echo ""
echo -e "  ${YELLOW}💡${NC} Next: Check which new upstream Python features need C11 porting"
echo -e "       Run: git log --oneline upstream/main ^$(git merge-base HEAD upstream/main 2>/dev/null || echo 'HEAD~30') | head -50"
echo ""

# ── 9. Check for PoP gaps ────────────────────────────────────────────────
echo -e "${CYAN}→${NC} Recent upstream changes for PoP consideration:"
git log --oneline upstream/main..main --first-parent 2>/dev/null | head -5 | while read -r line; do
    echo -e "  ${YELLOW}📝${NC} $line"
done
echo ""
