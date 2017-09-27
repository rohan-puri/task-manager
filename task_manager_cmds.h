#ifndef TASK_MANAGER_CMDS_H
#define TASK_MANAGER_CMDS_H

#include "task_manager.h"

int add_task_cmd(char *cmd_args[], int no_of_args, int efd);

int list_all_tasks_cmd(char *cmd_args[], int no_of_args, int efd);

int test_cmd(char *cmd_args[], int no_of_args, int efd);

#endif /** TASK_MANAGER_CMDS_H */
