#include "task_manager.h"
#include "task_manager_cmds.h"
#include "priority_queue.h"

long	total_tasks_submitted;
long	total_tasks_executed;

pthread_key_t	thread_key;

/**
 * Global task ready priority queue
 */
task_ready_priority_queue_t	g_task_ready_priority_queue;

/**
 * Global task blocking queue
 */
struct task_bq		g_task_blocking_queue;
pthread_mutex_t		blocking_queue_mutex;

void task_yield(void)
{
	thread_info_t	*thread_info = pthread_getspecific(thread_key);
	int		 rc = -1;

	ready_priority_queue_lock(&g_task_ready_priority_queue);
	rc = ready_priority_queue_insert(&g_task_ready_priority_queue,
					 thread_info->running_task);
	assert(rc == 0);
	ready_priority_queue_unlock(&g_task_ready_priority_queue);
	thread_info->running_task->tn_task_common_ctx->tcc_state = TASK_YIELD;
	swapcontext(&thread_info->running_task->tn_context,
		    &thread_info->thread_context);
}

void task_exit(void)
{
	thread_info_t	*thread_info = pthread_getspecific(thread_key);

	thread_info->running_task->tn_task_common_ctx->tcc_state = TASK_COMPLETED;
	total_tasks_executed++;
	swapcontext(&thread_info->running_task->tn_context,
		    &thread_info->thread_context);
}

int queue_task_with_timer_to_bq(int efd, task_node_t *task_node)
{
	
	struct itimerspec 	ts;
	struct epoll_event	event;
	int			tfd = timerfd_create(CLOCK_MONOTONIC, 0);
	int			rc = -1;

	assert(task_node->tn_task_common_ctx->tcc_timer > 0);

	/**
	 * Add the timerfd for this later to be scheduled
	 * task.
	 */
	if (tfd == -1) {
		fprintf(stderr, "ERR: Creating timer fd failed\n");
		/** TODO: Pick more appriopriate error type here */
		rc = -EINVAL;
		goto out;
	}

	ts.it_interval.tv_sec = 0;
	ts.it_interval.tv_nsec = 0;
	ts.it_value.tv_sec = task_node->tn_task_common_ctx->tcc_timer;
	ts.it_value.tv_nsec = 0;

	rc = timerfd_settime(tfd, 0, &ts, NULL);

	if (rc < 0) {
		fprintf(stderr, "ERR: Timer fd failed\n");
		close(tfd);
		goto out;
	}

	memset(&event, 0, sizeof(event));
	event.events = EPOLLIN;

	event.data.fd = tfd;

	task_node->tn_tfd = tfd;
	epoll_ctl(efd, EPOLL_CTL_ADD, tfd, &event);

	task_node->tn_task_common_ctx->tcc_state = TASK_BLOCKED;
	/**
	 * Add this task to the blocking queue
	 */
	pthread_mutex_lock(&blocking_queue_mutex);
	TAILQ_INSERT_TAIL(&g_task_blocking_queue, task_node,
			tn_blocking_queue);
	pthread_mutex_unlock(&blocking_queue_mutex);

out:
	return rc;
}



cmd_handler_t cmd_table[NO_OF_CMDS] = {
		{"add", add_task_cmd},
		{"list", list_all_tasks_cmd},
		{"test", test_cmd},
};

void *task_executor(void *args)
{
	task_node_t	*task_node = NULL;
	int		*thread_id = (int *) args;
	thread_info_t	*thread_info;

	/**
	 * Each thread has this execution delay to start with.
	 * Reason to have this delay is so that we give enough
	 * time to the user to enter all the tasks for the
	 * first initial run.
	 */
	sleep(THREAD_EXEC_DELAY);

	thread_info = malloc(sizeof(thread_info_t));

	if(!thread_info) {
		fprintf(stderr, "ERR: Malloc for thread_info_t failed\n");
		return NULL;
	}
	memset(thread_info, 0, sizeof(thread_info_t));
	getcontext(&thread_info->thread_context);

	while (1) {
		ready_priority_queue_lock(&g_task_ready_priority_queue);
		while (ready_priority_queue_is_empty(&g_task_ready_priority_queue)) {
			ready_priority_queue_cond_wait(&g_task_ready_priority_queue);
		}

		/** Get the first task from the ready priority queue to execute */
		task_node = ready_priority_queue_remove(&g_task_ready_priority_queue);
		/**
		 * Since the task is removed from the queue,
		 * our interaction with the queue is also over,
		 * so unlock the queue mutex here.
		 */
		ready_priority_queue_unlock(&g_task_ready_priority_queue);

		//memset(&task_node->tn_context, 0, sizeof(task_node->tn_context));

		assert(task_node->tn_task_common_ctx->tcc_state != TASK_COMPLETED);

		fprintf(stdout, "Thread no: %d, executing task with priority: %d\n",
				*thread_id,
				task_node->tn_task_common_ctx->tcc_priority);
		task_node->tn_task_common_ctx->tcc_state = TASK_RUNNING;

		pthread_setspecific(thread_key, thread_info);
		thread_info->running_task = task_node;

		swapcontext(&thread_info->thread_context,
			    &task_node->tn_context);

		thread_info->running_task = NULL;
	}
	if (task_node->tn_task_common_ctx->tcc_state == TASK_COMPLETED) {
		/**
		 * Release all task resources for this task
		 */
		free(task_node->tn_task_common_ctx);
		free(task_node->tn_private_data);
		free(task_node);
	}
}

int globals_init(void)
{
	int rc = -1;

	/**
	 * All initializations
	 */
	rc = ready_priority_queue_init(&g_task_ready_priority_queue,
					READY_PRIORITY_MAX_QUEUE_DEPTH);
	if (rc) {
		fprintf(stderr, "ERR: PQ INIT failed with rc: %d\n", rc);
		return rc;
	}
	TAILQ_INIT(&g_task_blocking_queue);
	pthread_mutex_init(&blocking_queue_mutex, NULL);
	pthread_key_create(&thread_key, NULL);

	return rc;
}

void timerfd_event_handler(int efd, struct epoll_event *event)
{
	task_node_t	*task = NULL;
	task_node_t	*task_next = NULL;
	int		 rc = -1;
	int		 signal = 0;

	/**
	 * Remove this timer_fd from the epoll watch list
	 */
	epoll_ctl(efd, EPOLL_CTL_DEL, event->data.fd, NULL);
	/**
	 * Since timer is triggered for this timerfd,
	 * we need to dequeue task from blocking queue &
	 * then enqueue task to ready queue.
	 */
	pthread_mutex_lock(&blocking_queue_mutex);
	ready_priority_queue_lock(&g_task_ready_priority_queue);
	for (task = TAILQ_FIRST(&g_task_blocking_queue);
			task != NULL; task = task_next) {
		task_next = TAILQ_NEXT(task, tn_blocking_queue);
		if (task->tn_tfd == event->data.fd) {
			/**
			 * Timer got triggered for this task
			 */
			TAILQ_REMOVE(&g_task_blocking_queue,
					task, tn_blocking_queue);
			task->tn_task_common_ctx->tcc_state = TASK_READY;
			signal = ready_priority_queue_is_empty(&g_task_ready_priority_queue);
			rc = ready_priority_queue_insert(&g_task_ready_priority_queue, task);
			if (rc) {
				fprintf(stderr, "ERR: Task insertion in ready pq failed with rc: %d\n", rc);
			}
			break;
		}
	}

	ready_priority_queue_unlock(&g_task_ready_priority_queue);
	pthread_mutex_unlock(&blocking_queue_mutex);
	if (signal)
		ready_priority_queue_signal(&g_task_ready_priority_queue);

	/**
	 * Timerfd event handling completed, close the timerfd
	 */
	close(event->data.fd);
}

void input_cmd_event_handler(int efd)
{
	char			 cmd_line[CMD_LINE_LEN] =  {'0'};
	char			*cmd_args[MAX_CMD_ARGS];
	char			*token = NULL;
	int			 i;
	int			 no_of_tokens = 0;
	int			 rc;

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

int main(void)
{
	int			 efd = -1;
	struct epoll_event	 event;
	int			 rc = 0;
	pthread_t		*tid = NULL;
	int			 no_of_threads = 0;
	int			*thread_local_var_list = NULL;
	int			 i;

	efd = epoll_create1(0);

	if (efd == -1) {
		fprintf(stderr, "Epoll create failed\n");
		exit(1);
	}


	rc = globals_init();
	if (rc) {
		fprintf(stderr, "ERR: Globals init failed with rc: %d\n", rc);
		return rc;
	}

	fprintf(stdout, "Specify the number of threads for this thread pool: ");
	scanf("%d", &no_of_threads);

	tid = malloc(sizeof(thread_info_t) * no_of_threads);

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

			/**
			 * event triggered on timerfd
			 */
			timerfd_event_handler(efd, &event);
		} else {
			/**
			 * event triggered on stdin(command line).
			 * i.e. command entered on stdin.
			 */
			input_cmd_event_handler(efd);
		}
	}

	return 0;
}

