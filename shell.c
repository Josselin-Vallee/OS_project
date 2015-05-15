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



int main(int argc, char *argv[]){
	
	shell_loop();
	
	return EXIT_SUCCESS;
}

//Loop of the shell.
void shell_loop(void){
	//Variable for user input.
	char user_input[MAX_INPUT + 1];
	//Array storing every argument of the command
	char **params;
	//status of the shell
	int run = 1;
	
	//Loop continues while run is not zero.
	while(run){
		//Poll background processes.
		pollBgProcesses();
		
		//Prompt shell message.
		printShell();
	
		//Read input from standard input.
		fgets(user_input, MAX_INPUT, stdin);
	
		//If enter was pressed, avoid unecessary computations. 
		if(user_input[0] == '\n'){
			continue;
		}else{
			//Parse the input.
			params = parseInput(user_input);
			//execute the command
			run = executeCmd(params);
		}
		
		//Clear all input 
		stpcpy(user_input, "");
		free(params);
	}
}

char **parseInput(char *user_input){
	//pointer to current token
	char *token;
	//Variables for size and position regarding the parameter array
	int size = MAX_PARAMS, pos = 0;
	//parameter array
	char **par = malloc(size * sizeof(char *));
	//Remove trailing \n.
	strtok(user_input, "\n");
	//First token :
	token = strtok(user_input, " ");
	
	//Loop that goes through user input and tokenizes it
	while(token != NULL){
		//Stores token in params.
		par[pos] = token;
		//Increases position in params.
		pos += 1;
		
		//Check if we have more parameters than the limit : In this case realloc more memory.
		if(pos >= size){
			//Add memory block of size MAX_PARAMS
			size += MAX_PARAMS;
			par = realloc(par, size * sizeof(char*));
			if (!par) {
				fprintf(stderr, "par: reallocation error\n");
				exit(EXIT_FAILURE);
			}
		}
		//Recursion : obtain the next token.
		token = strtok(NULL, " ");
	}
	//Insert a NULL mark at the end of the parameter array.
	par[pos] = NULL;
	//Return the tokenized input.
	return par;
}

void pollBgProcesses(){
	pid_t bg_pid;
	int status;
	bg_pid = waitpid(-1, &status, WNOHANG);
	if(bg_pid < 0){
		
	}else if(bg_pid > 0){
		//recursion : while we find terminating processes, we search for more
		pollBgProcesses();
	}

}

int executeCmd(char **par){
	//First : Check for built-in commands.
	//Exit command
	if(strcmp(par[0], "exit") == 0){
		return exitCommand();
	}else if(strcmp(par[0], "cd") == 0){ //cd command
		return cdCommand(par);
	}else if(strcmp(par[0], "checkEnv") == 0){ // checkEnv command
		return checkEnvCommand(par);
	}else{
		//Not a built in command.
		return regCommand(par);
	}
}

int regCommand(char **par){
	int last = 0;
	//Go to last element.
	while(par[last] != NULL){
		last += 1;
	}
	//Check if background process
	int bg = (strcmp(par[last-1], "&") == 0);
	if(bg){
		//Remove the &
		par[last-1] = NULL;
		return backgroundProcess(par);
	}else{
		return foregroundProcess(par);
	}
}

int foregroundProcess(char **par){
	//Pids
	pid_t pid, wpid;
	//Time of start and finish of the process
	float tStart, tFinish;
	printf("foreground process");
	return 1;
}

int backgroundProcess(char **par){
	pid_t pid;
	printf("background process");
	return 1;
}

int exitCommand(void){
	printf("exit command");
	return 0;
}

int cdCommand(char **par){
	printf("cd command");
	return 1;
}

int checkEnvCommand(char **par){
	printf("checkEnv command");
	return 1;
}

//Utilitary functions for prompting
//Simple shell string.
void printShell(){
	char* user = getEnv("USER");
	printf("%s@shell > ", user);
}

