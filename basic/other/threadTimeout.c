#include "threadTimeout.h"
#include <pthread.h>

static void* killer(void *arg)
{
  elem *payload = arg;

  sleep(payload->time);
  killpg(payload->pgid,SIGTERM);	// envio TERM al grupo timed-out-ed!
}


int main(int argc, char *argv[])
{
  pthread_t thid;
  elem *theOne;		// usa esto o el elemento de la lista de procesos
  int pgid = atoi(argv[1]);
  int when = atoi(argv[2]);
  int read;
  // Incluir info del temporizador en la estructura de tipo elem
  theOne = (elem *)malloc(sizeof(elem));
  theOne->pgid = pgid; // a quien vamos a matar
  theOne->time = when; // y cuando lo vamos a hacer
  // abre el thread detached (para no hace join)
  if (0 == pthread_create(&thid,NULL,killer,(void *)theOne))
    pthread_detach(thid);
  else
    fprintf(stderr,"pthread_create: error\n");

  exit(EXIT_SUCCESS);
}

