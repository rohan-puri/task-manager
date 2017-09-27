
#include "priority_queue.h"


pthread_mutex_t		ready_priority_queue_mutex;
pthread_cond_t		ready_priority_queue_not_empty_cond;

int ready_priority_queue_init(task_ready_priority_queue_t *task_ready_pq,
			      int size)
{
	pthread_mutex_init(&ready_priority_queue_mutex, NULL);
	pthread_cond_init(&ready_priority_queue_not_empty_cond, NULL);

	task_ready_pq->trpq_task_arr = malloc(sizeof(task_node_t *) * size);
	if (!task_ready_pq->trpq_task_arr) {
		fprintf(stderr, "ERR: Memory alloc for priority queue"
			"failed\n");
		return -ENOMEM;
	}
	/** In the beginning there are no elements */
	task_ready_pq->trpq_no_of_elements = 0;
	task_ready_pq->trpq_size = size;

	return 0;
}

int ready_priority_queue_is_empty(task_ready_priority_queue_t *task_ready_pq)
{
	return (task_ready_pq->trpq_no_of_elements == 0);
}

int ready_priority_queue_insert(task_ready_priority_queue_t *task_ready_pq,
				task_node_t *task_node)
{
	int i;

	if (task_ready_pq->trpq_no_of_elements == task_ready_pq->trpq_size) {
		fprintf(stderr, "ERR: Ready Priority queue full with no. of"
			" tasks: %d\n",
			task_ready_pq->trpq_no_of_elements);
		return -EINVAL;
	}
	task_ready_pq->trpq_task_arr[task_ready_pq->trpq_no_of_elements] = task_node;
	i = task_ready_pq->trpq_no_of_elements;
	task_ready_pq->trpq_no_of_elements++;

	/**
	 * If priority of new element is greater than its parent, swap with its
	 * parent
	 */
	while ((i > 0) &&
	       (task_ready_pq->trpq_task_arr[i]->tn_task_common_ctx->tcc_priority
		> task_ready_pq->trpq_task_arr[PARENT(i)]->tn_task_common_ctx->tcc_priority)) {
		task_node_t *temp = task_ready_pq->trpq_task_arr[i];

		task_ready_pq->trpq_task_arr[i] = task_ready_pq->trpq_task_arr[PARENT(i)];
		task_ready_pq->trpq_task_arr[PARENT(i)] = temp;
		i = PARENT(i);
	}
	return 0;
}

void ready_priority_queue_heapify(task_ready_priority_queue_t *task_ready_pq,
				  int idx)
{
	task_node_t	*temp = NULL;
	int		 left_idx;
	int		 right_idx;
	int		 lr_greater_idx;

	left_idx = LEFT(idx);
	right_idx = RIGHT(idx);

	if ((left_idx < task_ready_pq->trpq_no_of_elements) &&
	    (task_ready_pq->trpq_task_arr[left_idx]->tn_task_common_ctx->tcc_priority
		> task_ready_pq->trpq_task_arr[idx]->tn_task_common_ctx->tcc_priority)) {
		lr_greater_idx = left_idx;
	} else {
		lr_greater_idx = idx;
	}

	if ((right_idx < task_ready_pq->trpq_no_of_elements) &&
	    (task_ready_pq->trpq_task_arr[right_idx]->tn_task_common_ctx->tcc_priority
		> task_ready_pq->trpq_task_arr[lr_greater_idx]->tn_task_common_ctx->tcc_priority)) {
		lr_greater_idx = right_idx;
	}

	if (lr_greater_idx != idx) {
		temp = task_ready_pq->trpq_task_arr[lr_greater_idx];
		task_ready_pq->trpq_task_arr[lr_greater_idx] = task_ready_pq->trpq_task_arr[idx];
		task_ready_pq->trpq_task_arr[idx] = temp;
		ready_priority_queue_heapify(task_ready_pq, lr_greater_idx);
	}
}

task_node_t *ready_priority_queue_remove(task_ready_priority_queue_t *task_ready_pq)
{
	task_node_t *task = NULL;

	if (task_ready_pq->trpq_no_of_elements < 1) {	
		fprintf(stderr, "ERR: Task ready priority queue is empty\n");
		return NULL;
	}
	task = task_ready_pq->trpq_task_arr[0];
	task_ready_pq->trpq_task_arr[0] =
		task_ready_pq->trpq_task_arr[task_ready_pq->trpq_no_of_elements - 1];
	task_ready_pq->trpq_no_of_elements--;

	ready_priority_queue_heapify(task_ready_pq, 0);

	return task;
}

void ready_priority_queue_cond_wait(task_ready_priority_queue_t *task_ready_pq)
{
	pthread_cond_wait(&ready_priority_queue_not_empty_cond,
			&ready_priority_queue_mutex);
}

void ready_priority_queue_signal(task_ready_priority_queue_t *task_ready_pq)
{
	pthread_cond_signal(&ready_priority_queue_not_empty_cond);
}

void ready_priority_queue_lock(task_ready_priority_queue_t *task_ready_pq)
{
	pthread_mutex_lock(&ready_priority_queue_mutex);
}

void ready_priority_queue_unlock(task_ready_priority_queue_t *task_ready_pq)
{
	pthread_mutex_unlock(&ready_priority_queue_mutex);
}

int queue_task_to_rq(task_node_t *task_node)
{
	int	rc = -1;
	int	signal = 0;

	/**
	 * no non-negative timer is specified, put the task
	 * immediately on ready queue
	 */
	task_node->tn_task_common_ctx->tcc_state = TASK_READY;
	ready_priority_queue_lock(&g_task_ready_priority_queue);
	signal = ready_priority_queue_is_empty(&g_task_ready_priority_queue);
	rc = ready_priority_queue_insert(&g_task_ready_priority_queue,
			task_node);
	ready_priority_queue_unlock(&g_task_ready_priority_queue);
	if (rc) {
		fprintf(stderr, "ERR: Task insertion in ready pq failed "
				"with rc: %d\n", rc);
		goto out;
	}

	if (signal)
		pthread_cond_signal(&ready_priority_queue_not_empty_cond);

out:
	return rc;
}
