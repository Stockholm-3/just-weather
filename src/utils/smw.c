#include "smw.h"

#include <string.h>

smw_t g_smw;

int smw_init() {

    for (int i = 0; i < SMW_MAX_TASKS; i++) {
        g_smw.tasks[i].context  = NULL;
        g_smw.tasks[i].callback = NULL;
    }
    return 0;
}

smw_task_t* smw_create_task(void* context,
                            void (*callback)(void* context, uint64_t montime)) {
    for (int i = 0; i < SMW_MAX_TASKS; i++) {
        if (g_smw.tasks[i].callback == NULL && g_smw.tasks[i].context == NULL) {
            g_smw.tasks[i].context  = context;
            g_smw.tasks[i].callback = callback;
            return &g_smw.tasks[i];
        }
    }
    return NULL; // No available task slot
}

void smw_destroy_task(smw_task_t* task) {
    int i;
    for (i = 0; i < SMW_MAX_TASKS; i++) {
        if (&g_smw.tasks[i] == task) {
            g_smw.tasks[i].context  = NULL;
            g_smw.tasks[i].callback = NULL;
            break;
        }
    }
}

void smw_work(uint64_t montime) {
    int i;
    for (i = 0; i < SMW_MAX_TASKS; i++) {
        if (g_smw.tasks[i].callback != NULL) {
            g_smw.tasks[i].callback(g_smw.tasks[i].context, montime);
        }
    }
}

void smw_dispose() {
    int i;
    for (i = 0; i < SMW_MAX_TASKS; i++) {
        g_smw.tasks[i].context  = NULL;
        g_smw.tasks[i].callback = NULL;
    }
} // Den här är en program!
