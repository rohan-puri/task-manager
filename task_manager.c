#include <stdio.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/queue.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include <assert.h>

#include "task_manager.h"

TAILQ_HEAD(task_bq, task_node);

long	total_tasks_submitted;
long	total_tasks_executed;

/**
 * Global task ready priority queue
 */
task_ready_priority_queue_t	g_task_ready_priority_queue;

pthread_mutex_t			ready_priority_queue_mutex;
pthread_cond_t			ready_priority_queue_not_empty_cond;

/**
 * Global task blocking queue
 */
struct task_bq		g_task_blocking_queue;
pthread_mutex_t		blocking_queue_mutex;

int ready_priority_queue_init(task_ready_priority_queue_t *task_ready_pq,
			      int size)
{
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

#if 0
void dump_priority_arr(task_ready_priority_queue_t *task_ready_pq)
{
	int i = 0;

	for (i = 0; i < task_ready_pq->trpq_no_of_elements; i++) {
		fprintf(stderr, "%d ", task_ready_pq->trpq_task_arr[i]->tn_task_common_ctx->tcc_priority);
	}
	fprintf(stderr, "\n");
}
#endif

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

void debug_print_task_common(task_common_ctx_t *task_common_ctx)
{
	if (task_common_ctx != NULL) {
		printf("task_name:%s\n", task_common_ctx->tcc_task_name);
		printf("prio:%d\n", task_common_ctx->tcc_priority);
		printf("timer:%d\n", task_common_ctx->tcc_timer);
		printf("state:%d\n", task_common_ctx->tcc_state);
	}
}

int task1_fn(task_common_ctx_t *task_common_ctx, void *priv_data)
{
	/**
	 * Simulate task1 completion time.
	 */
	sleep(2);

	return 0;
}

int task2_fn(task_common_ctx_t *task_common_ctx, void *priv_data)
{
	/**
	 * Simulate task1 completion time.
	 */
	sleep(4);

	return 0;
}

int task3_fn(task_common_ctx_t *task_common_ctx, void *priv_data)
{
	/**
	 * Simulate task1 completion time.
	 */
	sleep(8);

	return 0;
}

int task1_handler(char *cmd_args[], int no_of_args, task_node_t *task_node)
{
	int		rc = 0;
	t1_priv_t	*task1_private_data = malloc(sizeof(t1_priv_t));

	if(!task1_private_data) {
		fprintf(stderr, "ERR: Memory allocation failed for t1_priv_t\n");
		rc = -ENOMEM;
		goto out;
	}

	if (no_of_args != NO_OF_ARGS_FOR_TASK1) {
		fprintf(stderr, "ERR: No of args expected for task1 are %d, "
				"while given are %d\n", NO_OF_ARGS_FOR_TASK1, no_of_args);
		rc = -EINVAL;
		goto out;
	}

	task_node->tn_task_fn = task1_fn;
	/**
	 * cmd_args[4] -> task1 specific args1
	 */
	task1_private_data->t1_args1 = atoi(cmd_args[4]);
	task_node->tn_private_data = task1_private_data;

out:
	return rc;
}

int task2_handler(char *cmd_args[], int no_of_args, task_node_t *task_node)
{
	int		rc = 0;
	t2_priv_t	*task2_private_data = malloc(sizeof(t2_priv_t));

	if(!task2_private_data) {
		fprintf(stderr, "ERR: Memory allocation failed for t2_priv_t\n");
		rc = -ENOMEM;
		goto out;
	}

	if (no_of_args != NO_OF_ARGS_FOR_TASK2) {
		fprintf(stderr, "ERR: No of args expected for task2 are %d, "
				"while given are %d\n", NO_OF_ARGS_FOR_TASK2, no_of_args);
		rc = -EINVAL;
		goto out;
	}

	task_node->tn_task_fn = task2_fn;
	/**
	 * cmd_args[4] -> task2 specific args1
	 */
	task2_private_data->t2_args1 = atoi(cmd_args[4]);
	/**
	 * cmd_args[5] -> task2 specific args2
	 */
	task2_private_data->t2_args2 = atoi(cmd_args[5]);
	task_node->tn_private_data = task2_private_data;

out:
	return rc;
}

int task3_handler(char *cmd_args[], int no_of_args, task_node_t *task_node)
{
	int		rc = 0;
	t3_priv_t	*task3_private_data = malloc(sizeof(t3_priv_t));

	if(!task3_private_data) {
		fprintf(stderr, "ERR: Memory allocation failed for t3_priv_t\n");
		rc = -ENOMEM;
		goto out;
	}

	if (no_of_args != NO_OF_ARGS_FOR_TASK3) {
		fprintf(stderr, "ERR: No of args expected for task3 are %d, "
				"while given are %d\n", NO_OF_ARGS_FOR_TASK3, no_of_args);
		rc = -EINVAL;
		goto out;
	}

	task_node->tn_task_fn = task3_fn;
	/**
	 * cmd_args[4] -> task3 specific args1
	 */
	task3_private_data->t3_args1 = atoi(cmd_args[4]);
	/**
	 * cmd_args[5] -> task3 specific args2
	 */
	task3_private_data->t3_args2 = atoi(cmd_args[5]);
	/**
	 * cmd_args[6] -> task3 specific args3
	 */
	task3_private_data->t3_args3 = atoi(cmd_args[6]);
	task_node->tn_private_data = task3_private_data;

out:
	return rc;
}

task_type_handler_t task_type_table[NO_OF_TASK_TYPES] = {
		{"task1", task1_handler},
		{"task2", task2_handler},
		{"task3", task3_handler},
};

/**
 * Command line input to add_task is as follows:
 * add task_type priority timer [args1..argsn]
 * args1 to argsn vary according to task_type.
 */
int add_task(char *cmd_args[], int no_of_args, int efd)
{
	int			i;
	int			rc = 0;
	int			cmd_idx = 0;
	task_common_ctx_t	*task_common_ctx = NULL;
	task_node_t		*task_node = NULL;
	t1_priv_t		*task1_private_data = NULL;
	t2_priv_t		*task2_private_data = NULL;
	t3_priv_t		*task3_private_data = NULL;


	if (no_of_args < MIN_ARGS_FOR_ADD_TASK) {
		fprintf(stderr, "ERR: minimum args expected is %d,"
			"while given is %d\n", MIN_ARGS_FOR_ADD_TASK, no_of_args);
		rc = -EINVAL;
		goto out;
	}

	printf("ADD TASK: arguments of form "
		"[cmd_name task_type priority timer task_type_args]: ");
	for (i = 0; i < no_of_args; i++) {
		fprintf(stdout, "%s ", cmd_args[i]);
	}

	task_common_ctx = malloc(sizeof(task_common_ctx_t));

	if(!task_common_ctx) {
		fprintf(stderr, "ERR: Memory allocation for "
			"task_common_ctx_t failed\n");
		rc = -ENOMEM;
		goto out;
	}

	/**
	 * cmd_args[0] -> command name
	 * cmd_args[1] -> task type
	 */
	cmd_idx++;
	strncpy(task_common_ctx->tcc_task_name, cmd_args[cmd_idx],
		strlen(cmd_args[cmd_idx]));

	cmd_idx++;

	/**
	 * cmd_args[2] -> task priority
	 */
	task_common_ctx->tcc_priority = atoi(cmd_args[cmd_idx++]);

	/**
	 * cmd_args[3] -> task timer (time in seconds after which the task is
	 * to be scheduled(put in the ready priority queue)
	 */
	task_common_ctx->tcc_timer = atoi(cmd_args[cmd_idx++]);

	task_node = malloc(sizeof(task_node_t));

	if (!task_node) {
		fprintf(stderr, "ERR: Memory allocation failed for task_node_t\n");
		rc = -ENOMEM;
		goto out;
	}

	task_node->tn_task_common_ctx = task_common_ctx;
	/**
	 * Initialize with -1, since -1 is not a valid fd
	 */
	task_node->tn_tfd = -1;

	for (i = 0; i < NO_OF_TASK_TYPES; i++) {
		if (strncmp(cmd_args[1], task_type_table[i].task_type_name,
					strlen(task_type_table[i].task_type_name)) == 0 ) {
//			rc = cmd_table[i].task_type_handler(cmd_args, no_of_tokens);
			rc = task_type_table[i].task_type_handler(cmd_args,
								  no_of_args,
								  task_node);
			if (rc) {
				fprintf(stderr, "ERR: cmd:%s failed with rc:%d\n",
						cmd_args[0], rc);
			}
			break;
		}
	}
	/**
	 * If a scheduling timer is provided,
	 * queue to blocking queue, instead of ready queue.
	 * Timer value of 0 indicates, immediate scheduling of the task
	 * to ready priority queue.
	 */
	if (task_node->tn_task_common_ctx->tcc_timer > 0) {
		struct itimerspec 	ts;
		struct epoll_event	event;
		int			tfd = timerfd_create(CLOCK_MONOTONIC, 0);
	
		/**
		 * Add the timerfd for this later to be scheduled
		 * task.
		 */
		if (tfd == -1) {
			fprintf(stderr, "ERR: Creating timer fd failed\n");
			/** TODO: Pick more appriopriate error type here */
			rc = -EINVAL;
			goto timerfd_out;
		}

		ts.it_interval.tv_sec = 0;
		ts.it_interval.tv_nsec = 0;
		ts.it_value.tv_sec = 10;
		ts.it_value.tv_nsec = 0;

		if (timerfd_settime(tfd, 0, &ts, NULL) < 0) {
			fprintf(stderr, "ERR: Timer fd failed\n");
			close(tfd);
			rc = -EINVAL;
			goto timerfd_out;
		}

		memset(&event, 0, sizeof(event));
		event.events = EPOLLIN;

		event.data.fd = tfd;

		task_node->tn_tfd = tfd;
		epoll_ctl(efd, EPOLL_CTL_ADD, tfd, &event);

		task_common_ctx->tcc_state = TASK_BLOCKED;
		/**
		 * Add this task to the blocking queue
		 */
		pthread_mutex_lock(&blocking_queue_mutex);
		TAILQ_INSERT_TAIL(&g_task_blocking_queue, task_node,
				  tn_blocking_queue);
		pthread_mutex_unlock(&blocking_queue_mutex);
	} else {
		/**
		 * no non-negative timer is specified, put the task
		 * immediately on ready queue
		 */
		task_common_ctx->tcc_state = TASK_READY;
		pthread_mutex_lock(&ready_priority_queue_mutex);
		rc = ready_priority_queue_insert(&g_task_ready_priority_queue,
						 task_node);
		if (rc) {
			fprintf(stderr, "ERR: Task insertion in ready pq failed "
				"with rc: %d\n", rc);
			goto timerfd_out;
		}
		pthread_mutex_unlock(&ready_priority_queue_mutex);
		pthread_cond_broadcast(&ready_priority_queue_not_empty_cond);
	}

	/**
	 * We do not need to free anything, if we are here,
	 * since we have successfully done everything. We
	 * just need to return rc go we jump to out.
	 */
	total_tasks_submitted++;
	goto out;

timerfd_out:
#if 0
	free(task3_private_data);
	free(task2_private_data);
	free(task1_private_data);
#endif
	free(task_node);
	free(task_common_ctx);
out:
	return rc;
}

/**
 * List all the tasks that were added to this task manager.
 * This is TODO
 */
int list_all_tasks(char *cmd_args[], int no_of_args, int efd)
{
	return 0;
}

/**
 * Run a basic test
 */
int test(char *cmd_args[], int no_of_args, int efd)
{
	int	i;
	char	*add_task1_args[5];
	int	priority;

	if (no_of_args > 1) {
		fprintf(stderr, "ERR: Invalid no. of arguments for test cmd, "
			"only 1 arg is required,"
			"i.e. the cmd name test itself\n");
		return -EINVAL;
	}

	for (i = 0; i < 5; i++) {
		add_task1_args[i] = malloc(sizeof(char) * 10);
	}
	for (i = 0; i < 10; i++) {
		memcpy(add_task1_args[0], "add", strlen("add"));
		memcpy(add_task1_args[1], "task1", strlen("task1"));
		/** Set priority */
		sprintf(add_task1_args[2], "%d", i + 1);
		/** Set timer */
		sprintf(add_task1_args[3], "%d", 0);
		/** Set first args1 */
		sprintf(add_task1_args[4], "%d", i + 1);

		add_task(add_task1_args, 5, efd); 
	}

	/** Add sleep for tasks to be completed */
	sleep(70);
	assert(total_tasks_submitted == total_tasks_executed);

	return 0;
}

cmd_handler_t cmd_table[NO_OF_CMDS] = {
		{"add", add_task},
		{"list", list_all_tasks},
		{"test", test},
};

void *task_executor(void *args)
{
	task_node_t	*task_node = NULL;
	int		*thread_id = (int *) args;
	int		 rc = 0;

	/**
	 * Each thread has this execution delay to start with.
	 * Reason to have this delay is so that we give enough
	 * time to the user to enter all the tasks for the
	 * first initial run.
	 */
	sleep(THREAD_EXEC_DELAY);

	while (1) {

		pthread_mutex_lock(&ready_priority_queue_mutex);

		while (ready_priority_queue_is_empty(&g_task_ready_priority_queue)) {
			pthread_cond_wait(&ready_priority_queue_not_empty_cond,
					  &ready_priority_queue_mutex);
		}
		/** Get the first task from the ready priority queue to execute */
		task_node = ready_priority_queue_remove(&g_task_ready_priority_queue);
		/**
		 * Since the task is removed from the queue,
		 * our interaction with the queue is also over,
		 * so unlock the queue mutex here.
		 */
		pthread_mutex_unlock(&ready_priority_queue_mutex);
		fprintf(stdout, "Thread no: %d, executing task with priority: %d\n",
			*thread_id,
			task_node->tn_task_common_ctx->tcc_priority);
		task_node->tn_task_common_ctx->tcc_state = TASK_RUNNING;
		rc = task_node->tn_task_fn(task_node->tn_task_common_ctx,
					   task_node->tn_private_data);

		/**
		 * If task function's execution failed, just log the error
		 * and proceed with execution of the next task from ready
		 * priority queue
		 */
		if (rc) {
			fprintf(stderr, "ERR: task_node: %p "
				" failed execution with rc: %d\n", task_node, rc);
		}
		task_node->tn_task_common_ctx->tcc_state = TASK_COMPLETED;
		total_tasks_executed++;
		/**
		 * Release all task resources for this task
		 */
		free(task_node->tn_task_common_ctx);
		free(task_node);
	}
}

int main(void)
{
	int			 efd = -1;
	struct epoll_event	 event;
	char			 cmd_line[CMD_LINE_LEN] =  {'0'};
	char			 *cmd_args[7];
	int			 priority = -1;
	int			 timer = 0;
	char			 *token = NULL;
	int			 i;
	int			 no_of_tokens = 0;
	int			 rc = 0;
	pthread_t		 *tid = NULL;
	int			 no_of_threads = 0;
	int			 *thread_local_var_list = NULL;

	efd = epoll_create1(0);

	if (efd == -1) {
		fprintf(stderr, "Epoll create failed\n");
		exit(1);
	}

	/**
	 * All initializations
	 */
	rc = ready_priority_queue_init(&g_task_ready_priority_queue,
					READY_PRIORITY_MAX_QUEUE_DEPTH);
	if (rc) {
		fprintf(stderr, "ERR: PQ INIT failed with rc: %d\n", rc);
		return rc;
	}
	pthread_mutex_init(&ready_priority_queue_mutex, NULL);
	pthread_cond_init(&ready_priority_queue_not_empty_cond, NULL);
	TAILQ_INIT(&g_task_blocking_queue);
	pthread_mutex_init(&blocking_queue_mutex, NULL);

	fprintf(stdout, "Specify the number of threads for this thread pool: ");
	scanf("%d", &no_of_threads);

	tid = malloc(sizeof(pthread_t) * no_of_threads);

	if (!tid) {
		fprintf(stderr, "ERR: Could not allocate memory for thread objects\n");
		return -ENOMEM;
	}
	thread_local_var_list = malloc(sizeof(int) * no_of_threads);

	if (!thread_local_var_list) {
		fprintf(stderr, "ERR: Could not allocate memory for thread variables\n");
		free(tid);
		return -ENOMEM;
	}

	/**
	 *  Create all the worker threads.
	 */
	for (i = 0; i < no_of_threads; i++) {
		thread_local_var_list[i] = i;
		rc = pthread_create(&tid[i], NULL, task_executor,
				    (void *) &thread_local_var_list[i]);
		if (rc) {
			fprintf(stderr, "%dth Thread creation failed with rc:%d\n", i, rc);
		}
	}
	
	memset(&event, 0, sizeof(event));

	event.events = EPOLLIN;

	/**
	 * Set to stdin
	 */
	event.data.fd = 0;

	epoll_ctl(efd, EPOLL_CTL_ADD, 0, &event);

	for (;;) {
		fprintf(stdout, "Enter command to run\n");
		fprintf(stdout, "Currently supported commands [add, list, test]\n");
		epoll_wait(efd, &event, 1, -1);

		if (event.data.fd != 0) {
			task_node_t	*task = NULL;
			task_node_t	*task_next = NULL;

			/**
			 * Remove this timer_fd from the epoll watch list
			 */
			epoll_ctl(efd, EPOLL_CTL_DEL, event.data.fd, NULL);
			/**
			 * Since timer is triggered for this timerfd,
			 * we need to dequeue task from blocking queue &
			 * then enqueue task to ready queue.
			 */
			pthread_mutex_lock(&blocking_queue_mutex);
			pthread_mutex_lock(&ready_priority_queue_mutex);
			for (task = TAILQ_FIRST(&g_task_blocking_queue);
				task != NULL; task = task_next) {
				task_next = TAILQ_NEXT(task, tn_blocking_queue);
				if (task->tn_tfd == event.data.fd) {
					/**
					 * Timer got triggered for this task
					 */
					TAILQ_REMOVE(&g_task_blocking_queue,
						     task, tn_blocking_queue);
					task->tn_task_common_ctx->tcc_state = TASK_READY;
					rc = ready_priority_queue_insert(&g_task_ready_priority_queue, task);
					if (rc) {
						fprintf(stderr, "ERR: Task insertion in ready pq failed with rc: %d\n", rc);
					}
					break;
					
				}
			}
			pthread_mutex_unlock(&ready_priority_queue_mutex);
			pthread_mutex_unlock(&blocking_queue_mutex);
			pthread_cond_broadcast(&ready_priority_queue_not_empty_cond);

			/**
			 * Timer has triggered now, close the timerfd
			 */
			close(event.data.fd);

		} else {
			read(0, cmd_line, sizeof(cmd_line));

			i = 0;
			token = strtok(cmd_line, " ");
			no_of_tokens = 0;
			while (token != NULL) {
				no_of_tokens++;
				cmd_args[i++] = token;
				token = strtok(NULL, " ");

			}

			for (i = 0; i < NO_OF_CMDS; i++) {
				if (strncmp(cmd_args[0], cmd_table[i].ch_cmd_name,
							strlen(cmd_table[i].ch_cmd_name)) == 0 ) {
					rc = cmd_table[i].ch_cmd_handler(cmd_args, no_of_tokens, efd);
					if (rc) {
						fprintf(stderr, "ERR: cmd:%s failed with rc:%d\n",
								cmd_args[0], rc);
					}
					break;
				}
			}
			if (i == NO_OF_CMDS) {
				fprintf(stderr, "ERR: Command (%s) entered is invalid\n", cmd_args[0]);
			}
		}
	}
	return 0;
}

