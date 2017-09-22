#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#define CMD_LINE_LEN			(100)
#define NO_OF_CMDS			(3)
#define CMD_NAME_LEN			(10)
#define THREAD_EXEC_DELAY		(45)	/** This value is in seconds */
#define READY_PRIORITY_MAX_QUEUE_DEPTH	(100)

typedef struct {
	int	t1_args1;
} t1_priv_t;

typedef struct {
	int	t2_args1;
	int	t2_args2;
} t2_priv_t;

typedef struct {
	int	t3_args1;
	int	t3_args2;
	int	t3_args3;
} t3_priv_t;

typedef enum {
	TASK_BLOCKED = 0,
	TASK_READY,
	TASK_RUNNING,
	TASK_COMPLETED
} task_state;

typedef struct {
	char		tcc_task_name[CMD_NAME_LEN];
	int		tcc_priority;
	int		tcc_timer;
	task_state	tcc_state;
} task_common_ctx_t;

struct task_node {
	task_common_ctx_t		*tn_task_common_ctx;
	int	(*tn_task_fn)		(task_common_ctx_t *, void *);
	void				*tn_private_data;
	TAILQ_ENTRY(task_node)		tn_blocking_queue;
	int				tn_tfd;
};

typedef struct task_node task_node_t;

typedef struct {
	task_node_t	**trpq_task_arr;
	int		 trpq_no_of_elements;
	int		 trpq_size;
} task_ready_priority_queue_t;

#define LEFT(x)		(2 * (x) + 1)
#define RIGHT(x)	(2 * (x) + 2)
#define PARENT(x)	((x) / 2)

/**
 * Minimum no. of arguments in cmdline for "add" command
 * that adds a task/job to the queue for thread pool to pick up
 * for execution, including command name too.
 */
#define MIN_ARGS_FOR_ADD_TASK	(4)

#define NO_OF_ARGS_FOR_TASK1	(5)
#define NO_OF_ARGS_FOR_TASK2	(6)
#define NO_OF_ARGS_FOR_TASK3	(7)

typedef struct {
	char	*ch_cmd_name;
	int 	(*ch_cmd_handler) (char *cmd_args[], int no_of_args, int efd);
} cmd_handler_t;

#endif /** TASK_MANAGER_H */


