INCL = -I./

CC 	= gcc
#CFLAGS 	= $(INCL) -g -Wall -Wunused
CFLAGS 	= -g -Wall -Wunused

default: test_threadpool
ALL = test_threadpool
all: $(ALL)

######################################################

KP_STUFF			= ../kp_common.h ../kp_common.c ../kp_recovery.h ../kp_recovery.c
QUEUE_STUFF			= queue.c queue.h
THREADPOOL_STUFF	= threadpool.c threadpool.h

# Kind of hacky...
queue.o: $(QUEUE_STUFF) $(KP_STUFF) threadpool_macros.h
	$(CC) -c queue.c -o $@ $(CFLAGS)

threadpool.o: $(THREADPOOL_STUFF) threadpool_macros.h
	$(CC) -c threadpool.c -o $@ $(CFLAGS)

TP_OBJ	= queue.o threadpool.o

test_threadpool: test_threadpool.c $(TP_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -f *.o $(ALL)

