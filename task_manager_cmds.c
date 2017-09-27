#include "task_manager_cmds.h"
#include "priority_queue.h"



void task1_fn(task_common_ctx_t *task_common_ctx, void *priv_data)
{

	fprintf(stdout, "TASK1:  1, priority: %d\n",
		task_common_ctx->tcc_priority);
	/**
	 * Simulate task1 completion time.
	 */
	sleep(10);

	task_yield();

	fprintf(stdout, "TASK1: 2, priority: %d\n",
		task_common_ctx->tcc_priority);

	task_exit();
}

void task2_fn(task_common_ctx_t *task_common_ctx, void *priv_data)
{
	fprintf(stdout, "TASK2:  1\n");
	/**
	 * Simulate task2 completion time.
	 */
	sleep(4);

	task_yield();
	fprintf(stdout, "TASK2:  2\n");

	task_exit();
}

void task3_fn(task_common_ctx_t *task_common_ctx, void *priv_data)
{
	/**
	 * Simulate task2 completion time.
	 */
	sleep(8);

	task_exit();
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
				"while given are %d\n", NO_OF_ARGS_FOR_TASK1,
				 no_of_args);
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
				"while given are %d\n", NO_OF_ARGS_FOR_TASK2,
				 no_of_args);
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
				"while given are %d\n", NO_OF_ARGS_FOR_TASK3,
				 no_of_args);
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
int add_task_cmd(char *cmd_args[], int no_of_args, int efd)
{
	int			i;
	int			rc = 0;
	int			cmd_idx = 0;
	task_common_ctx_t	*task_common_ctx = NULL;
	task_node_t		*task_node = NULL;

	if (no_of_args < MIN_ARGS_FOR_ADD_TASK) {
		fprintf(stderr, "ERR: minimum args expected is %d,"
			"while given is %d\n", MIN_ARGS_FOR_ADD_TASK, no_of_args);
		rc = -EINVAL;
		goto out;
	}

	fprintf(stderr, "ADD TASK: arguments of form "
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
	
	getcontext(&task_node->tn_context);
	task_node->tn_context.uc_link = 0;
	task_node->tn_context.uc_stack.ss_sp = malloc(USER_STACK_SIZE);
	task_node->tn_context.uc_stack.ss_size = USER_STACK_SIZE;
	task_node->tn_context.uc_stack.ss_flags=0;

	makecontext(&task_node->tn_context, (void *)task_node->tn_task_fn, 2,
			task_node->tn_task_common_ctx,
			task_node->tn_private_data);

	/**
	 * If a scheduling timer is provided,
	 * queue to blocking queue, instead of ready queue.
	 * Timer value of 0 indicates, immediate scheduling of the task
	 * to ready priority queue.
	 */
	if (task_node->tn_task_common_ctx->tcc_timer > 0) {

		rc = queue_task_with_timer_to_bq(efd, task_node);

		if (rc) {
			fprintf(stderr, "ERR: Task queue to blocking queue"
				" failed with rc: %d\n", rc);
			goto out;
		}
	} else {
		(void) queue_task_to_rq(task_node);
	}

	/**
	 * Freeing of task_node's resources is left to task scheduler,
	 * when the task is marked TASK_COMPLETED, task scheduler with
	 * free task_node's resources.
	 */
	total_tasks_submitted++;
	return rc;
out:
	free(task_node);
	free(task_common_ctx);
	return rc;
}

/**
 * List all the tasks that were added to this task manager.
 */
int list_all_tasks_cmd(char *cmd_args[], int no_of_args, int efd)
{
	task_node_t	*task = NULL;
	int		 i;

	fprintf(stdout, "Listing all the blocking tasks:\n");
	pthread_mutex_lock(&blocking_queue_mutex);
	TAILQ_FOREACH(task, &g_task_blocking_queue, tn_blocking_queue) {
		fprintf(stdout, "Task_type: %s, prio: %d, state: %d\n",
			task->tn_task_common_ctx->tcc_task_name,
			task->tn_task_common_ctx->tcc_priority,
			task->tn_task_common_ctx->tcc_state);
	}
	pthread_mutex_unlock(&blocking_queue_mutex);

	fprintf(stdout, "Listing all the ready tasks:\n");
	ready_priority_queue_lock(&g_task_ready_priority_queue);
	for (i = 0; i < g_task_ready_priority_queue.trpq_no_of_elements; i++) {
		task = g_task_ready_priority_queue.trpq_task_arr[i];
		fprintf(stdout, "Task_type: %s, prio: %d, state: %d\n",
			task->tn_task_common_ctx->tcc_task_name,
			task->tn_task_common_ctx->tcc_priority,
			task->tn_task_common_ctx->tcc_state);
	}
	ready_priority_queue_unlock(&g_task_ready_priority_queue);

	return 0;
}

/**
 * Run a basic test
 */
int test_cmd(char *cmd_args[], int no_of_args, int efd)
{
	int	i;
	char	*add_task1_args[5];

	test_value = 10;

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

		add_task_cmd(add_task1_args, 5, efd); 
	}

	pthread_cond_wait(&test_cond, &test_mutex);

	assert(total_tasks_submitted == total_tasks_executed);
	fprintf(stdout, "Test run completed successfully\n");

	return 0;
}

