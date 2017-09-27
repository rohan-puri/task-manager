# task-manager
Basic task manager:

There are basically 3 types of commands.
1. add
	- Adds a task to the task-manager
	- command specification on command-line, add task_type priority timer list_of_args
		- task_type represents type of task to be executed. There are currently
		  3 task_type "task1", "task2", "task3".
		- priority is an integer value, which specifies priority of the task that
		  is to be executed.
		- timer is also an integer value, it specifies the time in terms of seconds
		  timer value of 0 indicates execute immediately.
		  which means, the task should be runnable after this timer in seconds elapse.
		- list_of_args this is an list of arguments to be given to this task that it to
		  be queued. This list depends on the task_type. For example for your current
		  task types, they are as follows
			task_type:task1 -> 1 int arg.
			task_type:task2 -> 2 int args.
			task_type:task3 -> 3 int args.

2. list (TODO)
	- This command lists all the tasks submitted to the task manager.

3. test
	- This command runs a basic test for testing the functionality of task manager.


How to compile & run?

Compilation:
On command line run following command,
make

for cleaning up built files command is,
make clean

Run:
#./task_manager

This first asks how many threads to create, we need to specify the number here.

Then we can enter all the valid commands mentioned above.

Some examples,

To add a single task of type task1, with 5 as priority & no timer, command would be of the form
add task1 5 0 1

here, add is the command name, task1 is the task_type, 5 is the priority, 0 is the timer, 0
indicates execute immediately, 1 specifies arg1 value, which is specific for task_type "task1"

