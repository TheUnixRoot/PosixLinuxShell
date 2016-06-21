#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char const *argv[])
{
	DIR *procDirectory;
	if(procDirectory = opendir("/proc")) {
	} else {
		printf("Error al abrir el directorio\n");
		return -1;
	}
	struct dirent *procStruct;
	procStruct = readdir(procDirectory);
	while(procStruct != NULL) {
		pid_t pid = (pid_t)atoi(procStruct -> d_name);
		if (pid > 0) {
			// TENGO TODOS LOS PROCESOS DEL SISTEMA, 
			// UNO A UNO
			char *cadenaTemporal = (char*)malloc(sizeof(char)*100);
			
			/*
			 * Es el 4o parametro de /proc/pid/stat 
			 
			FILE *processStat;
			sprintf(cadenaTemporal,"/proc/%d/status", pid);
			
			if(processStat = fopen(cadenaTemporal, "r")) {
			} else {
				printf("Error al abrir el fichero stat\n");
				return -1;
			}
			size_t dummy;
			getline(&cadenaTemporal, &dummy, processStat);
			getline(&cadenaTemporal, &dummy, processStat);
			getline(&cadenaTemporal, &dummy, processStat);
			getline(&cadenaTemporal, &dummy, processStat);
			getline(&cadenaTemporal, &dummy, processStat);
			pid_t ppid;
			fscanf(processStat, "%s %d", cadenaTemporal, &ppid);
			 */
			// TENGO EL PROCESO Y SU PADRE
			// FALTAN LOS THREADS Y EL NOMBRE DEL PROCESO
			DIR *processDirectory;
			sprintf(cadenaTemporal,"/proc/%d/task", pid);
			if(processDirectory = opendir(cadenaTemporal)) {
			} else {
				printf("Error al abrir el directorio\n");
				return -1;
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
				return -1;
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
				return -1;
			}
			fscanf(processChild, "%s", cadenaTemporal);
			printf("%d\t%d\t%d\t%s\n", pid, nchild, i, cadenaTemporal);
			// fclose(processStat);
		}
		procStruct = readdir(procDirectory);
	}
	closedir(procDirectory);
	return 0;
}