/*
*
* NAME:
*   digenv is a program which daisy-chains several predetermined shell commands and pipes
*   and pipes their filtered input/output from beginning to end.
*
* SYNTAX:
*   $digenv.o [arguments] with filter strings being the arguments
*
* DESCRIPTION:
*   The chain of commands, with pipes, that the program replicates is either $printenv|sort|pager
*   (with no arguments) or $printenv|grep [parameters]|sort|pager (with arguments).
*   The separation between these two alternatives in the code is made by a condition 
*   on the number of arguments to the program on the command line; argc>1 produces the latter, 
*   all other numbers of arguments produce the former (the program name itself is always input,
*   so the only other possible argument number is exactly one).
*   The pager is chosen from the list 1) environment variable list pager, or 2) less, or 3) more, in that order.
*
* EXAMPLES:
*   $./digenv.o UB	(will usually produce one line of output, UBUNTU_MENUPROXY=libappmenu.so)
*   $./digenv.o KRR	(will produce empty output, no such string will filter out)
*   $./digenv.o		(will produce an alphabetically sorted list of all environment variables
*                    as per current installation)
*   $./digenv.o L U	(will produce an error message; ./digenv.o: U: No such file or directory
*                    because shell command grep takes either only one or zero arguments,
*                    see NOTES below)
*
* ENVIRONMENT: 
*   The program, for its proper function, depends on shell commands being placed in some 
*   available directory and the path to be so set that this directory is available. This is the normal
*   configuration for a UNIX/Linux installation which should make the program portable.
*
* NOTES:
*   The program's output deviates minimally from the output of the command/pipe series
*   that it replicates in that the error message, in the case of a faulty argument set,
*   begins with ./digenv.o as the source of the fault, instead of with grep as in the
*   actual command series result from the actual CLI. Faults emanating from other services
*   than grep, if provoked (difficult), will be similarly represented. This should be
*   immaterial to the user. Depending on stochastical premises, output may also go either
*   through the pager or end up directly in the CLI. Because the output is the same output
*   and the user would have had to exit the pager anyway, this too should be immaterial.
*
*/

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{

	pid_t pid0,pid1,pid2,pid3;	/* Child process ID's */ 
	int fd[3][2];				/* Vector of integer vectors to be used as pipe*/
	int status;				/* Input to wait()-function*/
	char *pager;				/* String to store a possible environment list pager */

	if (argc>1)					/* Separation condition; on true, grep is used, otherwise not */
	{
	if (-1==pipe(fd[0]))		/* Vectors are given pipe format */
	{
		exit(1);
	}
	if (-1==pipe(fd[1]))
	{
		exit(1);
	}
	if (-1==pipe(fd[2]))
	{
		exit(1);
	}
	pid0 = fork();				/* First child process, will shift to printenv */
	if (-1==pid0)
	{
		exit(1);
	}
	if (0==pid0)
	{
		if (-1==dup2(fd[0][1],STDOUT_FILENO)) /* Redirection */
		{
			exit(1);
		}
/*
 *		Format below recurs once everywhere in each process, rigorously closes idle pipe ends
 */

		if ((-1==close(fd[0][0])) || (-1==close(fd[0][1])) || (-1==close(fd[1][0])) || (-1==close(fd[1][1]))
		   || (close(fd[2][0])) || (close(fd[2][1])))
		{
			exit(1);
		}
		if (-1==execlp("printenv","printenv",(char *)0))
		{
			exit(1);
		}
		
	}
	pid1 = fork();		/* Second child process, will shift to grep */
	if (-1==pid1)
	{
		exit(1);
	}
	if (0==pid1)
	{
		if ((-1==dup2(fd[0][0],STDIN_FILENO)) || (-1==dup2(fd[1][1],STDOUT_FILENO))) /* Redirection */
		{
			exit(1);
		}
		if ((-1==close(fd[0][0])) || (-1==close(fd[0][1])) || (-1==close(fd[1][0])) || (-1==close(fd[1][1]))
		   || (close(fd[2][0])) || (close(fd[2][1])))
		{
			exit(1);
		}
		if (execvp("grep",argv)<0)
		{
			exit(1);
		}
		
	}
	pid2 = fork(); 	/* Third child process, will shift to sort */
	if (-1==pid2)
	{
		exit(1);
	}
	if ((0==pid2))
	{
		if ((-1==dup2(fd[1][0],STDIN_FILENO)) || (-1==dup2(fd[2][1],STDOUT_FILENO))) /* Redirection */
		{
			exit(1);
		}		
		if ((-1==close(fd[0][0])) || (-1==close(fd[0][1])) || (-1==close(fd[1][0])) || (-1==close(fd[1][1]))
		   || (close(fd[2][0])) || (close(fd[2][1])))
		{
			exit(1);
		}
		if (execlp("sort","sort",(char *)0)<0)
		{
			exit(1);
		}
	}
	pid3 = fork();
	if (-1==pid3)		/* Fourth child process, will shift to pager */
	{
		exit(1);
	}
	if (0==pid3)
	{
		if (-1==dup2(fd[2][0],STDIN_FILENO))
		{
			exit(1);
		}
		if (-1==close(fd[0][0]) || (-1==close(fd[0][1])) || (-1==close(fd[1][0])) || (-1==close(fd[1][1]))
		|| (-1==close(fd[2][0])) || (-1==close(fd[2][1])))
		{
			exit(1);
		}
		pager = getenv("PAGER");
/*
 * Format of if-conditions below will cover for BOTH the case that the       
 * pager is found in the environment list but does not work, AND for the case that 
 * it is not found at all.
 */
		if (NULL != pager)
		{
			if (execlp(pager,pager,(char *)0)<0)
			{
				if (execlp("less","less",(char *)0)<0)
				{
					if (execlp("more","more",(char *)0)<0)
					{
						exit(1);
					}
				}
			}			
		}
		else if (execlp("less","less",(char *)0)<0)
		{
			if (execlp("more","more",(char *)0)<0)
			{
				exit(1);
			}
			
		}
	}
	if ((-1==close(fd[0][0])) || (-1==close(fd[0][1])) || (-1==close(fd[1][0])) || (-1==close(fd[1][1])) 
	  || (-1==close(fd[2][0])) || (-1==close(fd[2][1])))
	{
		exit(1);
	}
	
	if (waitpid(pid0,&status,0)<0)	/* All four processes are waited on */
		exit(1);
	if (waitpid(pid1,&status,0)<0)
		exit(1);
	if (waitpid(pid2,&status,0)<0)
		exit(1);
	if (waitpid(pid3,&status,0)<0)
		exit(1);
	exit(0);
	}
	else	/* Beginning of other leg as per separation condition, no arguments, no grep, mutatis mutandis */
	{
		if (-1==pipe(fd[0]))
		{
			exit(1);
		}
		if (-1==pipe(fd[1]))
		{
			exit(1);
		}
		if (-1==pipe(fd[2]))
		{
			exit(1);
		}
		pid0 = fork();
		if (-1==pid0)
		{
			exit(1);
		}
		if (0==pid0)
		{
		if (-1==dup2(fd[0][1],STDOUT_FILENO))
		{
			exit(1);
		}
			if ((-1==close(fd[0][0])) || (-1==close(fd[0][1])) || (-1==close(fd[1][0])) || (-1==close(fd[1][1]))
		   || (close(fd[2][0])) || (close(fd[2][1])))
			{
				exit(1);
			}
			if (-1==execlp("printenv","printenv",(char *)0))
			{
				exit(1);
			}
		
		}
		pid1 = fork();
		if (-1==pid1)
		{
			exit(1);
		}
		if ((0==pid1))
		{
			if ((-1==dup2(fd[0][0],STDIN_FILENO)) || (-1==dup2(fd[1][1],STDOUT_FILENO)))
			{
				exit(1);
			}		
			if ((-1==close(fd[0][0])) || (-1==close(fd[0][1])) || (-1==close(fd[1][0])) || (-1==close(fd[1][1]))
		   || (close(fd[2][0])) || (close(fd[2][1])))
			{
				exit(1);
			}
			if (execlp("sort","sort",(char *)0)<0)
			{
			exit(1);
			}
		}
	pid2 = fork();
	if (-1==pid2)
	{
		exit(1);
	}
	if (0==pid2)
	{
		if (-1==dup2(fd[1][0],STDIN_FILENO))
		{
			exit(1);
		}
		if (-1==close(fd[0][0]) || (-1==close(fd[0][1])) || (-1==close(fd[1][0])) || (-1==close(fd[1][1]))
		|| (-1==close(fd[2][0])) || (-1==close(fd[2][1])))
		{
			exit(1);
		}
		pager = getenv("PAGER");
		if (NULL != pager)
		{
			if (execlp(pager,pager,(char *)0)<0)
			{
				if (execlp("less","less",(char *)0)<0)
				{
					if (execlp("more","more",(char *)0)<0)
					{
						exit(1);
					}
				}
			}			
		}
		else if (execlp("less","less",(char *)0)<0)
		{
			if (execlp("more","more",(char *)0)<0)
			{
				exit(1);
			}
			
		}
	}
	if ((-1==close(fd[0][0])) || (-1==close(fd[0][1])) || (-1==close(fd[1][0])) || (-1==close(fd[1][1])) 
	  || (-1==close(fd[2][0])) || (-1==close(fd[2][1])))
	{
		exit(1);
	}
	if (waitpid(pid0,&status,0)<0)
		exit(1);
	if (waitpid(pid1,&status,0)<0)
		exit(1);
	if (waitpid(pid2,&status,0)<0)
		exit(1);	
	exit(0);
	}
}


