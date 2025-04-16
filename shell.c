#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#define MAX_TOKEN_LENGTH 32
#define MAX_TOKEN_COUNT 100
#define MAX_LINE_LENGTH 512

/*
 * This function parses the next subcommand seperated by |, <, > etc
 * Place the content in args where args[0] = command name
 * args[1] [2] etc will be the arguments
 *
 * Return the leading character after this command,
 * which could be |, <, >, or a regular character
 * For example,  for "ls > tmp",  return the position of ">"
 * For example,  for "ls | wc ",  return the position of "|"
 */
char* parse(char * lineptr, char **args)
{
  //while lineIn isn't done. 
  while (*lineptr != '\0') 
  {
    // If it's whitespace, tab or newline,
    // turn it into \0 and keep moving till we find the next token.
    // This makes sure each arg has a \0 immidiately after it
    // without needing to copy parts of lineIn to new strings. 
    while (!(isdigit(*lineptr) || isalpha(*lineptr) || *lineptr == '-'
        || *lineptr == '.' || *lineptr == '\"'|| *lineptr == '/'
        || *lineptr == '_'))
    {   
      //break out if we reach an "end"
      if(*lineptr == '\0' || *lineptr == '<' || *lineptr == '>'
          || *lineptr == '|' || *lineptr == '&')
        break;

      *lineptr = '\0';
      lineptr++;
    }

    //break out of the 2nd while loop if we reach an "end".
    if(*lineptr == '\0' || *lineptr == '<' || *lineptr == '>'
        || *lineptr == '|' || *lineptr == '&' )
      break;

    //mark we've found a new arg
    *args = lineptr;    
    args++;

    // keep moving till the argument ends. Once we reach a termination symbol,
    // the arg has ended, so go back to the start of the loop.
    while (isdigit(*lineptr) || isalpha(*lineptr) || *lineptr == '-'
        || *lineptr == '.' || *lineptr == '\"'|| *lineptr == '/'
        || *lineptr == '_')
      lineptr++;
  }

  *args = NULL; 

  return lineptr;
}


/*
 * This function forks a child to execute  a command stored
 * in a string array *args[].
 * inPipe -- the input stream file descriptor.  For example,  wc < temp 
 * outPipe --the output stream file descriptor.  For example, ls > temp 
 */
void fchild(char **args,int inPipe, int outPipe)
{
  printf("Fork: '%s' In: %d Out: %d\n", args[0], inPipe, outPipe);
  pid_t pid;
  pid = fork();
  
  if (pid == 0)/*Child  process*/
  {
    int execReturn=-1;

    /*Call dup2 to setup redirection, and then call excevep*/

    /*Your solution*/
    if (inPipe != 0) {
      if (dup2(inPipe, 0) < 0) {
        fprintf(stderr, "ERROR: dup2 failed on inPipe: %d\n", inPipe);
        exit(1);
      }
    }
    if (outPipe != 1) {
      if (dup2(outPipe, 1) < 0) {
        fprintf(stderr, "ERROR: dup2 failed on outPipe: %d\n", outPipe);
        exit(1);
      }
    }
    execReturn = execvp(args[0], args);

    if (execReturn < 0) 
    { 
      perror(args[0]);
      exit(1);
    }
      
    _exit(0);

  }

  if (pid < 0)
  {
    printf("ERROR: Failed to fork child process.\n");
    exit(1); 
  }
  
  if(inPipe != 0)
    close(inPipe); /*clean up, release file control resource*/
    
  if(outPipe != 1)
    close(outPipe); /*clean up, release file control  resource*/
}


/*
 * This function parses and executes a command started from the string position
 * pointed by linePtr. This function is called recursively to execute a
 * subcommand one by one.
 *
 * linePtr --  points to the starting position of the next command in the input command string
 * length -- the length of the remaining command string
 * inPipe is the input file descriptor, initially it is 0, gradually it may be changed as we parse subcommmands.
 * out is the output file descriptor, initially it is 1, gradually it may be changed as we parse subcommmands.
 * The inPipe default value  is 0 and  outPipe default value is 1.
 */
void runcmd(char * linePtr, int length, int inPipe, int outPipe)
{
  char * args[length];
  char * nextChar = parse(linePtr, args);
  int remainingLength = length - (nextChar - linePtr);
  printf("Cmd: '%s' Curr: '%c' Next: '%c' Remaining: %d In: %d Out: %d\n", args[0], *linePtr, *nextChar, remainingLength, inPipe, outPipe);

  if (args[0] != NULL)
  {
    /*Exit if seeing "exit" command*/
    /*Your solution*/
    if (strcmp(args[0], "exit") == 0)
      exit(0);
            
    // Handle input redirection.
    if (*nextChar == '<' && inPipe == 0) {
      char *infileArgs[MAX_TOKEN_COUNT];
      // Skip the '<' operator.
      nextChar = parse(nextChar + 1, infileArgs);
      int newIn = open(infileArgs[0], O_RDONLY);
      if (newIn < 0) {
          perror(infileArgs[0]);
          return;
      }
      printf("Reading: '%s' Next: '%c' In: %d\n", infileArgs[0], *nextChar, newIn);
      fchild(args, newIn, outPipe);
      // Reset inPipe to standard input for subsequent commands.
      runcmd(nextChar, remainingLength, 0, outPipe);
      return;
  }
  
    // Handle output redirection.
    if (*nextChar == '>') {
      char *outfileArgs[MAX_TOKEN_COUNT];
      // Skip the '>' operator.
      nextChar = parse(nextChar + 1, outfileArgs);
      int newOut = open(outfileArgs[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (newOut < 0) {
          perror(outfileArgs[0]);
          return;
      }
      printf("Writing: '%s' Next: '%c' Out: %d\n", outfileArgs[0], *nextChar, newOut);
      fchild(args, inPipe, newOut);
      // Reset outPipe to standard output for subsequent commands.
      runcmd(nextChar, remainingLength, inPipe, 1);
      return;
    }
  
  // Handle pipe.
  if (*nextChar == '|') {
      int fd[2];
      if (pipe(fd) < 0) {
          perror("pipe");
          exit(1);
      }
      printf("Piping: Write: %d --> Read: %d\n", fd[1], fd[0]);
      // Execute the current command with its output going to fd[1].
      fchild(args, inPipe, fd[1]);
      // Process the remainder of the command using the read end of the pipe.
      runcmd(nextChar + 1, remainingLength, fd[0], outPipe);
      return;
  }

    if (*nextChar == '\0') 
    { /*There is noting special after this subcommand, so we just execute in a regular way*/
      fchild(args,inPipe,outPipe);
      return;
    }

    //else: some problem, so throw a fit.
    fprintf(stderr, "ERROR: Invalid input: %c\n", *nextChar);
  }
}


int main(int argc, char *argv[])
{
  /*Your solution*/
  char lineIn[MAX_LINE_LENGTH];

  while(1) 
  {
    if (fgets(lineIn,MAX_LINE_LENGTH,stdin) == NULL)
      break;
              
    int len = 0;
    while (lineIn[len] != '\0')
      len++;
      
    /* remove the \n that fgets adds to the end */
    if (len != 0 && lineIn[len-1] == '\n')
    {
      lineIn[len-1] = '\0';
      len--;
    }
        
    //Run this string of subcommands with 0 as default input stream
    //and 1 as default output stream
    printf("LINE: '%s'\n", lineIn);
    runcmd(lineIn, len,0,1);
  
    /*Wait for the child completes */
    /*Your solution*/
    int status = 0;
    while (wait(&status) > 0);
  }

  return 0;
}
