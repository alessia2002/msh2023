/*-
 * main.c
 * Minishell C source
 * Shows how to use "obtain_order" input interface function.
 *
 * Copyright (c) 1993-2002-2019, Francisco Rosales <frosal@fi.upm.es>
 * Todos los derechos reservados.
 *
 * Publicado bajo Licencia de Proyecto Educativo Práctico
 * <http://laurel.datsi.fi.upm.es/~ssoo/LICENCIA/LPEP>
 *
 * Queda prohibida la difusión total o parcial por cualquier
 * medio del material entregado al alumno para la realización
 * de este proyecto o de cualquier material derivado de este,
 * incluyendo la solución particular que desarrolle el alumno.
 *
 * DO NOT MODIFY ANYTHING OVER THIS LINE
 * THIS FILE IS TO BE MODIFIED
 */

#include <stddef.h> /* NULL */
#include <stdio.h>	/* setbuf, printf */
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/times.h>
#include <time.h>  
#include <pwd.h>
#include <glob.h>

#define buffer 1000


extern int obtain_order(); /* See parser.y for description */


char * statusMsg;  //mensaje de los mandatos internos
//int maskFichero = 0666;

//Necesario para mandato interno time
static struct tms shellTimes;
static clock_t realTimeInicio;
static clock_t userTimeInicio;
static clock_t systemTimeInicio;

/*Mandatos Internos*/
//??MAndar los pensajes de error 


//Conversion de tick a sec
int aSegundos(clock_t time){

	#ifndef CLK_TCK
		int CLK_TCK = sysconf(_SC_CLK_TCK);
	#endif

	return (int)(time/CLK_TCK);
}

int aMilisegundos(clock_t time){

	#ifndef CLK_TCK
		int CLK_TCK = sysconf(_SC_CLK_TCK);
	#endif

	return (int)((time%CLK_TCK)*1000L / CLK_TCK);
}

int cd(char **argv)
{	
	int ret;
	char * path=calloc(1000, sizeof(char));
	if(!argv[1]) 
	{	//no tiene argumentos 
		ret = chdir(getenv("HOME"));
		statusMsg= strerror(errno);
		if(ret == 0)
			printf("%s\n", getcwd(path, 1000*sizeof(char)));
	}
	else if(!argv[2])	//Solo un argumento !!!CORRECTO
	{
		ret = chdir(argv[1]);
		statusMsg=strerror(errno);
		if(ret == 0)
			printf("%s\n", getcwd(path, 1000*sizeof(char)));
	}else    //mas de un argumenro 
	{
		free(path);
		statusMsg = "Demasiados argumentos";
		return -1;
		
	}
	free(path);
	return ret; 
}

/*
	Mandato interno umask
*/
int umaskI(char ** argv){	
	mode_t oldMask;  //almacenara la antigua mascara 
	mode_t newMask; //nueva mascara
	if(!argv[1]) 
	{	//no tiene argumentos 
		oldMask = umask(0000); //hacemos un cambio parcial para obtener la mascara actual 
		printf("%o\n", oldMask); 
		umask(oldMask); //restablecemos la mascara original. 
	}
	else if(!argv[2]) 	//Solo un argumento !!!CORRECTO
	{
		errno = 0;
		newMask = strtol(argv[1], NULL, 8);
		if(errno!=0){
			statusMsg= strerror(errno);
			return -1; 
		}else if(newMask==00){
			statusMsg = "Mascara incorrecta, se requiere valor en base 8";
			return -1;
		}
		umask(newMask);
		printf("%o\n", newMask);

	}else{
		statusMsg = "Demasiados argumentos";
		return -1;
	}
	return 0; 
}



//Imprime el tiempo de uso de la minishell si se llama sin argumentos o el tiempo que tarda en ejecutarse el mandato pasado como argumento. 
int my_time(char ** argv){
	//struct tms shellTimes;
	if(!argv[1]) 
	{	//no tiene argumentos 
		clock_t realActual = times(&shellTimes) -realTimeInicio;
		clock_t userActual = shellTimes.tms_utime + shellTimes.tms_cutime -(userTimeInicio);
		clock_t systemActual= shellTimes.tms_stime + shellTimes.tms_utime - (systemTimeInicio);

		printf("%d.%03du %d.%03ds %d.%03dr\n", aSegundos(userActual), aMilisegundos(userActual) ,aSegundos(systemActual),aMilisegundos(systemActual), aSegundos(realActual), aMilisegundos(realActual));
      
	}
	else  	//uno o mas argumento 
	{
		struct tms internoTime; 
		clock_t real = -times(&internoTime);
		clock_t user = -(internoTime.tms_utime + internoTime.tms_cutime);
		clock_t systemT = -(internoTime.tms_stime + internoTime.tms_cstime);
		int pid;
		
		if((pid=fork())==-1){
			statusMsg= strerror(errno);
			return -1;
		}else if(pid==0){
			execvp(argv[1], &argv[1]);
			statusMsg= strerror(errno);
			return -1;
		}else{
			int status;		
			waitpid(pid ,&status, 0);
			real += times(&internoTime);
			user += (internoTime.tms_utime+internoTime.tms_cutime);
			systemT += (internoTime.tms_stime + internoTime.tms_cstime);
			printf("%d.%03du %d.%03ds %d.%03dr\n", aSegundos(user), aMilisegundos(user) ,aSegundos(systemT),aMilisegundos(systemT), aSegundos(real), aMilisegundos(real));     
		}
	
	}

	return 0;
}


/*
Mandato interno read que lee una linea de la entrada estandar y da valor a las variables de entorno 
Se puede leer como maximo 1000bytes de la entrada estandar (tamanyo buffer)
*/

int readI(char ** argv){
	int i = 0; //contador posicion en buffer
	int j = 1;  //contador del numero de variable
	char read;  //caracter leido 
	char bufferI[1001]; //guardamos lo leido (tamanyo maximo del valor de una variable es 1000B)
	char * valor; 
	int isSpaced = 1; 



	if(!argv[1] ){//No variables
		if(fgets(bufferI, buffer, stdin)==NULL){
			statusMsg= strerror(errno);
			return -1;
		}
		return 0;
	}
	
	while((read = fgetc(stdin))!='\n'){//implemtacion lectura caracter a caracter no se utiliza strtok() 
		if(argv[j+1]){
			if(isSpaced){
				if(read ==' ' || read=='\t') continue;
				else {
					isSpaced = 0; //comienza nuevo valor 
					i=0;
					bufferI[i]=read; 
					i++;
				}
				
			}else{
				if(read == ' ' || read=='\t'){
					isSpaced = 1; //separacion de token
					bufferI[i]='\0';
					valor = malloc(strlen(bufferI)+strlen(argv[j])+2);
					strcpy(valor, argv[j]);//nombre de la variable
					strcat(valor, "=");
					strcat(valor, bufferI);	//valor de la variable que hemos leido 
					if(putenv(valor)){
						statusMsg= strerror(errno);
						free(valor);
						return -1;
					}	
					j++;
					i=0;
				}else{
					bufferI[i]=read;
					i++;
				}

			}

		}else{
			if(i==0 && (read == ' ' || read=='\t')) continue;
			bufferI[i]=read;
			i++;
		}
	}

	bufferI[i]='\0';
	valor = malloc(strlen(bufferI)+strlen(argv[j])+2);
	strcpy(valor, argv[j]); 
	strcat(valor, "=");
	strcat(valor, bufferI);	
	if(putenv(valor)){
		statusMsg= strerror(errno);
		free(valor);
		return -1;
	}	
	return 0;

}



/*Comprobacion de que es un mandato interno para tratar de manera distinta su ejecucion*/
int isInterno(char *argv)
{
	return strcmp(argv, "cd") == 0 || strcmp(argv, "umask") == 0 || strcmp(argv, "time") == 0 || strcmp(argv, "read") == 0;
}


/*ejecuta la funcion correspondiente a cada mandato interno*/
int ejecutarInterno(char **argv){
	int ret; 
	if(strcmp(argv[0], "cd") == 0)
	{
		ret = cd(argv);
	}else if(strcmp(argv[0], "umask") == 0)
	{
		ret = umaskI(argv);
	}else if(strcmp(argv[0], "time") == 0)
	{
		ret= my_time(argv);
	}else{
		ret = readI(argv);
	}
	return ret;

}

/*
 Expansion de ~
*/
char * expandirTilde(char * start){
	char *res, *temp; //almacena el resultado expandido y temp es para la variable de entorno HOME
	int extended = 0; //tamanyo del nombre de usario.
	char *user; //guradara el nombre del ususario. 

	if(start){//despues de la tilde sigue
		user = calloc(1, strlen(start)+1);
		sscanf(start,"%[_a-zA-Z0-9]", user);
		if(strlen(user)==0){//No ususario
			free(user);
			if(!(temp =getenv("HOME"))){
				return NULL;
			}
		}else{
			extended = strlen(user);
			start += extended; //lo que siga tras el nombre de usario. 
			struct passwd *tempS = getpwnam(user);
			free(user);


			if(!tempS){//Usuario no existente, no expandimos
				return start-extended-1;
			}
			temp = tempS->pw_dir;
		}
	}else{
		if(!(temp =getenv("HOME"))){//Error al cargar la variable HOME
			return NULL;
		}
	}

	
		
	if(start){
		res = calloc(1 ,strlen(start)+strlen(temp)+1);
		res = strcpy(res, temp);
		strcat(res, start);
	}else{
		res = calloc(1 ,strlen(temp)+1);
		res = strcpy(res, temp);
	}
	
	return res; 
}

char *expandirDolar(char *start, char *sub){

	char *res, *temp, *var;
	var = calloc(1, strlen(++sub)+1);
	sscanf(sub,"%[_a-zA-Z0-9]", var);


	int extended = strlen(var);


	//Cadena a concatenar despues de la expansion
	sub = sub+extended;

	if(!(temp =getenv(var))){
		free(var);
		return NULL;
	}
	free(var);
 	res = malloc(strlen(start)+strlen(temp)+strlen(sub)+1);

	//Construimos el nuevo argumento
	res = strcpy(res, start);
	strcat(res, temp);
	strcat(res, sub);

	return res; 
}

char ** expandirTexto(char **argv, int pos){
	glob_t *match = calloc(1, sizeof(glob_t)); 
	int i, j; 

	if((i = glob(argv[pos], 0, NULL, match))){
		if(i == GLOB_NOMATCH){
			free(match);
			return argv;
		}
		free(match);
		return NULL;//caso ERROR llamada a glob
	}
	
	
	i=1;
	while(argv[pos+i]){//contamos los argumentos que quedan. 
		i++;
	}

	int numExpandidos = match->gl_pathc;
	int numPunteros = pos +i+numExpandidos;
	char ** newArgv = calloc(numPunteros, sizeof(char *));

	//asignamos argv anteriores a la expansion 
	for(j=0; j<pos; j++){
		newArgv[j] = argv[j];
	}

	//asignamos a argv las extensiones
	for(j=0; j<numExpandidos; j++){
		char *file=calloc(1, strlen((match->gl_pathv)[j])+1);
		strcpy(file,(match->gl_pathv)[j]);
		newArgv[pos+j]=file;
	}

	//asignamos los argv posteriores a la extension 
	for(j=1; j<i; j++){
		newArgv[pos+numExpandidos+j-1]=argv[pos+j];
	}

	globfree(match);
	free(match);

	return newArgv;
}


/*Funcion que detecta los  metacaracteres y llama a la funcion de extension correspondiente*/
int expansionMetacaracteres(char *** argvv){

	//buscamos los caracteres especiales ~ , $, ? 
	int contador = 0;
	int caracter; 
	char * reading;
	char ** argv;
	int cMandatos=0;

	while((argv = argvv[cMandatos])){
		contador=0;
		while((reading = argv[contador])){
			for(caracter=0; reading[caracter]; caracter++ ){
			       if(caracter == 0 && reading[caracter]=='~'){	
					if(!(argv[contador]=expandirTilde(++reading))){
						return -1;
					}			
				}else if(reading[caracter]=='$'){ //variables
				 reading[caracter]='\0';//marcamos el fin del inicio antes de la expansion.
					if(!(argv[contador]=expandirDolar(reading, reading+caracter)))
							return -1;

				
				}else if(reading[caracter]=='?' && !(reading[caracter-1]=='/')){//expansion de ficheros		
					if(!(argvv[cMandatos]=expandirTexto(argv, contador))){
						return -1;
					}
				}	
			}
			contador++;
		}
		cMandatos++;
	}
	return 0;
}




/*Ejecucion de la minishell*/
int main(void)
{

	char ***argvv = NULL;
	int argvc;
	char **argv = NULL;
	// int argc;
	char *filev[3] = {NULL, NULL, NULL};
	int bg;
	int ret;

	int in;	 // almacenaremos la entrada estandar en caso de redireccion
	int out; // salida
	int err; // salida error
	int status;//almacena el estado de ejecucion 


		/*variables de entorno*/
	//en estas variables almacenamos los valores que se pasaran a la funcion putenv()
	char *bgpidE = calloc(1, 32);// bgpid=---------------- 
	char *prompt = calloc(1, 100);
	char *mypid = calloc(1, 32);
	char *statusE =calloc(1,20); 

	setbuf(stdout, NULL); /* Unbuffered */
	setbuf(stdin, NULL);

	// guardar entrada tras pipe
	int pipeIn;
	int pipeBool = 0;


	int bgpid;
	// enmascaramiento de senyales
	// falta tratar fallos

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGQUIT);
	sigaddset(&mask, SIGINT);
	sigprocmask(SIG_SETMASK, &mask, NULL);


	strcpy(prompt,"msh> ");
	sprintf(mypid,"mypid=%i", getpid());
	putenv(mypid);
	putenv(prompt);


	/*Utilidades para el mandaro interno time*/
	realTimeInicio = times(&shellTimes);
	userTimeInicio = shellTimes.tms_utime + shellTimes.tms_cutime;
	systemTimeInicio = shellTimes.tms_stime + shellTimes.tms_cstime;
	


	while (1)
	{
		fprintf(stderr, "%s", "msh> "); /* Prompt */
		ret = obtain_order(&argvv, filev, &bg);
		
		if (ret == 0)
			break; /* EOF */
		if (ret == -1)
			continue;	 /* Syntax error */
		argvc = ret - 1; /* Line */
		if (argvc == 0)
			continue; /* Empty line */

		if (filev[0])
		{									   // redireccion de la entrada estandar
			int fd = open(filev[0], O_RDONLY); // abrimos un descriptor de fichero

			if (fd < 0)
			{
				perror("Error en la entrada estandar: ");
				continue;
			}

			in = dup(0); // duplicamos entrada estandar

			if (dup2(fd, 0) == -1)
			{
				perror("Redireccion entrada estandar");
				continue;
			}

			if (close(fd) == -1)
				continue; // cerramos fd
		}

		if (filev[1])
		{									// redireccion de la salida estandar
			int fd = creat(filev[1], 0666); // abrimos un descriptor de fichero
			if (fd < 0)
			{
				perror("creat");
				continue;
				;
			}

			out = dup(1); // duplicamos salida estandar

			if (dup2(fd, 1) == -1)
			{
				perror("Redireccion salida estandar");
				continue;
			}
			if (close(fd) == -1)
				continue; // cerramos fd
		}

		if (filev[2])
		{									// redireccion de la salida de error
			int fd = creat(filev[2], 0666); // abrimos un descriptor de fichero
			if (fd < 0)
			{
				perror("creat");
				continue;
				;
			}

			err = dup(2); // duplicamos salida estandar

			if (dup2(fd, 2) == -1)
			{
				perror("Redireccion salida de error");
				continue;
			}

			if (close(fd) == -1)
				continue; // cerramos fd
		}

		pipeBool=0;

		if(expansionMetacaracteres(argvv)==-1) continue; //expandimos todos los comandos;

		/*Comenzamos estructura de pipe con hijos*/
		for (argvc = 0; ((argv = argvv[argvc]) && argvc < ret - 2); argvc++)
		{
			
			int fildes[2];
			if (pipe(fildes) == -1)
				perror("pipe");
			int cod = fork();
			if (cod == -1)
				perror("Error Fork");
			else if (cod == 0)
			{ // Proceso hijo

				// cambiamos mascara de los procesos hijos si no background
				if (!bg)
				{
					sigemptyset(&mask);
					sigprocmask(SIG_SETMASK, &mask, NULL);
				}

				close(fildes[0]);	// hijo cierra pipe de lectura
				dup2(fildes[1], 1); // redirige salida estandar al pipe
				close(fildes[1]);
				if(isInterno(argv[0]))
				{
					int ret = ejecutarInterno(argv);
					exit(ret);
				}else{
					execvp(argv[0], argv); // ejecucion del proceso
					perror(argv[0]);
					exit(-1);
				}
			}
			else
			{
				waitpid(-1, &status, WNOHANG); //para evitar la acumulacion de procesos zombie pero sin bloquear la minishell
				if (!pipeBool) //contol de que ha ocurrido pipe a la vez que guardamos el descriptor de fichero original de entrada. 
				{
					pipeBool = 1;
					pipeIn = dup(0);
				}

				close(fildes[1]);	// padre cierra escritura para que proximo hijo herede
				dup2(fildes[0], 0); // redirige su entrada estandar al pipe
				close(fildes[0]);
			}
			// for (argc = 0; argv[argc]; argc++)
			// printf("%s ", argv[argc]);
		}

		/*Ultimo hijo*/
		// comprobamos si es mandato interno
		
		if (!isInterno(argv[0]))
		{		
			bgpid = fork();
			if (bgpid == -1)
				perror("Error Fork");
			else if (bgpid == 0)
			{ // Proceso hijo
				// cambiamos mascara de los procesos hijos
				if (!bg)
				{
					sigemptyset(&mask);
					sigprocmask(SIG_SETMASK, &mask, NULL);
				}
				execvp(argv[0], argv); // ejecucion del proceso
				
				perror(argv[0]);
				exit(1);
			}
			else
			{
				if (!bg)
				{
					waitpid(bgpid, &status, 0);
					sprintf(statusE, "status=%i", status);
					putenv(statusE);//variable de entorno status 
				}
			}
		}else if(bg){    //es interno y se ejecuta en backgrounf
			bgpid = fork();
			if (bgpid == -1)
				perror("Error Fork");
			else if (bgpid == 0)
			{ // Proceso hijo
				int ret = ejecutarInterno(argv);
				exit(ret);
			}
			//Minishell no hace nada, no tiene que esperar al hijo 
		}else{ //es interno y no se ejecuta en background
			if((status=ejecutarInterno(argv))==-1)
			{
				printf("%s\n", statusMsg);
			};

			
			sprintf(statusE, "status=%i", status);
			putenv(statusE);//variable de entorno status 
		}



		if (pipeBool)
		{
			dup2(pipeIn, 0);
			close(pipeIn);
		}

		/*terminamos con los hijos, restablecemos las condiciones iniciales de la shell*/
		if (filev[0])
		{
			if (dup2(in, 0) == -1)
			{
				perror("dup2");
				exit(-1);
			} //??como tratar el error ????
			if (close(in) == -1)
			{
				perror("Close");
				exit(-1);
			}
		}

		if (filev[1])
		{
			if (dup2(out, 1) == -1)
			{
				perror("dup2");
				exit(-1);
			}
			if (close(out) == -1)
			{
				perror("Close");
				exit(-1);
			}
		}

		if (filev[2])
		{
			if (dup2(err, 2) == -1)
			{
				perror("dup2");
				exit(-1);
			}
			if (close(err) == -1)
			{
				perror("Close");
				exit(-1);
			}
		}

		if (bg)
		{ // imprimimos mensaeje en caso de background
			printf("[%d]\n", bgpid);
			sprintf(bgpidE, "bgpid=%i", bgpid);
			putenv(bgpidE);//variable de entorno bgpid
		}

		

		// if (filev[0]) printf("< %s\n", filev[0]);/* IN */
		// if (filev[1]) printf("> %s\n", filev[1]);/* OUT */
		// if (filev[2]) printf(">& %s\n", filev[2]);/* ERR */
		// if (bg) printf("&\n");
	}
	exit(0);
	return 0;
}





