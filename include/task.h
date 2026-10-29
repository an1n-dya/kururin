// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_TASK_H
#define GUARD_TASK_H

// src/task.c (080040E0-080041F4): the task heap: create, free, run and find tasks. The functions
// other modules call.

#include "game.h"

void Task_Init(void);
struct Task *Task_Create(void (*func)(struct Task *), u32 size);
s32 Task_Destroy(struct Task *task);
s32 Task_DestroyByKey(u16 key);
void Task_RunAll(void);
struct Task *Task_FindByKey(u16 key);

#endif // GUARD_TASK_H
