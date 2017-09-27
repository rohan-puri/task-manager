CC=gcc
CFLAGS= -Wall -g

task_manager: task_manager.o task_manager_cmds.o
	$(CC) -o task_manager task_manager.o task_manager_cmds.o -lpthread

clean:
	rm -f task_manager *.o

