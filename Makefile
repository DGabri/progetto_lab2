# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
CC=gcc
CFLAGS=-std=c11 -Wall -g -pthread # -O
LDLIBS=-lm -lrt -pthread


# su https://www.gnu.org/software/make/manual/make.html#Implicit-Rules
# sono elencate le regole implicite e le variabili
# usate dalle regole implicite

# Variabili automatiche: https://www.gnu.org/software/make/manual/make.html#Automatic-Variables
# nei comandi associati ad ogni regola:
#  $@ viene sostituito con il nome del target
#  $< viene sostituito con il primo prerequisito
#  $^ viene sostituito con tutti i prerequisiti

# elenco degli eseguibili da creare
#EXECS=archivio
#INCS=hashtable.h  include.h  xerrori.h
#OBJS=archivio.o  buffer.o  hashtable.o  xerrori.o

all: run_server

run_server: archivio
	/bin/rm -f valgrind-*
	./server.py 5 -r 2 -w 4 -v & 
	sleep 2
	./client2 file1 file2
	sleep 2
	./client1 file3
	pkill -INT -f server.py 


archivio: archivio.o buffer.o thread_functions.o hashtable.o xerrori.o
	$(CC) $(CFLAGS) -o $@ $^

archivio.o: archivio.c buffer.h hashtable.h include.h
	$(CC) $(CFLAGS) -c $<

buffer.o: buffer.c buffer.h
	$(CC) $(CFLAGS) -c $<

hashtable.o: hashtable.c hashtable.h
	$(CC) $(CFLAGS) -c $<

thread_functions.o: thread_functions.c thread_functions.h
	$(CC) $(CFLAGS) -c $<

killpy:
	pkill -INT -f server.py

clean: 
	rm -f archivio archivio.o buffer.o hashtable.o lettori.log server.log
