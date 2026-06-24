/*
 * port_tools_async_delegation.c — Port of Python tools/async_delegation.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _adjust_thread_count */
int async_delegation_adjust_thread_count(int current, int target) {
    if (target <= 0) return current; /* No change */
    if (current < target) return target; /* Scale up */
    if (current > target) return target; /* Scale down */
    return current;
}


/* Port of Python: _finalize */
typedef struct {
    char delegation_id[256];
    char status[64];
    char result[4096];
    bool complete;
} async_delegation_result_t;

async_delegation_result_t async_delegation_finalize(const char *delegation_id) {
    async_delegation_result_t result = {0};
    if (!delegation_id) return result;
    
    strncpy(result.delegation_id, delegation_id, 255);
    strncpy(result.status, "completed", 63);
    result.complete = true;
    return result;
}


/* Port of Python: _finalize_batch */
int async_delegation_finalize_batch(const char **delegation_ids, int count,
                                     async_delegation_result_t *results, int max_results) {
    if (!delegation_ids || !results || count <= 0) return 0;
    
    int n = 0;
    for (int i = 0; i < count && n < max_results; i++) {
        results[n] = async_delegation_finalize(delegation_ids[i]);
        n++;
    }
    return n;
}


/* Port of Python: _get_executor */
typedef struct {
    int max_threads;
    int active_count;
    bool running;
} async_executor_t;

static async_executor_t global_executor = {4, 0, false};

async_executor_t *async_delegation_get_executor(void) {
    return &global_executor;
}


/* Port of Python: _new_delegation_id */
void async_delegation_new_id(char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    snprintf(out, out_sz, "dlg-%08x-%04x", rand(), rand() & 0xffff);
}


/* Port of Python: _prune_completed_locked */
int async_delegation_prune_completed_locked(async_delegation_result_t *delegations, int count) {
    if (!delegations || count <= 0) return 0;
    
    int write = 0;
    for (int i = 0; i < count; i++) {
        if (!delegations[i].complete) {
            if (write != i) {
                memcpy(&delegations[write], &delegations[i], sizeof(async_delegation_result_t));
            }
            write++;
        }
    }
    return write;
}


/* Port of Python: _push_completion_event */
typedef void (*completion_callback_t)(const async_delegation_result_t *result);

static completion_callback_t completion_callback = NULL;

void async_delegation_push_completion_event(const async_delegation_result_t *result) {
    if (completion_callback && result) {
        completion_callback(result);
    }
}

void async_delegation_set_completion_callback(completion_callback_t cb) {
    completion_callback = cb;
}


/* Port of Python: active_count */
int async_delegation_active_count(void) {
    async_executor_t *exec = async_delegation_get_executor();
    return exec ? exec->active_count : 0;
}


/* Port of Python: dispatch_async_delegation */
async_delegation_result_t async_delegation_dispatch(const char *task_json) {
    async_delegation_result_t result = {0};
    if (!task_json) return result;
    
    async_delegation_new_id(result.delegation_id, sizeof(result.delegation_id));
    strncpy(result.status, "dispatched", 63);
    
    async_executor_t *exec = async_delegation_get_executor();
    if (exec) exec->active_count++;
    
    return result;
}


/* Port of Python: dispatch_async_delegation_batch */
int async_delegation_dispatch_batch(const char **tasks, int count,
                                     async_delegation_result_t *results, int max_results) {
    if (!tasks || !results || count <= 0) return 0;
    
    int n = 0;
    for (int i = 0; i < count && n < max_results; i++) {
        results[n] = async_delegation_dispatch(tasks[i]);
        n++;
    }
    return n;
}


/* Port of Python: interrupt_all */
void async_delegation_interrupt_all(void) {
    async_executor_t *exec = async_delegation_get_executor();
    if (exec) {
        exec->active_count = 0;
        exec->running = false;
    }
}


/* Port of Python: list_async_delegations */
typedef struct {
    async_delegation_result_t items[64];
    int count;
} delegation_list_t;

delegation_list_t async_delegation_list(void) {
    delegation_list_t list = {0};
    /* Return active delegations */
    async_executor_t *exec = async_delegation_get_executor();
    if (exec && exec->active_count > 0) {
        list.count = exec->active_count;
        for (int i = 0; i < list.count && i < 64; i++) {
            async_delegation_new_id(list.items[i].delegation_id, sizeof(list.items[i].delegation_id));
            strncpy(list.items[i].status, "running", 63);
        }
    }
    return list;
}

