CC = gcc
FLAGS = -Wall -std=c11

all: my_shell

my_shell: 
	$(CC) $(FLAGS) -o my_shell my_shell.c

.PHONY: clean
clean_exc:
	rm -f my_shell
clean:
	rm -f *.o 
