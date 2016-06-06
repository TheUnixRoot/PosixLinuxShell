/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell          
	(then type ^D to exit program)

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
	enum status status2;

	for(i = 1; i <= list_size(lista); i++){
		// itero por toda la lista de jobs preguntando que les ha 
		// pasado a los procesos miembros
		actual = get_item_bypos(lista, i);
		int pid_wait = waitpid(actual->pgid, &status, WUNTRACED|WNOHANG|WCONTINUED);
		if(pid_wait == actual->pgid) {
			// Este proceso ha recibido una señal
			// Lo he encontrado
			status2 = analyze_status(status, &info);
			print_analyzed_status(status2, info);
			printf("Signaled %d \n", info);
			fflush(stdout);
			if (status2 == EXITED) {
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
			} else if(status2 == CONTINUED){
				// asi sale cuando hago kill -18
				if(actual -> state != RESPAWNABLE) {
					// El proceso no es respawnable, 
					actual -> state = BACKGROUND;
					printf("CONTINUED\n");
					fflush(NULL);
				} 
				// si fuese respawnable, sigue igual, 
				// así que no haría nada
			} else if(status2 == SUSPENDED) {
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
					history linea = getIelem(historial, i);
					
					i = 0;
					while(linea -> args[i] != NULL) {
						strcpy(args[i], linea -> args[i]);
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


			pid_fork = fork();
			if (pid_fork) { 
				pthread_t thid;
				elem *theOne;		// usa el elemento de la lista de procesos
				int pgid = pid_fork;
				int when = atoi(args[1]);
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
					int status2;
					status2 = analyze_status(status, &info);
					printf("Comando %d ejecutado en foreground %s %s: \n", pid_fork, argumentosHijo[0], status2?"EXITED":"SUSPENDED");
					print_analyzed_status(status2, info);

					if(status2 == SUSPENDED) {
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
					printf("FG con argumentos %s \n", args[1]);
					pos = atoi(args[1]);
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
				
				int status2;
				status2 = analyze_status(status, &info);
				printf("Comando %d ejecutado en foreground %s %s: \n", aux -> pgid, aux -> command, status2?"EXITED":"SUSPENDED");
				print_analyzed_status(status2, info);
				
				if(status2 == SUSPENDED) {
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
					job *aux = get_item_bypos(lista, atoi(args[1]));
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
						if(aux -> state == STOPPED || aux -> state ==) {
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
		if (pid_fork) { 
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

				int status2;
				status2 = analyze_status(status, &info);
				printf("Comando %d ejecutado en foreground %s %s: \n", pid_fork, args[0], status2?"EXITED":"SUSPENDED");
				print_analyzed_status(status2, info);

				if(status2 == SUSPENDED) {
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
