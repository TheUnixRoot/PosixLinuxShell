/*--------------------------------------------------------
UNIX Shell Project
function prototypes, macros and type declarations for job_control module

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.
--------------------------------------------------------*/

#ifndef _JOB_CONTROL_H
#define _JOB_CONTROL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>


// ----------------------------------------------------------------------

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct {
  pid_t   pgid;
	// datos adicionales para sleep()
  int time;
} elem;	// o bien, poner el puntero al timer en la lista de procesos


// el manejador, encargado de matar al grupo de procesos indicado
static void* killer(void *arg);
// ----------------------------------------------------------------------
// ----------- ENUMERATIONS ---------------------------------------------
enum status { SUSPENDED, SIGNALED, EXITED, CONTINUED};			// GENERA SIGCHILD
enum job_state { FOREGROUND, BACKGROUND, STOPPED, RESPAWNABLE };
static char* status_strings[] = { "Suspended","Signaled","Exited","Continued" };
static char* state_strings[] = { "Foreground","Background","Stopped","Respawnable" };

// ----------- JOB TYPE FOR JOB LIST ------------------------------------
typedef struct job_
{
	pid_t pgid; /* group id = process lider id */
	char * command; /* program name */
	enum job_state state;
	struct job_ *next; /* next job in the list */
	char **args;
} job;

// ------------ HISTORY LINKED LIST TYPE ---------------------------------

typedef struct history_ *history;
struct history_
{
	history prev;
	int background;
	int respawnable;
	char** args; // args[0] = 
};

// -----------------------------------------------------------------------
//      PUBLIC FUNCTIONS
// -----------------------------------------------------------------------

// -------------- FUNCTIONS TO MANAGE HISTORY ----------------------------

void addToHistory(history *lista, char *args2[], int bk, int resp);

void clearHistory(history *lista);

history getIelem(history lista, int index);

void showHistory(history lista);

void removeIelem(history *lista, int index);

char** getArgs(history nodo);

int length(history lista);

// -----------------------------------------------------------------------

void get_command(char inputBuffer[], int size, char *args[],int *background, int *respawnable, history *historial);

job * new_job(pid_t pid, const char * command, enum job_state state, char **args);

void add_job (job * list, job * item);

int delete_job(job * list, job * item);

job * get_item_bypid  (job * list, pid_t pid);

job * get_item_bypos( job * list, int n);

enum status analyze_status(int status, int *info);

void print_analyzed_status(int status, int info);		// Mio

// -----------------------------------------------------------------------
//      PRIVATE FUNCTIONS: BETTER USED THROUGH MACROS BELOW
// -----------------------------------------------------------------------

void print_item(job * item);

void print_list(job * list, void (*print)(job *));

void terminal_signals(void (*func) (int));

void block_signal(int signal, int block);

// -----------------------------------------------------------------------
//      PUBLIC MACROS
// -----------------------------------------------------------------------

#define list_size(list) 	 list->pgid   // number of jobs in the list
#define empty_list(list) 	 !(list->pgid)  // returns 1 (true) if the list is empty

#define new_list(name) 			 new_job(0,name,FOREGROUND, NULL)  // name must be const char *

#define print_job_list(list) 	 print_list(list, print_item)

#define restore_terminal_signals()   terminal_signals(SIG_DFL)
#define ignore_terminal_signals() terminal_signals(SIG_IGN)

#define set_terminal(pgid)        tcsetpgrp (STDIN_FILENO,pgid)
#define new_process_group(pid)   setpgid (pid, pid)

#define block_SIGCHLD()   	 block_signal(SIGCHLD, 1)
#define unblock_SIGCHLD() 	 block_signal(SIGCHLD, 0)

// macro for debugging----------------------------------------------------
// to debug integer i, use:    debug(i,%d);
// it will print out:  current line number, function name and file name, and also variable name, value and type
#define debug(x,fmt) fprintf(stderr,"\"%s\":%u:%s(): --> %s= " #fmt " (%s)\n", __FILE__, __LINE__, __FUNCTION__, #x, x, #fmt)

// -----------------------------------------------------------------------
#endif

