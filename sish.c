#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

//global constants used for the the maximum amount of arguments and history size
#define MAX_ARGS 22 //maximum of 20 arguemnts can be passed for a command 
#define MAX_ARCHIVE 101 //maxiumum of 100 entries can be in logged in the history at a time

//function prototypes 
void sishLoop();
int tokenLine(char**,char*[MAX_ARGS]);
void execute(char*[MAX_ARGS]);
void errorHelper(char*); 
void cd(char*);
void history(char*);
int isEmpty(char*);
void archiveLine(char*);
void executePipe(char*, char*[MAX_ARGS]);

//global variables used to represent the history of commands inputted into the shell
char **archive;
int archive_size = 0;

//main function that clears the screen and calls the shell's loop 
int main(int argc, char **argv)
{
  system("clear");
  sishLoop();

    return EXIT_SUCCESS;
}


//the main while loop that represents the shell 
void sishLoop()
{
	//local variables 
	char *line = NULL;
        size_t length = 0;
	char *args[MAX_ARGS];
	int flag = 1;

	//initializing the archive 
	archive = malloc(MAX_ARCHIVE*sizeof(char*));
	
	do {
		printf("sish> "); //shell prompt

		//gets the users input
		getline(&line,&length,stdin);
		line[strlen(line)-1]='\0';

		//logs the user's input into the history
		archiveLine(line);
		if(archive_size != MAX_ARCHIVE-1)
			archive_size++;

		//if command is exit, program will end
		if(strcmp(line, "exit") == 0)
			return;
		
		//if command is empty, the shell will ignore the line and contontinue to the next iteration of the loop
		if(isEmpty(line)==0)
			continue;

		//checks on which execute command to run based off if there is a pipe present or not
		if(strchr(line, '|') == NULL)
		{
			tokenLine(&line,args); //breaks the line into tokens and stores them into an args array
				execute(args); //executes the command with the given arguments
		}
		else
			executePipe(line,args); //executes the pipe command 
	} while (flag);
}

//used to log a command into the history 
void archiveLine(char *line)
{
	int size = archive_size;

		/*if the size of the archive is already at its max size, 
		delete the oldest entry and shift the archive to make room for the newest one*/
		if(size == MAX_ARCHIVE-1)
		{	
			int i;
			for(i = 0; i < MAX_ARCHIVE - 1; i++)
			{
				archive[i] = archive[i+1];
			}
			size--;
		}

		//allocates space for the entry then logs the line into the archive
		archive[size] =malloc((strlen(line)+1) * sizeof(char));
		strcpy(archive[size],line);
		return;
}

//tokens a line into an array that can be used by execvp and returns the amount of arguments in the command
int tokenLine(char** line, char* args[MAX_ARGS])
{
	char* token;
	char *ptr;
	int count = 1;

	//makes the first element in the array the command 
	args[0] = strtok_r(*line," ",&ptr);

	//tokenizes the line based on white space to get all of the arguments and stores them in the array
	token = strtok_r(NULL," ",&ptr);
	while(token != NULL)
	{
		args[count++] = token;
		token = strtok_r(NULL," ",&ptr);
	}

	//sets the final element in the array to NULL for execvp to use 
	args[count] = NULL;

	return count;
}

//executes the command if no pipe is present
void execute(char*args[MAX_ARGS])
{
	//checks the first element in args to see if the command is a built in shell command
	if(strcmp(args[0], "cd") == 0)
		cd(args[1]); 
	else if(strcmp(args[0], "history") == 0)
		history(args[1]);

	/*if the command isnt a built in command, the shell uses fork and execvp to create a child 
	process that runs the system command*/
	else
	{
		int pid = fork();
		if(pid < 0)
			errorHelper("Error creating child");

		if(pid ==0)
		{
			execvp(args[0], args);
			errorHelper("Invalid command");
		}
		else
		{
			wait(NULL);
			return;	
		}
	}
	return;
}

//closes all the pipes that may have been opened while executing a pipe command
void closePipes(int fd[][2], int n)
{
	for(int i=0;i<n;i++)
	{
		for(int j =0; j < 2; j++)
			close(fd[i][j]);		
	}
}

//executes a pipe command using pipe and execvp 
void executePipe(char* line, char* args[MAX_ARGS])
{
	char* command;
	char *ptr;
	int pipe_count = 0;
	int status;
	int n = 0;

	//counts the amount of pipes present within the command
	for (int i = 0; line[i] != '\0'; ++i) 
	{
		if ('|' == line[i])
			++pipe_count;
	}

	//creates a 2d array to hold the file descriptors of the pipes based off the amount of pipes created
	int fd[pipe_count][2];

	//creates the necessary amount of pipes 
	for(int i=0;i<pipe_count;i++)
	{
	   if(pipe(fd[i]) == -1)
	   	perror("Pipe creation failed");
	}
	
	//tokenizes the line to execute each sub-command one at a time 
	while ((command = strtok_r(line,"|",&ptr)))
	{
		line = NULL; 
		tokenLine(&command,args); // tokenizes the sub-command to read its arguments 

		//creates a child process to execute sub-command
		int pid = fork();
		if(pid < 0)
			errorHelper("Error creating child");

		//child 
		else if(pid == 0)
		{
			//if sub-command isnt the first sub-command in the line, changes STDIN to previous command's STDOUT
			if(n != 0) 
				if(dup2(fd[n-1][0],STDIN_FILENO) < 0)
					errorHelper("Error creating pipe");

			//if sub-command isnt the last sub-command in the , line, changes STDOUT to next command's STDIN
			if(n != pipe_count )
				if(dup2(fd[n][1],STDOUT_FILENO) < 0)
					errorHelper("Error creating pipe");
			
			//closes all the pipes and execcutes the command
			closePipes(fd,pipe_count);
			execvp(args[0], args);
			errorHelper("Invalid command");
		}
		n++;
	}

	//parent closes all the pipes and waits for children to finish executing
	closePipes(fd,pipe_count);
	for (int i = 0; i <= pipe_count; i++)
	      wait(&status);
	return;	
}

//checks if the line is comprised of empty characters or white space and returns 0 if so
int isEmpty(char* line)
{
	if(strcmp(line, "") == 0)
		return 0;
	while (*line != '\0') 
	{
		if (!isspace((unsigned char)*line))
			return 1;
		line++;
	}
	return 0;
}

//change directory built in command that changes the current working directory of the shell
void cd(char* dir)
{
	if(chdir(dir) < 0)
		perror("Invalid directory");	
	return;
}

/*history built in command that shows the history of the last 100 
commands given to the shell and can be used to re-execute them*/
void history(char* arg)
{
	int *size = &archive_size;

	//if the argument is -c, clears the histroy and sets the archive size back to 0
	if(arg != NULL && strcmp(arg,"-c") == 0) 
	{
		for(int i = 0; i < *size; i++)
		{
			archive[i] = NULL;
		}
		*size = 0;
		return;
	}

	//if there is no argument, displays the last 100 commands inputted by the shell 
	if(arg == NULL)
	{
		for(int i =0; i < *size; i++)
		{
			printf("%d %s\n", i, archive[i]);
		}
	}
	else
	{
		//checks to see if the argument is a valid number, if so executes the command at that history number
		int n  = atoi(arg);
		if((strcmp(arg,"0") != 0 && n==0) ||  n >= *size-1)
		{
			printf("\033[0;31m");
			printf("Invalid argument\n");
			printf("\033[0m");
		}

		else
		{
			char* line;
			char* args[MAX_ARGS];

			//copies the element from the given argument number into line
			line =malloc((strlen(archive[n])+1) * sizeof(char));
			strcpy(line,archive[n]);

			//checks on which execute command to run based off if there is a pipe present or not
			if(strchr(line, '|') == NULL)
			{
				tokenLine(&line,args);
				execute(args);
			}
			else
				executePipe(line,args);
		}
			
	}
	return;
}

//used to exit child processes if an error occurs during its run time.
void errorHelper(char* msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}
