#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "task_manager.h"

int ready_priority_queue_init(task_ready_priority_queue_t *task_ready_pq,
			      int size);

int ready_priority_queue_is_empty(task_ready_priority_queue_t *task_ready_pq);

int ready_priority_queue_insert(task_ready_priority_queue_t *task_ready_pq,
				task_node_t *task_node);

task_node_t *ready_priority_queue_remove(task_ready_priority_queue_t *task_ready_pq);

void ready_priority_queue_lock(task_ready_priority_queue_t *task_ready_pq);

void ready_priority_queue_unlock(task_ready_priority_queue_t *task_ready_pq);

void ready_priority_queue_cond_wait(task_ready_priority_queue_t *task_ready_pq);

void ready_priority_queue_broadcast(task_ready_priority_queue_t *task_ready_pq);

#endif /** PRIORITY_QUEUE_H */
