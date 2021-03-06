/*--------------------------------------------------------
UNIX Shell Project
job_control module

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.
--------------------------------------------------------*/

#include "job_control.h"
#ifndef __APPLE__
#include <malloc.h>
#endif
#include <string.h>

// -----------------------------------------------------------------------
//  get_command() reads in the next command line, separating it into distinct tokens
//  using whitespace as delimiters. setup() sets the args parameter as a 
//  null-terminated string.
// -----------------------------------------------------------------------

void get_command(char inputBuffer[], int size, char *args[],int *background, int *respawnable, history *historial)
{
	int length, /* # of characters in the command line */
		i,      /* loop index for accessing inputBuffer array */
		start,  /* index where beginning of next command parameter is */
		ct;     /* index of where to place the next parameter into args[] */

	ct = 0;
	*background = 0;
	*respawnable = 0;

	/* read what the user enters on the command line */
	length = read(STDIN_FILENO, inputBuffer, size);  

	
	start = -1;
	if (length == 0)
	{
		printf("\nBye\n");
		exit(0);            /* ^d was entered, end of user command stream */
	} 
	if (length < 0){
		perror("error reading the command");
		exit(-1);           /* terminate with error code of -1 */
	}

	/* examine every character in the inputBuffer */
	for (i=0;i<length;i++) 
	{ 
		switch (inputBuffer[i])
		{
		case ' ':
		case '\t' :               /* argument separators */
			if(start != -1)
			{
				args[ct] = &inputBuffer[start];    /* set up pointer */
				ct++;
			}
			inputBuffer[i] = '\0'; /* add a null char; make a C string */
			start = -1;
			break;

		case '\n':                 /* should be the final char examined */
			if (start != -1)
			{
				args[ct] = &inputBuffer[start];     
				ct++;
			}
			inputBuffer[i] = '\0';
			args[ct] = NULL; /* no more arguments to this command */
			break;

		default :             /* some other character */

			if (inputBuffer[i] == '&') // background indicator
			{
				*background  = 1;
				if (start != -1)
				{
					args[ct] = &inputBuffer[start];     
					ct++;
				}
				inputBuffer[i] = '\0';
				args[ct] = NULL; /* no more arguments to this command */
				i=length; // make sure the for loop ends now

			} else if (inputBuffer[i] == '#') {
				*respawnable  = 1;
				if (start != -1)
				{
					args[ct] = &inputBuffer[start];     
					ct++;
				}
				inputBuffer[i] = '\0';
				args[ct] = NULL; /* no more arguments to this command */
				i=length; // make sure the for loop ends now					
			} else if (start == -1) {
				start = i;  // start of new argument
			}
		}  // end switch
	}  // end for   
	args[ct] = NULL; /* just in case the input line was > MAXLINE */

	addToHistory(historial, args, *background, *respawnable);
	
} 


// -----------------------------------------------------------------------
/* devuelve puntero a un nodo con sus valores inicializados,
devuelve NULL si no pudo realizarse la reserva de memoria*/
job * new_job(pid_t pid, const char * command, enum job_state state, char *args[], int numProc)
{	
	job * aux;
	if(args) {
		aux=(job *) malloc(sizeof(job));
		aux -> pgid=pid;
		aux -> state=state;
		aux -> numProc = numProc;
		aux -> command=strdup(command);
		aux -> next=NULL;
		aux -> args = (char**) malloc(sizeof(char*)*128);

		char ** doblePuntero = aux -> args;
		int i = 0;
		while(args[i] != NULL) {
			doblePuntero[i] = strdup(args[i]);
			i++;
		}
		// args[i] == NULL
		doblePuntero[i] = NULL;
	} else {
		aux=(job *) malloc(sizeof(job));
		aux -> pgid=pid;
		aux -> state=state;
		aux -> numProc = 0;
		aux -> command=strdup(command);
		aux -> next=NULL;
		aux -> args = NULL;
	}
	return aux;
}

// -----------------------------------------------------------------------
/* inserta elemento en la cabeza de la lista */
void add_job (job * list, job * item)
{
	job * aux=list->next;
	list->next=item;
	item->next=aux;
	list->pgid++;

}

// -----------------------------------------------------------------------
/* elimina el elemento indicado de la lista 
devuelve 0 si no pudo realizarse con exito */
int delete_job(job * list, job * item)
{
	job * aux=list;
	while(aux -> next != NULL && aux -> next != item) {
		aux=aux->next;
	}
	if(aux -> next) { // aux -> next == item
		aux -> next = item -> next;
		free(item -> command);

		int i = 0;
		while(item -> args[i] != NULL) {
			free(item -> args[i]);
			i++;
		}
		free(item -> args);
		
		free(item);
		list->pgid--;
		return 1;
	}
	else
		return 0;

}
// -----------------------------------------------------------------------
/* busca y devuelve un elemento de la lista cuyo pid coincida con el indicado,
devuelve NULL si no lo encuentra */
job * get_item_bypid  (job * list, pid_t pid)
{
	job * aux=list;
	while(aux->next!= NULL && aux->next->pgid != pid) aux=aux->next;
	return aux->next;
}
// -----------------------------------------------------------------------
job * get_item_bypos( job * list, int n)
{
	job * aux=list;
	if(n<1 || n>list->pgid) return NULL;
	n--;
	while(aux->next!= NULL && n) { aux=aux->next; n--;}
	return aux->next;
}

// -----------------------------------------------------------------------
/*imprime una linea en el terminal con los datos del elemento: pid, nombre ... */
void print_item(job * item)
{

	printf("pid: %d, command: %s, state: %s\n", item->pgid, item->command, state_strings[item->state]);
}

// -----------------------------------------------------------------------
/*recorre la lista y le aplica la funcion pintar a cada elemento */
void print_list(job * list, void (*print)(job *))
{
	int n=1;
	job * aux=list;
	printf("Contents of %s:\n",list->command);
	while(aux->next!= NULL) 
	{
		printf(" [%d] ",n);
		print(aux->next);
		n++;
		aux=aux->next;
	}
}

// -----------------------------------------------------------------------
/* interpretar valor estatus que devuelve wait */
enum status analyze_status(int status, int *info)
{
	if (WIFSTOPPED (status)) {
		*info=WSTOPSIG(status);
		return(SUSPENDED);
	} else if(WIFCONTINUED (status)) {
		*info=-1;
		return(CONTINUED);
	} else {
		// el proceso termio
		if (WIFSIGNALED (status)) { 
			*info=WTERMSIG (status);
			return(SIGNALED);
		} else { 
			*info=WEXITSTATUS(status); 
			return(EXITED);
		}
	}
	return;
}

// -----------------------------------------------------------------------
// cambia la accion de las seÃ±ales relacionadas con el terminal
void terminal_signals(void (*func) (int))
{
	signal (SIGINT,  func); // crtl+c interrupt tecleado en el terminal
	signal (SIGQUIT, func); // ctrl+\ quit tecleado en el terminal
	signal (SIGTSTP, func); // crtl+z Stop tecleado en el terminal
	signal (SIGTTIN, func); // proceso en segundo plano quiere leer del terminal 
	signal (SIGTTOU, func); // proceso en segundo plano quiere escribir en el terminal
}		

// -----------------------------------------------------------------------
void block_signal(int signal, int block)
{
	/* declara e inicializa máscara */
	sigset_t block_sigchld;
	sigemptyset(&block_sigchld );
	sigaddset(&block_sigchld,signal);
	if(block)
	{
		/* bloquea señal */
		sigprocmask(SIG_BLOCK, &block_sigchld, NULL);
	}
	else
	{
		/* desbloquea señal */
		sigprocmask(SIG_UNBLOCK, &block_sigchld, NULL);
	}
}

void print_analyzed_status(int status, int info) {
	if (status == SUSPENDED) {
		printf("Proceso suspendido %d\n", info);
	} else if(status == SIGNALED) {
		printf("El proceso ha recibido una señal %d\n", info);
	} else {
		printf("Proceso finalizado con codigo: %d\n", info);
	}
}

// -------------- FUNCTIONS TO MANAGE HISTORY ----------------------------

void addToHistory(history *lista, char *args2[], int bk, int resp) {
	if (args2[0]) {

		history new = (history) malloc(sizeof(struct history_));
		new -> prev = *lista;
		new -> background = bk;
		new -> respawnable = resp;

		// recorro y saco la longitud

		new -> args = (char**) malloc(sizeof(char*)*128);

		char ** aux = new -> args;
		int i = 0;
		while(args2[i] != NULL) {
			aux[i] = strdup(args2[i]);
			i++;
		}
		// args2[i] == NULL
		aux[i] = NULL;

		*lista = new;
	}
}

void clearHistory(history *lista) {
	history aux = NULL;
	while( (*lista) != NULL) {
		aux = *lista;
		*lista = (*lista) -> prev;
		int i = 0;
		while(aux -> args[i] != NULL) {
			free(aux -> args[i]);
			i++;
		}
		free(aux -> args);
		free(aux);
	}
	// *lista is NULL
}

history getIelem(history lista, int index) {
	int i = 0;
	while(i < index && lista != NULL) {
		lista = lista -> prev;
		i++;
	}
	if(lista) {
		//is not null, I have Ielem
		return lista;
	} else {
		return NULL;
	}
}

void showHistory(history lista) {
	int i = 0;
	while(lista != NULL) {
		printf("%d: ", i+1);
		int j = 0;
		char ** aux = lista -> args;
		while(aux[j] != NULL) {
			printf("%s ", aux[j]);
			j++;
		}
		if(lista -> background)
			printf("&");
		if (lista -> respawnable)
			printf("#");
		printf("\n");
		lista = lista -> prev;
		i++;
	}
}

void removeIelem(history *lista, int index) {
	int i = 0;
	history it = *lista;
	history aux = NULL;
	while (it != NULL && i < index) {
		aux = it;
		it = it -> prev;
		i++;
	}
	if(it) {
		// is not NULL
		aux -> prev = it -> prev;

		int i = 0;
		while(it -> args[i] != NULL) {
			free(it -> args[i]);
			i++;
		}
		free(it -> args);
		free(it);
	}
}

char** getArgs(history nodo) {
	return nodo -> args;
}


int length(history lista) {
	int i = 0;
	while(lista != NULL) {
		lista = lista -> prev;
		i++;
	}
	return i;
}

// ---------------------------------------------------------------------------------------------

/*------------------Procedimiento para calcular el numero de argumentos-------------------------*/
int longitudArgs(char ** args) {
	int i = 0;
	while(args[i] != NULL) {
		i++;
	}
	return i;
}
/*----------------------------------------------------------------------------------------------*/

// ------------ Function to show output of children command --------------

void showingChildren(){
	DIR *procDirectory;
	if(procDirectory = opendir("/proc")) {
	} else {
		printf("Error al abrir el directorio\n");
		return ;
	}
	struct dirent *procStruct;
	procStruct = readdir(procDirectory);
	while(procStruct != NULL) {
		pid_t pid = (pid_t)atoi(procStruct -> d_name);
		if (pid > 0) {
			// TENGO TODOS LOS PROCESOS DEL SISTEMA, 
			// UNO A UNO
			char *cadenaTemporal = (char*)malloc(sizeof(char)*100);
			DIR *processDirectory;
			sprintf(cadenaTemporal,"/proc/%d/task", pid);
			if(processDirectory = opendir(cadenaTemporal)) {
			} else {
				printf("Error al abrir el directorio\n");
				return ;
			}
			struct dirent *processStruct;
			processStruct = readdir(processDirectory);
			int i = 0;
			while(processStruct != NULL) {
				// printf("%s\n", processStruct -> d_name);
				if(atoi(processStruct -> d_name)) {
					i++;
				}
				processStruct = readdir(processDirectory);
			}
			
			closedir(processDirectory);
			FILE *processChild;
			sprintf(cadenaTemporal,"/proc/%d/task/%d/children", pid, pid);

			if(processChild = fopen(cadenaTemporal, "r")) {
			} else {
				printf("Error al abrir el fichero\n");
				return;
			}
			int nchild = 0;
			int dum;
			while(fscanf(processChild, "%d ", &dum) != EOF) {
				nchild++;
			}

			fclose(processChild);

			sprintf(cadenaTemporal,"/proc/%d/task/%d/comm", pid, pid);
			if(processChild = fopen(cadenaTemporal, "r")) {
			} else {
				printf("Error al abrir el fichero\n");
				return;
			}
			fscanf(processChild, "%s", cadenaTemporal);
			printf("%d\t%d\t\t%d\t\t%s\n", pid, nchild, i, cadenaTemporal);
			// fclose(processStat);
			free(cadenaTemporal);
		}
		procStruct = readdir(procDirectory);
	}
	closedir(procDirectory);
}

// -----------------------------------------------------------------------
