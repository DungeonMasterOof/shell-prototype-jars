CC = gcc
PROG = jars
all: $(PROG)
CFLAGS = -g -Wall
list.o: list.c list.h
	$(CC) $(CFLAGS) -c $< -o list.o
exec.o: exec.c exec.h
	$(CC) $(CFLAGS) -c $< -o exec.o 
jars.o: jars.c
	$(CC) $(CFLAGS) -c $< -o jars.o 
$(PROG): list.o exec.o jars.o
	$(CC) $(CFLAGS) jars.o list.o exec.o -o $(PROG)

clean:
	rm -f *.o

val:
	valgrind --leak-check=full ./jars
