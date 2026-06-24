/*
 * port_agent_tool_executor.c — Port of Python agent/tool_executor.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _budget_for_agent */
typedef struct {
    int max_chars;
    int max_tokens;
} budget_config_t;

budget_config_t budget_for_agent(int context_length) {
    budget_config_t budget = {100000, 4096}; /* DEFAULT_BUDGET */
    
    if (context_length <= 0) return budget;
    
    /* Scale budget for small context models */
    if (context_length <= 65000) {
        budget.max_chars = context_length * 10; /* ~1 char per token */
        budget.max_tokens = context_length / 4;
    } else if (context_length <= 131000) {
        budget.max_chars = 200000;
        budget.max_tokens = 8192;
    } else {
        budget.max_chars = 400000;
        budget.max_tokens = 16384;
    }
    return budget;
}

