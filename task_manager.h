#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

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
#include <ucontext.h>

#define CMD_LINE_LEN			(100)
#define NO_OF_CMDS			(3)
#define NO_OF_TASK_TYPES		(3)
#define CMD_NAME_LEN			(10)
#define THREAD_EXEC_DELAY		(15)	/** This value is in seconds */
#define READY_PRIORITY_MAX_QUEUE_DEPTH	(100)
#define MAX_CMD_ARGS			(7)	/** This count includes cmd name too */
#define	USER_STACK_SIZE			(64 * 1024) /** in bytes */

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
	TASK_BLOCKED = 1,
	TASK_READY,
	TASK_RUNNING,
	TASK_YIELD,
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
	void	(*tn_task_fn)		(task_common_ctx_t *, void *);
	void				*tn_private_data;
	TAILQ_ENTRY(task_node)		tn_blocking_queue;
	int				tn_tfd;
	ucontext_t			tn_context;
};

typedef struct task_node task_node_t;

typedef struct {
	task_node_t	**trpq_task_arr;
	int		 trpq_no_of_elements;
	int		 trpq_size;
	pthread_mutex_t	 trpq_mutex;
	pthread_cond_t	 trpq_not_empty_cond;
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

typedef struct {
	char	*task_type_name;
	int 	(*task_type_handler) (char *cmd_args[], int no_of_args, task_node_t *task_node);
} task_type_handler_t;


typedef struct {
	task_node_t	*running_task;
	ucontext_t	thread_context;
} thread_info_t;

void task_yield(void);
void task_exit(void);

TAILQ_HEAD(task_bq, task_node);

extern long				total_tasks_submitted;
extern long				total_tasks_executed;
extern pthread_mutex_t			blocking_queue_mutex;
extern struct task_bq			g_task_blocking_queue;
extern pthread_mutex_t			ready_priority_queue_mutex;
extern task_ready_priority_queue_t	g_task_ready_priority_queue;
extern pthread_mutex_t			test_mutex;
extern pthread_cond_t			test_cond;
extern int				test_value;

int queue_task_with_timer_to_bq(int efd, task_node_t *task_node);
int queue_task_to_rq(task_node_t *task_node);

#endif /** TASK_MANAGER_H */
