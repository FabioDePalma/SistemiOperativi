#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /*FOR fork, execve & alarm*/
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <time.h> /*FOR random number MACRO: CLOCK_REALTIME func: clock_gettime*/


#define POP_SIZE 500 /*Numeri processi*/
#define SIM_TIME 20
#define CHIAVE_URNA 567
#define CHIAVE_MSG_PARI 9999
#define CHIAVE_MSG_DISPARI 1987
#define CHIAVE_GRUPPI 1563
#define CHIAVE_SHM 1998

typedef struct info{
  int cur_idx;
  int voto_architettura[POP_SIZE];
}Info;

typedef struct gruppo{
  int pid[4];
  int voti[4];
  int a_c;
  int nof_elem;
}Gruppo;

typedef struct insieme{
  int indice;
  Gruppo gruppo[POP_SIZE];
  int fine;
}Insieme;


#define TEST_ERROR if(errno){\
        dprintf(STDERR_FILENO,"%s:%d: PID=%5d: Error %d (%s)\n", \
					               __FILE__, __LINE__, getpid(), errno, strerror(errno));}

/*DICHIARAZIONI DI FUNZIONI*/
int voto_architettura();
int *make_urna(int *, int *);
int preferenza();
void fine_tempo(int);
