#ifndef _smw_h_
#define _smw_h_

#include <stdint.h>

#ifndef SMW_MAX_TASKS
#    define SMW_MAX_TASKS 16
#endif

typedef struct {
    void* context;
    void (*callback)(void* context, uint64_t montime);
} smw_task_t;

typedef struct {
    smw_task_t tasks[SMW_MAX_TASKS];
} smw_t;

extern smw_t g_smw;

int smw_init();

smw_task_t* smw_create_task(void* context,
                            void (*callback)(void* context, uint64_t montime));
void        smw_destroy_task(smw_task_t* _Task);

void smw_work(uint64_t montime);
void smw_dispose();

#endif //_smw_h_