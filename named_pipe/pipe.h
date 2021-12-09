#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <sys/ipc.h> 
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#define MSG_SIZE 80
#define PIPEPATH1 "./named_pipe1"
#define PIPEPATH2 "./named_pipe2"
#define PIPEPATH3 "./named_pipe3"
#define PIPENAME "./named_pipe_file"
#define STDIN 0


