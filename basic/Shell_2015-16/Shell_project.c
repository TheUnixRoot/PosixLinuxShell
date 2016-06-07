/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some (very few) code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o shell -pthread
   $ ./shell          
	(then type ^D to exit program, also writting exit)


**/

#include "job_control.h"   // remember to compile with module job_control.c 
#include <string.h>

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

// -----------------------------------------------------------------------
//                            MAIN          
// -----------------------------------------------------------------------

job *lista;	// lista es global porque se utiliza en la RTS de SIGCHLD y main

void my_sigchld(int signum) {	// manejador de SIGCHLD
	// printf("\nLlego un SIGCHLD\n");
	block_SIGCHLD();
	job *actual;
	int status;
	int info;
	int pid_wait;
	int i;
	enum status status_res;

	for(i = 1; i <= list_size(lista); i++){
		// itero por toda la lista de jobs preguntando que les ha 
		// pasado a los procesos miembros
		actual = get_item_bypos(lista, i);
		int pid_wait = waitpid(actual->pgid, &status, WUNTRACED|WNOHANG|WCONTINUED);
		if(pid_wait == actual->pgid) {
			// Este proceso ha recibido una señal
			// Lo he encontrado
			status_res = analyze_status(status, &info);
			print_analyzed_status(status_res, info);
			printf("Signaled %d \n", info);
			fflush(stdout);
			if (status_res == EXITED) {
				if(actual -> state != RESPAWNABLE) {
					// el proceso ha finalizado y no es respawnable
					delete_job(lista, actual);
					printf("EXITED\n");
					fflush(NULL);
					i--;
				} else {
					// la tarea que ha acabado es respawnable
					char **temp = actual -> args;
					pid_t pid_fork = fork();
					if (pid_fork) { 
						// código del padre


						// Nuevo grupo para mi hijo
						new_process_group(pid_fork);
			

						// Lo agrego a la lista de jobs
						actual -> pgid = pid_fork;
					} else { 
						// hijo
						// Redundante para garantizar su ejecucion
						
						// creo un nuevo grupo?
						new_process_group(getpid());
						

						restore_terminal_signals();
						execvp(temp[0], temp);
						printf("Error, ha existido algún fallo (nombre programa, permisos insuficientes, etc...)\n");
						exit(EXIT_FAILURE);
					}
					printf("RESPAWNED\n");
					fflush(NULL);
				}
			} else if(status_res == CONTINUED){
				// asi sale cuando hago kill -18
				if(actual -> state != RESPAWNABLE) {
					// El proceso no es respawnable, 
					actual -> state = BACKGROUND;
					printf("CONTINUED\n");
					fflush(NULL);
				} 
				// si fuese respawnable, sigue igual, 
				// así que no haría nada
			} else if(status_res == SUSPENDED) {
				if(actual -> state != RESPAWNABLE) {
					// cambio el estado de la tarea a stopped
					actual -> state = STOPPED;
					printf("STOPPED");
					fflush(NULL);
				} else {
					// la tarea suspendida es respawnable, 
					// la relanzo
					killpg(actual -> pgid, SIGCONT);
					printf("RESPAWNED\n");
					fflush(NULL);
				}




			/******/
			} else {
				// SIGNALED
				if(actual -> state != RESPAWNABLE) {
					if (info == 19) {
						actual -> state = STOPPED;
					} else if(info == 9 || info == 15) {

						delete_job(lista, actual);
						i--;
					}
				} else {
					if (info == 19) {
						// la tarea suspendida es respawnable, 
						// la relanzo
						killpg(actual -> pgid, SIGCONT);
						printf("RESPAWNED\n");
						fflush(NULL);
					} else if(info == 9 || info == 15) {
						// la tarea que ha acabado es respawnable
						char **temp = actual -> args;
						pid_t pid_fork = fork();
						if (pid_fork) { 
							// código del padre
							// Nuevo grupo para mi hijo
							new_process_group(pid_fork);
			

							// Lo agrego a la lista de jobs
							actual -> pgid = pid_fork;
						} else { 
							// hijo
							// Redundante para garantizar su ejecucion
							// creo un nuevo grupo
							new_process_group(getpid());
							

							restore_terminal_signals();
							execvp(temp[0], temp);
							printf("Error, ha existido algún fallo (nombre programa, permisos insuficientes, etc...)\n");
							exit(EXIT_FAILURE);
						}
					}
				}
				printf("SIGNALED\n");
				fflush(NULL);
			/******/
			}
		}
	}
	printf("Señal tratada\n");
	fflush(NULL);
	unblock_SIGCHLD();
}
/*----------------------Procedimiento para el thread time-out----------------------------------*/
static void* killer(void *arg)
{
  elem *payload = arg;

  sleep(payload->time);
  killpg(payload->pgid,SIGTERM);	// envio TERM al grupo timed-out-ed!
}
/*----------------------------------------------------------------------------------------------*/
int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; 	/* pid for created and waited process */
	int status;             	/* status returned by wait */
	enum status status_res; 	/* status processed by analyze_status() */
	int info;					/* info processed by analyze_status() */

	int respawnable;			/* Variable para controlar procesos lanzados en modo respawnable */
	history historial = NULL;	/* Lista que controla el historial de comando ejecutados en la sesion */

	job *nuevo, *aux;
	lista = new_list("Jobs list");
	signal(SIGCHLD, my_sigchld);

	ignore_terminal_signals();
	
	/* 
	 * Incluye:
	 * Nombre de usuario @ Nombre máquina :/path>
	 * hola -> print saludo
	*/
	char cwd[100];
	char user[40];
	getlogin_r(user, 40);
	char pc[40];
	gethostname(pc,40);
	getcwd(cwd, 100);

	char * oldpwd = getenv("OLDPWD");
	char * home = getenv("HOME");
	/* Variables iniciales del entorno, utilizadas para el comando interno cd */


	while (1) {		/* Program terminates normally inside get_command() after ^D is typed*/

		printf("%s@%s:%s>", user, pc, cwd);	
		/* Impresion de entrada de comandos, muestra el usuario, el nombre del host y el directorio actual */
		fflush(stdout);
		
		get_command(inputBuffer, MAX_LINE, args, &background, &respawnable, &historial);  /* get next command */
		
		if(args[0]==NULL) continue;   // if empty command

		if (strcmp(args[0], "history") == 0) {
			// comprueba y ejecuta el comando interno history
			// sin argumentos, muestra el histórico de comandos de la sesion
			// acepta argumentos:
			// -clear para limpiar el historial
			// -remove i para eliminar una entrada concreta
			if(args[1]) {
				// quiere una operación en concreto
				if(strcmp(args[1], "-remove") == 0) {	// -remove i
					int i = 0;
					if(args[2]) {
						i = atoi(args[2]);
						removeIelem(&historial, i);
						continue;
					} else {
						printf("Error, elemento no introducido al eliminar entrada del historial\n");
					}
				} else if (strcmp(args[1], "-clear") == 0) {	
					// -clear
					clearHistory(&historial);
					continue;
				} else {	// execute i
					int i = atoi(args[1]);
					if(i > length(historial) || i < 0){
						printf("Error, el argumento del historial no es un numero valido\n");
						continue;
					}
					history linea = getIelem(historial, i);
					i = 0;
					while(linea -> args[i] != NULL) {
						args[i] = linea -> args[i];
						i++;
					}
					args[i] = NULL;		


					background = linea -> background;
					respawnable = linea -> respawnable;
					
					printf("%s \n", args[0]);
				}
				// no hay continue porque así al modificar args, ejecuto el comando
				// deseado del historial 
			} else {
				// history sin argumentos, por defecto
				// muestra todo el historico
				showHistory(historial);
				continue;
			}
		}
		// redirección a entrada o salida de fichero
		int j = 0;
		while((strcmp(args[j], ">") != 0) && (strcmp(args[j], "<") != 0) && (args[j+1] != NULL)) {
			j++;
		}
		if(args[j+1] == NULL){	// do nothing, no hay redirección
		} else if(strcmp(args[j],"<") == 0) {

			// toca redirigir la entrada 
			FILE *infile;
			int fnum1,fnum2;
			
			printf("redirección de entrada\n");
			
			pid_fork = fork();
			/*-----------padre-----------*/
			if(pid_fork) {
				// código del padre
				// --->
				struct termios conf;	// configuracion antes de lanzar job
				int shell_terminal; 	// descriptor de fichero del terminal
				shell_terminal = STDIN_FILENO;
    			/* leemos la configuracion actual */
   				tcgetattr(shell_terminal, &conf);
				// <---
			
				// Nuevo grupo para mi hijo
				new_process_group(pid_fork);
			
				if(background) {
					// Lo agrego a la lista de jobs
					printf("Background job running... pid: %d comando: %s \n", pid_fork, args[0]);
					nuevo = new_job(pid_fork, args[0], BACKGROUND, args);
					block_SIGCHLD();
					// Evito que me interrumpan
					add_job(lista, nuevo);
					unblock_SIGCHLD();
					// libero la lista

				} else if (respawnable) {
					// Lo agrego a la lista de jobs
					printf("Respawnable job running at background... pid: %d comando: %s \n", pid_fork, args[0]);
					nuevo = new_job(pid_fork, args[0], RESPAWNABLE, args);
					block_SIGCHLD();
					// Evito que me interrumpan
					add_job(lista, nuevo);
					unblock_SIGCHLD();
					// libero la lista

				} else {
					// Le cedo el terminal
					set_terminal(pid_fork);

					pid_wait = waitpid(pid_fork, &status, WUNTRACED);
					// pid_wait MUST BE EQUALS TO pid_fork
					
					// Le quito el terminal
					set_terminal(getpid());

		
					status_res = analyze_status(status, &info);
					printf("Comando %d ejecutado en foreground %s %s: \n", pid_fork, args[0], status_res?"EXITED":"SUSPENDED");
					print_analyzed_status(status_res, info);

					if(status_res == SUSPENDED) {
						// Lo agrego a la lista de jobs
						nuevo = new_job(pid_fork, args[0], STOPPED, args);
						block_SIGCHLD();
						// Evito que me interrumpan
						add_job(lista, nuevo);
						unblock_SIGCHLD();
						// libero la lista
					}
				}
				// --->
				tcsetattr(shell_terminal, TCSANOW, &conf);
				// <---
				continue;
			/*------------hijo-----------*/
			} else {
				if (NULL==(infile=fopen(args[j+1],"r"))) {
					printf("\tError: abriendo: %s\n",args[j+1]);
					return(-1);
				}
				args[j] = NULL;
				// termino la estructura de argumentos en el elemento de redirección, 
				// porque los argumentos están antes seguro

				fnum1=fileno(infile);
				fnum2=fileno(stdin);
				dup2(fnum1,fnum2);

				new_process_group(getpid());
				if(!background && !respawnable) {
					set_terminal(getpid());
				}
				restore_terminal_signals();
				execvp(args[0], args);
				printf("Error, ha existido algún fallo (nombre programa, permisos insuficientes, etc...)\n");
				exit(EXIT_FAILURE);
			}
			// done
		} else if(strcmp(args[j], ">") == 0) {
			
			// toca redirigir la salida
			FILE *outfile;
			int fnum1,fnum2;
			
			pid_fork = fork();
			/*-----------padre-----------*/
			if(pid_fork) {
			// código del padre
			// --->
			struct termios conf;	// configuracion antes de lanzar job
			int shell_terminal; 	// descriptor de fichero del terminal
			shell_terminal = STDIN_FILENO;
    		/* leemos la configuracion actual */
   			tcgetattr(shell_terminal, &conf);
			// <---
			
			// Nuevo grupo para mi hijo
			new_process_group(pid_fork);
			
			if(background) {
				// Lo agrego a la lista de jobs
				printf("Background job running... pid: %d comando: %s \n", pid_fork, args[0]);
				nuevo = new_job(pid_fork, args[0], BACKGROUND, args);
				block_SIGCHLD();
				// Evito que me interrumpan
				add_job(lista, nuevo);
				unblock_SIGCHLD();
				// libero la lista

			} else if (respawnable) {
				// Lo agrego a la lista de jobs
				printf("Respawnable job running at background... pid: %d comando: %s \n", pid_fork, args[0]);
				nuevo = new_job(pid_fork, args[0], RESPAWNABLE, args);
				block_SIGCHLD();
				// Evito que me interrumpan
				add_job(lista, nuevo);
				unblock_SIGCHLD();
				// libero la lista

			} else {
				// Le cedo el terminal
				set_terminal(pid_fork);

				pid_wait = waitpid(pid_fork, &status, WUNTRACED);
				// pid_wait MUST BE EQUALS TO pid_fork
				
				// Le quito el terminal
				set_terminal(getpid());

	
				status_res = analyze_status(status, &info);
				printf("Comando %d ejecutado en foreground %s %s: \n", pid_fork, args[0], status_res?"EXITED":"SUSPENDED");
				print_analyzed_status(status_res, info);

				if(status_res == SUSPENDED) {
					// Lo agrego a la lista de jobs
					nuevo = new_job(pid_fork, args[0], STOPPED, args);
					block_SIGCHLD();
					// Evito que me interrumpan
					add_job(lista, nuevo);
					unblock_SIGCHLD();
					// libero la lista
				}
			}
			// --->
			tcsetattr(shell_terminal, TCSANOW, &conf);
			// <---
			continue;
			/*------------hijo-----------*/
			} else {
				if (NULL==(outfile=fopen(args[j+1],"w"))) {
					printf("\tError: abriendo: %s\n",args[j+1]);
					return(-1);
				}
				args[j] = NULL;
				// termino la estructura de argumentos en el elemento de redirección, 
				// porque los argumentos están antes seguro

				fnum1=fileno(outfile);
				fnum2=fileno(stdout);
				dup2(fnum1,fnum2);

				new_process_group(getpid());
				if(!background && !respawnable) {
					set_terminal(getpid());
				}
				restore_terminal_signals();
				execvp(args[0], args);
				printf("Error, ha existido algún fallo (nombre programa, permisos insuficientes, etc...)\n");
				exit(EXIT_FAILURE);
			}
			// done
		}
		if (strcmp(args[0], "children") == 0) {
			printf("PID\tCOMMAND\t#CHILDREN\t#THREADS\n");
			// TODO
			continue;
		}
		/*
		 * MIS PIPES
		 * NO PERMITEN
		 * EJECUCION EN
		 * BACKGROUD
		 */
		// enod
		j = 0;
		int argc = longitudArgs(args);
		int descf[2], anterior[2];
		int numPipes = 0;
		while(j < argc) {	// cuenta los pipes
			if((strcmp(args[j], "|") == 0))
				numPipes++;
			j++;
		}
		if(numPipes > 0) {	// parece que hay un pipe
				
			
			int posicionesPipes[numPipes];
			j = 0;
			int k = 0;
			while(j < argc) {	// ubica los pipes
				if((strcmp(args[j], "|") == 0))
					posicionesPipes[k++] = j;
				j++;
			}
			// posicionesPipes tiene un array con el numPipes y su ubicacion
			k = 0; j = 0;
			int out = 0;
			while(k < numPipes && !out) {	// valido las posiciones de los pipes
				j = posicionesPipes[k];
				if(j < 1 || j == argc) {
					printf("Error al introducir los pipes, fallo de posiciones %d\n", j);
					out = 1;
				}
				k++;
			}
			if(out) {	// error en pipes, reiniciar bucle
				continue;
			} else {	// tenemos pipes, vamos a ponernos a trabajar
				// TERMIOS WHEREEEEE
				// reclio la variable out
				out = 0;
				for (k = 0; k < numPipes; ++k) {
					pipe(descf);
					j = posicionesPipes[k];	// almaceno en temporal la ubicación del k-esimo pipe
					pid_fork = fork();
					//-----------padre-----------
					if (pid_fork) {
						// sobreescribo el array de posiciones con pids de hijos
						posicionesPipes[k] = pid_fork;
						//

						close(anterior[0]);
						close(anterior[1]);
					//-----------hijo------------
					} else {
						if(k > 0) {				// no es el primer pipe
							dup2(anterior[0],fileno(stdin));
							close(anterior[1]);
						}						// es el primer pipe

						if(k < numPipes) {		// no es el ultimo pipe
							dup2(descf[1],fileno(stdout));
							close(descf[0]);
						}						// es el ultimo pipe
						if(numPipes == 1) {		// es el unico pipe, pongo la salida y fuera hago la entrada
							dup2(descf[1],fileno(stdout));
							close(descf[0]);
						}
						args[j] = NULL;
						restore_terminal_signals();
						execvp(args[out], &args[out]);
						printf("Error, ha existido algún fallo (nombre programa, permisos insuficientes, etc...)\n");
						exit(EXIT_FAILURE);
						// k-esimo hijo creado
					}
					anterior[0] = descf[0];
					anterior[1] = descf[1];
					out = j+1;	// guardo en out el start del siguiente comando
					// loopping again
				}
				// me queda 1 proceso desde out hasta argc
				pipe(descf);
				pid_fork = fork();
				//-----------padre-----------
				if (pid_fork) {
					close(anterior[0]);
					close(anterior[1]);
					//-----------hijo------------
				} else {
					dup2(anterior[0],fileno(stdin));
					close(anterior[1]);
					// args[out] es el ultimo comando, termina en args[argc] que ya es NULL
					restore_terminal_signals();
					execvp(args[out], &args[out]);
					printf("Error, ha existido algún fallo (nombre programa, permisos insuficientes, etc...)\n");
					exit(EXIT_FAILURE);
				}
				// ultimo hijo creado
				
				// Nuevo grupo para mi hijo
				new_process_group(pid_fork);
				for (k = 0; k < numPipes; ++k) {	// meto a todos los hijos en el mismo grupo
					setpgid (posicionesPipes[k], pid_fork);
				}
				// entrego terminal a los procesos
				set_terminal(pid_fork);
				// wait del ultimo hijo
				pid_wait = waitpid(pid_fork, &status, WUNTRACED);
				// pid_wait MUST BE EQUALS TO pid_fork
				if(pid_wait == pid_fork) {	// ha muerto el ultimo, 
					// tengo que hacer wait de todo el array de hijos
					out = 0;
					j = 0;
					while(j < numPipes) {
						waitpid(posicionesPipes[j], &status, WUNTRACED);
						j++;
					}		
				}
				// Le quito el terminal
				set_terminal(getpid());

				printf("Pipe finalizado con exito\n");
				continue;
			}	
			// done
		}
		if (strcmp(args[0], "hola") == 0) {
			printf("%s\n", "Hello world");
			continue;
		}
		if (strcmp(args[0], "time-out") == 0) {
			// comando interno para lanzar una tarea en modo time-out
			// no controla el lanzamiento de una tarea respawnable en time-out
			// debido a la naturaleza del respawnable, es absurdo, con lo que el 
			// time-out prevalece, eliminando la cualidad respawnable
			// args[1] tiene los segundos de vida del job
			// args[2] tiene el comando
			// args[3-inf] tienen los parametros
			pthread_t thid;
			elem *theOne;		// usa el elemento de la lista de procesos
			int pgid = pid_fork;
			int when = atoi(args[1]);
			if(when < 0){
				printf("Error, el argumento de time-out no es un numero valido\n");
				continue;
			}
			pid_fork = fork();
			if (pid_fork) { 
				
				// Incluir info del temporizador en la estructura de tipo elem
				theOne = (elem *)malloc(sizeof(elem));
				theOne->pgid = pgid; // a quien vamos a matar
				theOne->time = when; // y cuando lo vamos a hacer
				// abre el thread detached (para no hace join)
				if (0 == pthread_create(&thid,NULL,killer,(void *)theOne))
					pthread_detach(thid);
				else
					fprintf(stderr,"time-out fallido\n");
				// código del padre
				// --->
				struct termios conf;	// configuracion antes de lanzar job
				int shell_terminal; 	// descriptor de fichero del terminal
				shell_terminal = STDIN_FILENO;
	    		/* leemos la configuracion actual */
	   			tcgetattr(shell_terminal, &conf);
				// <---
				
				// Nuevo grupo para mi hijo
				new_process_group(pid_fork);
				char** argumentosHijo = &args[2];
					
				if(background || respawnable) {
					// Lo agrego a la lista de jobs
					printf("Background job running... pid: %d comando: %s \n", pid_fork, argumentosHijo[0]);
					nuevo = new_job(pid_fork, argumentosHijo[0], BACKGROUND, argumentosHijo);
					block_SIGCHLD();
					// Evito que me interrumpan
					add_job(lista, nuevo);
					unblock_SIGCHLD();
					// libero la lista

				} else {
					// Le cedo el terminal
					set_terminal(pid_fork);

					pid_wait = waitpid(pid_fork, &status, WUNTRACED);
					// pid_wait MUST BE EQUALS TO pid_fork
					
					// Le quito el terminal
					set_terminal(getpid());
		
					status_res = analyze_status(status, &info);
					printf("Comando %d ejecutado en foreground %s %s: \n", pid_fork, argumentosHijo[0], status_res?"EXITED":"SUSPENDED");
					print_analyzed_status(status_res, info);

					if(status_res == SUSPENDED) {
						// Lo agrego a la lista de jobs
						nuevo = new_job(pid_fork, argumentosHijo[0], STOPPED, argumentosHijo);
						block_SIGCHLD();
						// Evito que me interrumpan
						add_job(lista, nuevo);
						unblock_SIGCHLD();
						// libero la lista
					}
				}
				// --->
				tcsetattr(shell_terminal, TCSANOW, &conf);
				// <---
			} else if(pid_fork < 0) {
				printf("Error al crear el proceso (memoria insuficiente, límite de procesos, etc...)\n");
				exit(EXIT_FAILURE);
			
			} else { // hijo
				// Redundante para garantizar su ejecucion
				new_process_group(getpid());
				if(!background) {
					set_terminal(getpid());
				}
				restore_terminal_signals();
				char** argumentosHijo = &args[2];
				execvp(*argumentosHijo, argumentosHijo);
				printf("Error, ha existido algún fallo (nombre programa, permisos insuficientes, etc...)\n");
				exit(EXIT_FAILURE);
			}
			continue;
		}
		if (strcmp(args[0], "cd") == 0) {
			int e;
			if(args[1]) {	// path absoluto
				if(strcmp(args[1], "-") == 0) {
					chdir(oldpwd);
					strcpy(oldpwd, cwd);
				} else {
					getcwd(oldpwd, 100);
					e = chdir(args[1]);
				}
			} else {		// path por defecto
				getcwd(oldpwd, 100);
				e = chdir(home);
			}
			if(e) 
				printf("Dirección errónea, inexistente o faltan permisos de acceso\n");
			else {
				getcwd(cwd, 100);
				setenv("OLDPWD", oldpwd, 1);	// sets OLDPWD variable with overwritting
			}
			continue;
		}
		if (strcmp(args[0], "jobs") == 0) {
			// muestra la lista de tareas
			print_job_list(lista);
			continue;
		}
		if(strcmp(args[0], "exit") == 0) {
			// comando interno para cerrar el shell
			// alternativa a Ctrl + D
			if(args[1])
				exit(atoi(args[1]));
			exit(0);
			continue;
		}
		if (strcmp(args[0], "fg") == 0) {
			// quiero lanzar en foreground una tarea, bien suspendida bien en background
			if(empty_list(lista)) {
				printf("No existen tareas\n");
			} else {
				// existen tareas en la lista de jobs
				int pos;
				if(args[1] != NULL) {
					// quiero una tarea en concreto
					// printf("FG con argumentos %s \n", args[1]);
					pos = atoi(args[1]);
					if(pos > list_size(lista) || pos < 1){
						printf("Error, el argumento de fg no es un numero valido\n");
						continue;
					}
				} else {
					// por defecto lanza la primera tarea
					// independientemente del estado de la misma
					pos = 1;
				}
				// entrega terminal y enciende
				
				// --->
				struct termios conf;	// configuracion antes de lanzar job
				int shell_terminal; 	// descriptor de fichero del terminal
				shell_terminal = STDIN_FILENO;
    			/* leemos la configuracion actual */
   				tcgetattr(shell_terminal, &conf);
				// <---
				
				// printf("%d\n", pos);
				block_SIGCHLD();
				job *aux = get_item_bypos(lista, pos);
				unblock_SIGCHLD();

				set_terminal(aux -> pgid);
				killpg(aux -> pgid, SIGCONT);
				
				pid_wait = waitpid(aux -> pgid, &status, WUNTRACED);

				// pid_wait MUST BE EQUALS TO pid_fork
				
				// Le quito el terminal
				set_terminal(getpid());

				// --->
				tcsetattr(shell_terminal, TCSANOW, &conf);
				// <---
				
	
				status_res = analyze_status(status, &info);
				printf("Comando %d ejecutado en foreground %s %s: \n", aux -> pgid, aux -> command, status_res?"EXITED":"SUSPENDED");
				print_analyzed_status(status_res, info);
				
				if(status_res == SUSPENDED) {
					// Lo agrego a la lista de jobs
					block_SIGCHLD();
					// Evito que me interrumpan
					aux -> state = STOPPED;
					unblock_SIGCHLD();
					// libero la lista
				} else {
					block_SIGCHLD();
					delete_job(lista, aux);
					unblock_SIGCHLD();
				}
			}
			continue;
		}
		if (strcmp(args[0], "bg") == 0) {
			if(list_size(lista)) {
				// Tiene elementos
				if(args[1]) {
					// como tiene argumentos, lo saco de la lista
					// segun la posicion puesta
					block_SIGCHLD();
					int i = atoi(args[1]);
					if(i > list_size(lista) || i < 1){
						printf("Error, el argumento de bg no es un numero valido\n");
						continue;
					}
					job *aux = get_item_bypos(lista, i);
					unblock_SIGCHLD();
					if (aux -> state == BACKGROUND) {
						printf("Proceso ya en background\n");
					} else {
						// está suspendido
						killpg(aux -> pgid, SIGCONT);
						aux -> state = BACKGROUND;
					}
				} else {
					// es NULL
					// recorrer la lista hasta el primero en STOPPED O RESPAWNABLE
					// enciende sin entregar terminal
					block_SIGCHLD();
					int i = 1, terminar = 1;
					while((i <= list_size(lista)) && terminar) {
						job *aux = get_item_bypos(lista, i);
						if(aux -> state == STOPPED || aux -> state == RESPAWNABLE) {
							killpg(aux -> pgid, SIGCONT);
							aux -> state = BACKGROUND;
							terminar = 0;
						} else 
							i++;
					}
					if(terminar) {
						printf("No hay tareas suspendidas, todas estan en BACKGROUND\n");
					}
					unblock_SIGCHLD();
				}

			} else {
				printf("No existen tareas\n");
			}
			continue;
		}

		// comandos externos restore_terminal_signals();
		pid_fork = fork();
		if (pid_fork) { // código del padre
			// --->
			struct termios conf;	// configuracion antes de lanzar job
			int shell_terminal; 	// descriptor de fichero del terminal
			shell_terminal = STDIN_FILENO;
    		/* leemos la configuracion actual */
   			tcgetattr(shell_terminal, &conf);
			// <---
			
			// Nuevo grupo para mi hijo
			new_process_group(pid_fork);
			
			if(background) {
				// Lo agrego a la lista de jobs
				printf("Background job running... pid: %d comando: %s \n", pid_fork, args[0]);
				nuevo = new_job(pid_fork, args[0], BACKGROUND, args);
				block_SIGCHLD();
				// Evito que me interrumpan
				add_job(lista, nuevo);
				unblock_SIGCHLD();
				// libero la lista

			} else if (respawnable) {
				// Lo agrego a la lista de jobs
				printf("Respawnable job running at background... pid: %d comando: %s \n", pid_fork, args[0]);
				nuevo = new_job(pid_fork, args[0], RESPAWNABLE, args);
				block_SIGCHLD();
				// Evito que me interrumpan
				add_job(lista, nuevo);
				unblock_SIGCHLD();
				// libero la lista

			} else {
				// Le cedo el terminal
				set_terminal(pid_fork);

				pid_wait = waitpid(pid_fork, &status, WUNTRACED);
				// pid_wait MUST BE EQUALS TO pid_fork
				
				// Le quito el terminal
				set_terminal(getpid());

	
				status_res = analyze_status(status, &info);
				printf("Comando %d ejecutado en foreground %s %s: \n", pid_fork, args[0], status_res?"EXITED":"SUSPENDED");
				print_analyzed_status(status_res, info);

				if(status_res == SUSPENDED) {
					// Lo agrego a la lista de jobs
					nuevo = new_job(pid_fork, args[0], STOPPED, args);
					block_SIGCHLD();
					// Evito que me interrumpan
					add_job(lista, nuevo);
					unblock_SIGCHLD();
					// libero la lista
				}
			}
			// --->
			tcsetattr(shell_terminal, TCSANOW, &conf);
			// <---
			continue;
		} else if(pid_fork < 0) {
			printf("Error al crear el proceso (memoria insuficiente, límite de procesos, etc...)\n");
			exit(EXIT_FAILURE);
		} else { // hijo
			// Redundante para garantizar su ejecucion
			new_process_group(getpid());
			if(!background && !respawnable) {
				set_terminal(getpid());
			}
			restore_terminal_signals();
			execvp(args[0], args);
			printf("Error, ha existido algún fallo (nombre programa, permisos insuficientes, etc...)\n");
			exit(EXIT_FAILURE);
		}
	} // end while
}
