/*
*
* NAME:
*   miniShell is a program which emulates a shell. Both background and foreground processes can be
*   played out, and are given representation in the form of output on PID, initiation and termination. * *
*   Foreground processes are timed. Background processes can be nested within each other
*   without a need for extant processes to terminate first. Other features exist, e.g. two built-in commands,
*   disabling of <Ctrl-C> and its signal so <Ctrl-C> doesn't terminate processes.
*   
* SYNTAX:
*   $./miniShell.o 
*   Minishell prompt is produced: miniShell$
*   miniShell$[legit bash command with up to five options]
*
* DESCRIPTION:
*		The program executes differently depending on whether the user requests a background process
*   (command line ends with "&") or a foreground process (no final "&"). Three code blocks, A, B and C, serve
*   different purposes.
*   Code block A performs wait() on any extant background processes so that no zombie processes can persist 
*   long after their termination. A is executed only on the condition that one or more background processes can *   be seen to exist. A performs as many wait()'s as there are extant background processes. A makes sure that *   any background process's termination gets represented in output, no later than immediately after the latest *   carriage return.
*   Code block B manages background processes (final "&" on command line). A context switch is performed, the
*   shell command is executed, and the process is automatically placed in a zombie state. 
*   Code block C manages foreground processes (no final "&"). Context switch, execution of shell command and   
*   wait() all happen in sequence for one process. An execution time metric is calculated and output.
*   One code portion, in the middle, without a letter, is common to all alternatives, and is traversed every  
*   time the user enters a bash command. It manages the built-in commands ("exit" and "cd") so they work
*   as intended, and handles empty input (with an automatic line feed). The other thing it does is to parse
*   user input so the result, placed in an argument vector of strings, can be transferred to execvp() - the    
*   command, the command again, and (a maximum of) five consequent options. The first string, containing the *   command, is in a separate variable - all to accommodate execvp():s standard input format.
*
* EXAMPLES:
*   miniShell$ ls -a -b -c -d -f
* 	  produces output similar to   
*   Started foreground process 3430 
*   .
*   Terminated foreground process 3430 
*   Elapsed command execution wallclock time: 5.590000 ms
*		miniShell$
*
*		   Background processes can be nested, and contain foreground processes at the innermost level: 
*   miniShell$ sleep 5 &
*   Spawned background process 3517 
*   miniShell$ sleep 3 &
*   Spawned background process 3518 
*   Terminated background process 3517 
*   miniShell$ tty
*   Started foreground process 3519 
*   /dev/pts/0
*   Terminated foreground process 3519 
*   Elapsed command execution wallclock time: 21.118000 ms
*   miniShell$ 
*   Terminated background process 3518 
*   miniShell$ 
*			if the user will be minimally gentle in waiting for a prompt to appear.
*
* ENVIRONMENT: 
*   The program, for its proper function, needs implementing so that the home directory's value is available 
*   in an environment variable.
*   
* NOTES:
*   The A-block has a minimal delay instruction, sleep(1), close to the end. This is in order to ensure that 
*   the prompt is output at the right time for the user, irrespective of whether the child or the parent
*   process executes first (which is a stochastical matter). The arrangement is meant to be esthetical. It
*   also  means that the application is not 100% scalable for higher throughput. The only other way to
*   resolve this problem is through a full-fledged signal handler - not part of ID2200's syllabus for the lab. 
*
*		<Ctrl-C> gets disabled per SIG_IGN during child process execution, and otherwise produces a carriage
*		return, just as it would in a regular shell.
*
*		Code for handling time metrics is consistently placed in the parent process, even though the measurement
*   object, the shell command, is in the child process. This is necessary for consistency - only the parent
*   process's variables are persistent, the child process's disappear on termination. This implies an 
*   overestimation of the child process's execution time because fork(), waitpid(), conditionals &c. get    
*   included in the measurement. Because these instructions execute in nanoseconds, while shell commands
*   execute in milliseconds, the effect should be minimal. An indication of this is that shell command sleep 
*   executes in a time very minimally different from the expected. 
*/

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

int not_finished = 1;														/* Program execution terminates on reset            */
                                                /* of this variable through built in command 'exit' */

int options_exist;                              /* This variable helps facilitate background        */
																	              /* process execution                                */

int status, ret_val;                            /* Variable 'status' needs checking after wait() -  */
																								/* ret_val is sigaction()'s designated return       */
																								/* variable, for error handling &c.                 */

int i,x;																				/* Counters																					*/

int number_of_pending_background_processes = 0; /* This variable enables execution of A-block if >0 */

int regular_prompt = 1;													/* This variable toggles so the user always gets		*/
																								/* a fresh prompt irrespective of process type -    */
																								/* even if a pending background process is present  */

char command[71];																/* First parameter to execvp() - function parse()   */
																								/* sets the parameter value                         */

char user_input[71];														/* Variable where the user's stdin input goes       */

char *argument_vector[6];												/* Second parameter to execvp() - function parse()  */
																								/* sets the parameter value (command + 5 options)   */

char *last;																			/* Variable that minds whether last option is "&"   */

pid_t pid;																			/* PID's need storing for inspection/representation */
pid_t bg_pid;

struct timeval before, after, tv;								/* Foreground processes are timed, data go in these */
																								/* system-defined structs, see NOTES                */

float time_before_ms, time_after_ms;						/* Interim variables for calculation of foreground  */
float time_difference;													/* process execution time                           */
int time_before, time_after;

/* Function divvies up user input to a format that  */
/* execvp() can accept                              */
/* N. B. not a pure function, affects global        */
/* variables, e.g. argument_vector[]                */

void parse(char user_input[71])									
{																								
		int i=1;		/* Counter for vector elements*/																		
		char *p;		/* Auxiliary char pointer to help link strings into vector */																
		options_exist = 0;											
		strtok(user_input, "\n");	
		p=strcpy(command,strtok(user_input, " "));
		argument_vector[0] = p;
		while (((p = strtok(NULL, " "))!=NULL)&&(i<6))	/* Heavy lifting occurs here, vector is filled */
		{																								/* with string content from user's input       */
			options_exist = 1;
			argument_vector[i] = p;
			i++;
			last = p;
		}
}

/* Input needs cutting down to zero as soon as it   */
/* has been parsed, in order to forestall conflict  */
/* N. B. not a pure function, affects global        */
/* variables, e.g. argument_vector[]                */

void clear_input()														  
{																								
	for (i=0;i<6;i++)
		argument_vector[i]=(char *)0;
	strcpy(user_input,"");
	strcpy(command,"");
} 

/* Function determines whether input is "exit" which  */
/* terminates whole program, logic 1 or 0 returned    */

int identical_with_exit(char s[71])							
{																								
		if (0==strncmp(s,"exit",4))
			return 1;
		else
			return 0;
}
/* Function determines whether input is "cd" which  */
/* actuates other built-in command for change       */
/* of directory, logic 1 or 0 returned              */

int identical_with_cd(char s[71])								
{																								
		if (0==strncmp(s,"cd",2))										
			return 1;
		else
			return 0;
}

/* Function links signal to signal handler as per   */
/* standard operating procedure in POSIX            */

void link_to_handler(int signal, void(*handler)(int sig)) 
{																								
	int ret_val;										/* Contains sigaction's return value for error handling */
	struct sigaction settings;			/* Standard system datatype for signal linkage          */
	settings.sa_handler = handler;	/* Intraneous fields are set here and below             */
	sigemptyset(&settings.sa_mask);
	settings.sa_flags=0;
	ret_val = sigaction(signal,&settings,(void *) 0);
	if (-1 == ret_val)
	{perror("Can't perform sigaction()"); exit (1);}
}

/* Signal is linked to carriage return */
void first_signal_handler(int signal)						
{
	printf("\n");
}

int main(int argc, char *argv[])
{

	link_to_handler(SIGINT, first_signal_handler); /* <Ctrl-C>/SIGINT gets its specific handler */

	do
	{	

		/* A-block which wait():s on background processes (if any exist) begins here ********************/
	  /************************************************************************************************/
		if (number_of_pending_background_processes > 0)
		{
			int i;																										/* Counter 											 */

			for (i=0;i<=number_of_pending_background_processes;i++)		/* Heavy lifting occurs here, 	 */
			{																													/* wait()'ing on all background  */
				bg_pid = waitpid(-1, &status,WNOHANG);									/* processes in turn             */
				if (bg_pid<0)
				{
					if (errno==ECHILD)																		/* No process to wait() on means */
						;																										/* no actual error               */
					else
					{
						perror("Can't perform wait\n");
						exit(1);
					}
				}
				else if (0==bg_pid)																			/* Process not ready to terminate */
					;
				else if (bg_pid>0)																			/* Termination takes place        */
				{
					regular_prompt = 1;
					if (WIFEXITED(status))
						printf("%s %d %s","Terminated background process",bg_pid,"\n");
					else
						printf("%s %d %s","Background process ",bg_pid," did not terminate normally\n");
					number_of_pending_background_processes--;
				}	
			} 
			//sleep(1);																								/* Ugly sleep() is needed for ordering */
																															/* prompt and other output right       */

			if ((0==regular_prompt))																/* Prompting gets done even if         */
				printf("miniShell$ ");																/* background process/-es are pending  */
		} 
		/* A-block concludes here ******************************************************************************/		
			
		/* Code portion which executes every time do-loop is traversed, irrespective of other conditions *******/
	  /*******************************************************************************************************/
		if (1==regular_prompt)		
				printf("miniShell$ ");
			fgets(user_input,70,stdin);
			if (1==strlen(user_input))							/* Carriage return results as in a regular shell       */
			{
				continue;
			}	
			else
			if (identical_with_exit(user_input))		/* User exits, whole do-loop terminates                */
			{
				not_finished=0;
				break;
			}
			else if (strlen(user_input)>1)
			{
				parse(user_input);
				if (identical_with_cd(command))				/* Built-in cd command is employed                     */
				{
					if (-1==chdir(argument_vector[1]))
					{
						perror("Can't change to that directory - trying to change to HOME directory\n");
						if (-1==chdir(getenv("HOME")))
						{
							perror("Can't change to HOME directory either\n.");
							exit(1);
						}
					}
					clear_input(); 
					continue;
				}
			/* "Every time"-code portion concludes here *****************************************************/
			
			/* B-block which generates background processes begins here *************************************/
   	  /************************************************************************************************/
			if (options_exist && (0 == strcmp(last,"&"))) 
			{
				regular_prompt = 0;
				bg_pid = fork(); 
				if (0==bg_pid)
				{		
					signal(SIGINT,SIG_IGN);
					int i=0;																	/* Counter */

					while (argument_vector[i]!=NULL)					/* "&" gets cleared away, execvp() doesn' t */
					{																					/* execute it, it is just an indicator of   */
						if (0==strcmp(argument_vector[i],"&"))	/* of process type on the command line      */
						{
						  argument_vector[i] = NULL;
						  break;
						}					
						i++;
					}
					if (-1 == execvp(command, argument_vector))
					{ 
						perror("Can't exec background process");
					  exit(1);
					}
				}	
				else if (-1==bg_pid)
				{ 
					perror("Can't fork");
				  exit(1);
			  }
				else if (bg_pid>0)
				{		
					printf("%s %d %s", "Spawned background process",bg_pid,"\n");
					clear_input();
					number_of_pending_background_processes++;
				}
				/* No wait() on these processes because they are background processes, A-block minds that */
			}
			else 
			/* C-block which manages foreground processes begins here ***************************************/
   	  /************************************************************************************************/
			{
				signal(SIGINT,SIG_IGN);	/* <Ctrl-C> gets totally disabled so it can't interrupt process */
				regular_prompt = 1; 
				if (-1==gettimeofday(&before,NULL))			/* First point in time gets determined          */
				{
					perror("Can't timestamp\n"); exit(0);
				}
				pid = fork(); 
				if (-1==pid)
				{
					perror("Can't fork");
					exit(1);
				}				
				if (0==pid)
				{				
					printf("%s %d %s","Started foreground process",getpid(),"\n");
					if (-1==execvp(command,argument_vector))
					{
						perror("Can't exec");
						exit(1);
					}					
				}
				else
				{
		      strcpy(command,"");
					if (-1 == waitpid(pid, &status,0))
					{
						perror("Can't perform wait\n");
						exit(1);
					}
					if (WIFEXITED(status))
						printf("%s %d %s","Terminated foreground process",pid,"\n");
					else
						printf("%s %d %s","Foreground process ",pid," did not terminate normally\n");

					if (-1==gettimeofday(&after,NULL))			/* Second point in time gets determined, minimal */
					{																				/* wait()-/post-wait() times is included         */
						perror("Can't timestamp\n"); exit(0);
					}
					time_before_ms = (float)before.tv_usec; /* Execution time gets calculated */
					time_before = before.tv_sec;
					time_after_ms = (float)after.tv_usec;
					time_after = after.tv_sec;
					time_difference = 1000*(time_after-time_before)+0.001*fabs(time_after_ms-time_before_ms);		
					printf("%s %f %s\n", "Elapsed command execution wallclock time:",time_difference,"ms");
					clear_input();
					link_to_handler(SIGINT, first_signal_handler); /* <Ctrl-C> gets back to producing a		*/
																												 /* carriage return as in regular shell */
					
				}
			}
	  }	 
	}
	while (1==not_finished);
	exit(0);
}
