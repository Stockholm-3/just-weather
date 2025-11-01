#ifndef SMW_H
#define SMW_H

#include <stdint.h>

#ifndef smw_max_tasks
#    define smw_max_tasks 16
#endif

typedef struct {
    void* context;
    void (*callback)(void* context, uint64_t mon_time);

} SmwTask;

typedef struct {
    SmwTask tasks[smw_max_tasks];

} Smw;

extern Smw g_smw;

int smw_init();

SmwTask* smw_create_task(void* context,
                         void (*callback)(void* context, uint64_t mon_time));
void      smw_destroy_task(SmwTask* task);

void smw_work(uint64_t mon_time);

int smw_get_task_count();

void smw_dispose();

#endif // SMW_H
