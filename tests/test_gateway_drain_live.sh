#!/usr/bin/env bash
# test_gateway_drain_live.sh — Live SIGTERM drain test for the gateway.
# Starts the real slermes binary in webhook mode (no external API keys needed),
# waits for startup, sends SIGTERM, and verifies the graceful drain sequence
# runs and the process exits. In-flight work gets the full restart_drain_timeout
# (default 30s) plus a 5s interrupt grace, so the wait window is generous.
set -euo pipefail
cd /home/wubu/hermes-agent-dev/slermes

BINARY=./slermes
WEBHOOK_PORT=18573
WEBHOOK_URL="http://127.0.0.1:${WEBHOOK_PORT}"
LOGFILE=/tmp/gw_drain_test.log
MAX_WAIT=15      # seconds to wait for startup
DRAIN_WAIT=60    # seconds to wait for graceful exit (30s drain + 5s grace + slack)

echo "=== Gateway Drain Live Test ==="

# 1. Start gateway in webhook mode (single platform, no API key needed).
#    SLERMES_WEBHOOK_PORT selects the listen port (see gw_setup.c:get_webhook_port).
echo "[1/5] Starting slermes gateway (webhook)..."
rm -f "$LOGFILE"
SLERMES_WEBHOOK_PORT="$WEBHOOK_PORT" OPENROUTER_API_KEY=dummy $BINARY gateway --platform webhook >"$LOGFILE" 2>&1 &
GW_PID=$!
echo "[1/5] PID=$GW_PID, webhook port=$WEBHOOK_PORT"

# 2. Wait for startup (poll the webhook port)
echo "[2/5] Waiting for gateway to start..."
started=0
for i in $(seq 1 $MAX_WAIT); do
    if kill -0 "$GW_PID" 2>/dev/null; then
        : # process alive, check port
    else
        echo "    process died early — log:"
        cat "$LOGFILE"
        exit 1
    fi
    # check if webhook port is listening
    if (echo > /dev/tcp/127.0.0.1/$WEBHOOK_PORT) 2>/dev/null; then
        started=1
        echo "    gateway listening on $WEBHOOK_URL"
        break
    fi
    sleep 1
done
if [ "$started" != "1" ]; then
    echo "    TIMEOUT waiting for startup — log:"
    cat "$LOGFILE"
    kill -9 "$GW_PID" 2>/dev/null || true
    exit 1
fi

# 3. Send SIGTERM (graceful drain trigger)
echo "[3/5] Sending SIGTERM to PID $GW_PID..."
kill -TERM "$GW_PID"

# 4. Wait for graceful exit within the drain window
echo "[4/5] Waiting for graceful drain+exit (up to ${DRAIN_WAIT}s)..."
exited=0
for i in $(seq 1 $((DRAIN_WAIT * 2))); do
    if ! kill -0 "$GW_PID" 2>/dev/null; then
        exited=1
        break
    fi
    sleep 0.5
done

if [ "$exited" != "1" ]; then
    echo "FAIL: gateway did not exit within ${DRAIN_WAIT}s — killing"
    kill -9 "$GW_PID" 2>/dev/null || true
    echo "--- Log tail ---"
    tail -20 "$LOGFILE"
    exit 1
fi

# 5. Verify the graceful drain sequence appeared in the log
echo "[5/5] Verifying graceful shutdown sequence..."
if grep -q "Shutdown requested (SIGTERM) — beginning drain" "$LOGFILE" && \
   grep -q "Shutdown complete" "$LOGFILE"; then
    echo "PASS: gateway drained and shut down gracefully on SIGTERM"
    echo "--- Log tail ---"
    tail -12 "$LOGFILE"
    exit 0
else
    echo "FAIL: graceful drain markers missing from log"
    echo "--- Log tail ---"
    tail -20 "$LOGFILE"
    exit 1
fi
