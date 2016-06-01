/*
 * File: timeout_pthread_sleep.c
 *
 * Compile: gcc threadTimeout.c threadTimeout.h -o timeout -pthread
 *
 * Author: Sergio Romero Montiel <sromero@uma.es>
 *
 * Created on May 20th, 2016
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct {
  pid_t   pgid;
	// datos adicionales para sleep()
  int time;
} elem;	// o bien, poner el puntero al timer en la lista de procesos


// el manejador, encargado de matar al grupo de procesos indicado
static void* killer(void *arg);