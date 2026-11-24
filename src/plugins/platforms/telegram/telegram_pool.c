/*
 * telegram_pool.c -- real C11 port of the pool-generation accounting of
 * upstream Hermes plugins/platforms/telegram/telegram_pool.py: the descriptor
 * budget that caps a transport's connection pool, and the lease that decides
 * when a retired generation may close (idle now, at the last request, or at the
 * 3600 s grace bound). The transport itself and the socket close stay upstream.
 *
 * PoP: w20_telegrampo_0aa0d4_fd_bounded_limits @ plugins/platforms/telegram/telegram_pool.py:fd_bounded_limits
 * PoP: w20_telegrampo_0aa0d4_lease_ops @ plugins/platforms/telegram/telegram_pool.py:PoolGeneration.retire
 */
#include "hermes_json.h"
#include "noc_w20_telegrampo_0aa0d4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Every Telegram pool together may use at most 1/_FD_SHARE of RLIMIT_NOFILE:
 * two PTB clients, each with one pool per path. */
#define TP0_FD_SHARE 4
#define TP0_PTB_CLIENTS 2
#define TP0_MIN_POOL_CONNECTIONS 4

struct json_t *w20_telegrampo_0aa0d4_fd_bounded_limits(const struct json_t *request) {
    json_t *out = json_object();
    json_t *soft_value = json_obj_get(request, "soft");
    json_t *max_conn_value = json_obj_get(request, "max_connections");
    json_t *keepalive_value = json_obj_get(request, "max_keepalive_connections");
    json_t *expiry_value = json_obj_get(request, "keepalive_expiry");
    long long paths = (long long)json_get_num(request, "paths", 1);
    long long divisor, cap, requested;

    if (paths < 1) paths = 1;

    if (!soft_value || soft_value->type != JSON_NUMBER ||
        !max_conn_value || max_conn_value->type != JSON_NUMBER) {
        /* soft is None, or limits.max_connections is None: the same object */
        json_set(out, "capped", json_bool(0));
        json_set(out, "max_connections", max_conn_value ? json_copy(max_conn_value) : json_null());
        json_set(out, "max_keepalive_connections",
                 keepalive_value ? json_copy(keepalive_value) : json_null());
        json_set(out, "keepalive_expiry", expiry_value ? json_copy(expiry_value) : json_null());
        return out;
    }
    divisor = TP0_PTB_CLIENTS * paths;
    cap = (long long)soft_value->num_val / TP0_FD_SHARE / divisor;
    if (cap < TP0_MIN_POOL_CONNECTIONS) cap = TP0_MIN_POOL_CONNECTIONS;
    requested = (long long)max_conn_value->num_val;
    if (requested <= cap) {
        json_set(out, "capped", json_bool(0));
        json_set(out, "max_connections", json_copy(max_conn_value));
        json_set(out, "max_keepalive_connections",
                 keepalive_value ? json_copy(keepalive_value) : json_null());
        json_set(out, "keepalive_expiry", expiry_value ? json_copy(expiry_value) : json_null());
        return out;
    }
    json_set(out, "capped", json_bool(1));
    json_set(out, "max_connections", json_number((double)cap));
    if (!keepalive_value) {
        json_set(out, "max_keepalive_connections", json_number((double)cap));
    } else if (keepalive_value->type == JSON_NUMBER && (long long)keepalive_value->num_val < cap) {
        json_set(out, "max_keepalive_connections", json_copy(keepalive_value));
    } else {
        json_set(out, "max_keepalive_connections", json_number((double)cap));
    }
    json_set(out, "keepalive_expiry", expiry_value ? json_copy(expiry_value) : json_null());
    return out;
}

/* The PoolGeneration lease as a step machine over the op sequence. */
typedef struct {
    long long active;
    int       retired;
    int       closed;
    int       grace;
    long long inner_closes;
} tp0_lease;

static void tp0_close(tp0_lease *lease) {
    if (lease->closed) return;
    lease->closed = 1;
    lease->grace = 0;          /* the timer holds this generation; drop it once closed */
    lease->inner_closes += 1;  /* await self._inner.aclose() */
}

static void tp0_release(tp0_lease *lease) {
    lease->active -= 1;
    if (lease->retired && lease->active <= 0) tp0_close(lease);
}

static void tp0_retire(tp0_lease *lease) {
    lease->retired = 1;
    if (lease->active <= 0) {
        tp0_close(lease);
        return;
    }
    if (!lease->grace) lease->grace = 1;   /* call_later(RETIRE_GRACE_S, ...) */
}

struct json_t *w20_telegrampo_0aa0d4_lease_ops(const struct json_t *ops) {
    tp0_lease lease;
    json_t *out = json_object();
    size_t i;

    lease.active = 0;
    lease.retired = 0;
    lease.closed = 0;
    lease.grace = 0;
    lease.inner_closes = 0;

    if (ops && ops->type == JSON_ARRAY) {
        for (i = 0; i < ops->c.count; i++) {
            json_t *op = ops->c.items[i];
            const char *name;
            if (!op || op->type != JSON_STRING || !op->str_val) continue;
            name = op->str_val;
            if (!strcmp(name, "begin")) {
                lease.active += 1;
            } else if (!strcmp(name, "begin_fail")) {
                lease.active += 1;
                /* _close_unowned_connections(inner) finds no pool._requests here */
                tp0_release(&lease);
            } else if (!strcmp(name, "end") || !strcmp(name, "end_twice")) {
                tp0_release(&lease);   /* release_once: a second aclose is a no-op */
            } else if (!strcmp(name, "retire")) {
                tp0_retire(&lease);
            } else if (!strcmp(name, "grace")) {
                if (lease.grace) tp0_close(&lease);
            } else if (!strcmp(name, "aclose")) {
                tp0_retire(&lease);
            }
        }
    }
    json_set(out, "active", json_number((double)lease.active));
    json_set(out, "retired", json_bool(lease.retired));
    json_set(out, "closed", json_bool(lease.closed));
    json_set(out, "inner_closes", json_number((double)lease.inner_closes));
    json_set(out, "grace_armed", json_bool(lease.grace));
    return out;
}
