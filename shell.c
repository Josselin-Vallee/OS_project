#define _XOPEN_SOURCE

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>


#define MAX_INPUT 80
#define MAX_PARAMS 10


/*Main loop of the shell*/
void shell_loop(void);
/*Utilitary fuction to parse the command line.*/
char **parseInput(char *);
/*method used to poll background processes for termination*/
void reapBgProcesses(void);
/*Utilitary function for command execution*/
int executeCmd(char **);
int regCommand(char **); 
int foregroundProcess(char **);
int backgroundProcess(char **);
int exitCommand(void);
int cdCommand(char **);
int checkEnvCommand(char **);
/*Utilitary function to print shell line*/
void printShell(void);
void linkToHandler(int signal, void(*handler)(int signal));




int main(void){

	/*main loop of the shell*/
	shell_loop();
	
	return EXIT_SUCCESS;
}

/*Loop of the shell.*/
void shell_loop(void){
	/*Variable for user input.*/
	char user_input[MAX_INPUT + 1];
	/*Array storing every argument of the command*/
	char **params;
	/*status of the shell*/
	int run = 1;
	
	/*Loop continues while run is not zero.*/
	while(run){
		/* Ignore incoming signals that would stop the shell without exit */
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTTOU, SIG_IGN);
	    
		
		/*Poll background processes.*/
		reapBgProcesses();
		
		/*Prompt shell message.*/
		printShell();
	
		/*Read input from standard input.*/
		if(fgets(user_input, MAX_INPUT, stdin)== NULL){
		    perror("fgets system call failed \n");
		    exit(1);
		}
	
		/*If enter was pressed, avoid unecessary computations. */
		if(user_input[0] == '\n'){
			continue;
		}else{
			/*Parse the input.*/
			params = parseInput(user_input);
			/*execute the command*/
			run = executeCmd(params);
		}
		
		/*Clear all input at end of loop*/
		strcpy(user_input, "");
		free(params);
	}
}

char **parseInput(char *user_input){
	/*pointer to current token*/
	char *token;
	/*Variables for size and position regarding the parameter array*/
	int size = MAX_PARAMS, pos = 0;
	/*parameter array*/
	char **par = malloc(size * sizeof(char *));
	/*Remove trailing \n.*/
	strtok(user_input, "\n");
	/*First token :*/
	token = strtok(user_input, " ");
	
	/*Loop that goes through user input and tokenizes it*/
	while(token != NULL){
		/*Stores token in params.*/
		par[pos] = token;
		/*Increases position in params.*/
		pos += 1;
		
		/*Check if we have more parameters than the limit : In this case realloc more memory.*/
		if(pos >= size){
			/*Add memory block of size MAX_PARAMS*/
			size += MAX_PARAMS;
			par = realloc(par, size * sizeof(char*));
			if (!par) {
				fprintf(stderr, "par: reallocation error\n");
				exit(EXIT_FAILURE);
			}
		}
		/*Recursion : obtain the next token.*/
		token = strtok(NULL, " ");
	}
	/*Insert a NULL mark at the end of the parameter array.*/
	par[pos] = NULL;
	/*Return the tokenized input.*/
	return par;
}


void reapBgProcesses(){
#if defined SIGDET && SIGDET==1
    /*Reap with signals*/
    printf("signals\n");
#else
	 /*Function polling for terminated child processes:
	 It will call itself recursively until it doesn't find any zombie processes.*/

	/*Variable for background process*/
	pid_t bg_pid;
	/*Status variable*/
	int status;
	/*Try reaping zombie processes.*/
	bg_pid = waitpid(-1, &status, WNOHANG);
	/*Didn't find any terminated process*/
	if(bg_pid < 0){ 
		if (errno==ECHILD){
			;
		}else {
			perror("Problem occured while checcking for terminated background processes.\n");
			exit(1);
		}
	}else if(bg_pid > 0){ /*Found zombie process.*/
		/*Check for exit status*/
		if (WIFEXITED(status)){
			/*Process terminated successfully : print message*/
			printf("Terminated background process : %d \n",bg_pid);
		} else {
			/*Process didn't terminate normally*/
			printf("Background process : %d an error occured during termination.\n", bg_pid);
		}
		/*recursion : while we find terminating processes, we search for more*/
		reapBgProcesses();
	}
#endif
}

/*Function that test for built-in function or regular call.*/
int executeCmd(char **par){
	/*First : Check for built-in commands.
	Exit command*/
	if(strcmp(par[0], "exit") == 0){
		return exitCommand();
	}else if(strcmp(par[0], "cd") == 0){ /*cd command*/
		return cdCommand(par);
	}else if(strcmp(par[0], "checkEnv") == 0){ /* checkEnv command*/
		return checkEnvCommand(par);
	}else{
		/*Not a built in command.*/
		return regCommand(par);
	}
}

/*Regular command*/
int regCommand(char **par){
	int last = 0;
	int bg;

	/*Go to last element.*/
	while(par[last] != NULL){
		last += 1;
	}
	/*Check if background process*/
	bg = (strcmp(par[last-1], "&") == 0);
	if(bg){
		/*Remove the &*/
		par[last-1] = NULL;
		return backgroundProcess(par);
	}else{
		return foregroundProcess(par);
	}
}

/*Foreground process*/
int foregroundProcess(char **par){
	/*Pids*/
	pid_t pid, wpid;
	int status;
	/*Time of start and finish of the process*/
	float tStart, tFinish, tStart_ms, tFinish_ms;
	/*
	{
		perror("Error occured in call of gettimeofday.\n"); 
		exit(1);
	}
	*/
	pid = fork();
    
	if(pid == 0){
	    signal(SIGINT, SIG_DFL);
	    signal(SIGQUIT, SIG_DFL);
	    signal(SIGTSTP, SIG_DFL);
	    signal(SIGTTIN, SIG_DFL);
	    signal(SIGTTOU, SIG_DFL);
	 
	   printf("Started foreground process : %d \n", getpid());		
	   if(execvp(par[0], par) == -1){
		perror("Can't execute foreground process \n");
		exit(1);
	    }
	}else if(pid == -1){
		perror("fork system call failed");
		exit(1);
	}else if(pid > 0){
	    signal(SIGINT, SIG_IGN);
	    signal(SIGQUIT, SIG_IGN);
	    signal(SIGTSTP, SIG_IGN);
	    signal(SIGTTIN, SIG_IGN);
	    signal(SIGTTOU, SIG_IGN);

	    wpid = waitpid(pid, &status, 0);
	    if(wpid == -1){
		perror("Wait system call failed \n");
		exit(1);
	    }
	    if(WIFEXITED(status)){
		printf("Terminated foreground process: %d \n", pid);
	    }else{
		printf("Foreground process : %d , did not terminate normally \n", pid);
	    }
	}
		
	return 1;
}

/*Background process: Executes the command without waiting for it.*/
int backgroundProcess(char **par){
	pid_t pid;
	pid = fork();
	if(pid == 0){ /*Child process : fork successful*/
	    if(execvp(par[0], par) == -1){ /*execute the command*/
		perror("Cannot execute background process\n");
		exit(1);
	    }
	}else if(pid == -1){ /*problem occured with the fork system call.*/
		perror("fork system call failed");
		exit(1);
	}else if (pid > 0){ /*Parent process : Background proces spawned successfully*/ 
		printf("Spawned background process : %d.\n", pid);
	}
	return 1;
}

/*Built-in command : exit.*/
int exitCommand(void){
	if(kill(0, SIGKILL) == -1){
	    perror("Failed killing active background processes upon exiting\n");
	    exit(1);
	}
	return 0;
}

/*Built-in command : cd*/
int cdCommand(char **par){
	if(chdir(par[1]) == -1){
		perror("Can't change to that directory.\n");
	}
	return 1;
}

/*Built-in command : checkEnv*/
int checkEnvCommand(char **par){
	printf("checkEnv command\n");
	return 1;
}

/*Utilitary functions for prompting
Simple shell string.*/
void printShell(){
	char* user = getenv("USER");
	printf("%s@shell > ", user);
}

void linkToHandler(int signal, void(*handler)(int signal)){
    struct sigaction settings;
    settings.sa_handler = handler;
    sigemptyset(&settings.sa_mask);
    settings.sa_flags =0;
    if(sigaction(signal, &settings, NULL) == -1){
	perror("Sigaction system call failed\n");
	exit(1);
    }
}

    
