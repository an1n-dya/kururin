// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// task: the task heap: create, free, run and find tasks.
// 080040E0-080041F4, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "task.h"

extern u8 gTaskHeap[]; // a heap (Heap_Init)
extern s32 gTaskCount;

/**
 * Sets up the task heap gTaskHeap on EWRAM 0x0203E000 (0x2000 bytes): no tasks left.
 */
void Task_Init(void)
{
    Heap_Init(gTaskHeap, (void *)0x0203E000, 0x2000);
    gTaskCount = 0;
}
/**
 * Allocates a task of *size* bytes from gTaskHeap and sets it up to run *func*, with
 * key and state cleared; counts it in gTaskCount. NULL when the heap is full.
 */
struct Task *Task_Create(void (*func)(struct Task *), u32 size)
{
    struct Task *task = Heap_Alloc(gTaskHeap, size);

    if (task != NULL)
    {
        task->func = func;
        task->key = 0;
        task->state = 0;
        gTaskCount++;
    }
    return task;
}
/**
 * Frees a task Task_Create allocated and takes it off gTaskCount's count; returns the
 * new count.
 */
s32 Task_Destroy(struct Task *task)
{
    Heap_Free(gTaskHeap, task);
    return --gTaskCount;
}
/**
 * Frees every task of the heap gTaskHeap whose key is *key* (as Task_Destroy does);
 * returns how many tasks are left.
 */
s32 Task_DestroyByKey(u16 key)
{
    struct Heap *heap = (struct Heap *)gTaskHeap;
    struct HeapBlock *b = heap->head.next;

    do
    {
        if (b->used)
        {
            struct Task *task = (struct Task *)(b + 1);

            if (task->key == key)
            {
                Heap_Free(gTaskHeap, task);
                gTaskCount--;
            }
        }
        b = b->next;
    } while (b != &heap->head);
    return gTaskCount;
}
/**
 * Runs every task of the heap gTaskHeap (Task_Create): each block in use, from the
 * first after the head round the ring, calls its task's func with the task.
 */
void Task_RunAll(void)
{
    struct Heap *heap = (struct Heap *)gTaskHeap;
    struct HeapBlock *b = heap->head.next;

    do
    {
        if (b->used)
        {
            struct Task *task = (struct Task *)(b + 1);

            task->func(task);
        }
        b = b->next;
    } while (b != &heap->head);
}
/**
 * The first task of the heap gTaskHeap (round the ring from the head) whose key is
 * *key*, or NULL.
 */
struct Task *Task_FindByKey(u16 key)
{
    struct Heap *heap = (struct Heap *)gTaskHeap;
    struct HeapBlock *b = heap->head.next;

    do
    {
        if (b->used)
        {
            struct Task *task = (struct Task *)(b + 1);

            if (task->key == key)
                return task;
        }
        b = b->next;
    } while (b != &heap->head);
    return NULL;
}
