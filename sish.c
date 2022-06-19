/*
    Zachary Williams
    11/14/2021
    CS 3377.0W1
    Simple Shell Project
*/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Global nums
#define MAX_HISTORY_SIZE 100
#define TOKEN_BUFFSIZE 128

// Func prototypes
char* readInput();
char** tokenizeInput(char* line);
int cd(char** args);
int history(char** args);
int exitMyShell(char** args);
void initMyHistory();
int addToHistory(char* line);
int execute(char* line, int numPipes, int isHistory);
int pipeHelp(int in, int out, char** args);

// this struct is used to store data needed for recording user history
struct myHistory
{
    char* historyList[MAX_HISTORY_SIZE];
    int currIndex;
    int size;
};
// declaring struct and struct pointer
struct myHistory oldHistory;
struct myHistory* myNewHistory;

// list of commands and func pointers below
char* myCommands[] =
    {
        "cd",
        "exit",
        "history"};
int (*myCommandsPointers[])(char**) =
    {
        &cd,
        &exitMyShell,
        &history};

int main(int argc, char* argv[])
{
    char* line;
    int index;
    int status = 1;
    int numPipes; // used to record num of pipe operators

    initMyHistory();

    do
    {
        numPipes = 0;
        printf("sish> ");
        line = readInput();

        for (index = 0; line[index] != '\0'; index++)
        {
            if (line[index] == '|')
            {
                numPipes++;
            }
        }

        status = execute(line, numPipes, 0);

    } while (status == 1);

    // free memory
    free(line);
    return 0;
}

void initMyHistory()
{
    myNewHistory = &oldHistory;
    int index1;
    // Initializing the history struct

    myNewHistory->currIndex = 0;
    myNewHistory->size = MAX_HISTORY_SIZE;

    // Time to fill history with null
    for (index1 = 0; index1 < myNewHistory->size; index1++)
    {
        myNewHistory->historyList[index1] = NULL; 
    }
}

// The below func execs all except for the very last command, which is handled at the bottom
// of the execute function
int pipeHelp(int in, int out, char** args)
{
    pid_t pid;
    pid = fork();

    if (pid == -1)
    {
        fprintf(stderr, "Forking failed\n");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        if (in != 0)
        {
            if (dup2(in, 0) < 0)
            {
                fprintf(stderr, "dup2 failed\n");
                exit(EXIT_FAILURE);
            }
            close(in);
        }

        if (out != 1)
        {

            if(dup2(out, 1) < 0)
            {
                fprintf(stderr, "dup2 failed\n");
                exit(EXIT_FAILURE);
            }
            close(out);
        }

        return execvp(args[0], args);
    }
    if(waitpid(pid, NULL, 0) < 0)
    {
        
            fprintf(stderr, "waitpid error\n");
            exit(EXIT_FAILURE);
        
    }

    return pid;
}

// This function handles both piped and unpiped commands, calls pipeHelp when necessary
// line is the input, numPipes is the number of pipe operators
// isHistory is used to prevent the function from adding multiple entries to
// the history object whenever a previous command is called using the history [offset] command
int execute(char* line, int numPipes, int isHistory)
{
    int index;
    int index2;
    pid_t pid;
    int in, fd[2];
    char* token = (char*)malloc(TOKEN_BUFFSIZE); 
    char** args;
    int status = 0;

    if (line == NULL)
    {
        return 1;
    }
    if (isHistory == 0)
    {
        addToHistory(line);
    }
    char* temp = (char*)malloc(TOKEN_BUFFSIZE);
    strcpy(temp, line);
    in = 0;

    token = strtok_r(temp, "|", &temp);

    for (index = 0; index < numPipes; index++)
    {
        args = tokenizeInput(token);

        pipe(fd);
        pipeHelp(in, fd[1], args);

        token = strtok_r(NULL, "|", &temp);

        close(fd[1]);
        in = fd[0];
    }

    if (token != NULL)
    {

        args = tokenizeInput(token);
    }
    else
    {
        args = tokenizeInput(line);
    }

    if (isHistory == 0)
    {
        for (index2 = 0; index2 <= 2; index2++) //change 2 later to global num
        {
            // A return of 0 for strcmp means that the strings match
            if (strcmp(args[0], myCommands[index2]) == 0)
            {

                return (*myCommandsPointers[index2])(args);
            }
        }
    }

    if ((pid = fork()) == 0)
    {
        if (in != -1)
        {
            dup2(in, STDIN_FILENO);
            close(in);
        }

        execvp(args[0], args);
        fprintf(stderr, "execvp failed\n");
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        fprintf(stderr, "forking error\n");
        exit(EXIT_FAILURE);
    }
    if (waitpid(pid, &status, 0) < 0)
    {
        fprintf(stderr, "waitpid error\n");
        exit(EXIT_FAILURE);
    }

    return 1;
}

// readInput reads input from stdin, returns a char*
char* readInput()
{
    // this func originally used getc() instead of getline()
    // it was changed to getline() because it allows the func to be much shorter
    char* line = NULL;   // Not 100% necessary to instantiate to NULL
    size_t buffSize = 0; // Will be changed as needed for getline
    int index = 0;
    getline(&line, &buffSize, stdin);

    if (line == NULL)
    {
        fprintf(stderr, "Error getting input\n");
        exit(EXIT_FAILURE);
    }

    // checking for valid input
    while (line[index] == ' ' || line[index] == '|' || line[index] == '\n')
    {
        if (line[index + 1] == '\n')
        {
            fprintf(stderr, "Bad input, please try again\nsish> ");
            strcpy(line, ""); // setting line back to empty
            index = -1;
            getline(&line, &buffSize, stdin);
        }

        index++;
    }
    return line;
}

char** tokenizeInput(char* line)
{
    int buffSize = TOKEN_BUFFSIZE; // change to a #define number later
    int index = 0;           // used to parse through tokens
    char* token = NULL;
    char* currState = NULL;
    char* temp = (char*)malloc(TOKEN_BUFFSIZE); // using a temp var is necessary since strtok destroys what it operates on
    strcpy(temp, line);

    char** tokens = malloc(buffSize * sizeof(char*));
    if (tokens == NULL)
    {
        fprintf(stderr, "MEM allocation error in tokenizeInput\n");
        exit(EXIT_FAILURE);
    }

    for (token = strtok_r(temp, " \n\t", &currState); token != NULL; token = strtok_r(NULL, " \n\t", &currState)) // INSERT DELIMS in the middle LATER
    {
        tokens[index] = token;
        index++;
        if (index >= buffSize)
        {
            buffSize = buffSize + TOKEN_BUFFSIZE;               // insert define num later
            tokens = realloc(tokens, buffSize * sizeof(char*)); // reallocing additional mem if it goes over the limit
            if (tokens == NULL)
            {
                fprintf(stderr, "MEM allocation error in tokenizeInput\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    tokens[index] = NULL;
    return tokens;
}

int addToHistory(char* line)
{
    char* tok;
    tok = strtok_r(line, "\n", &line);

    if (myNewHistory->currIndex == MAX_HISTORY_SIZE - 1)
    {
        myNewHistory->currIndex = 0; // reset the current index to 0 if we reach the max possible index
    }

    //add to the global struct
    myNewHistory->historyList[myNewHistory->currIndex] = tok;
    strcpy(myNewHistory->historyList[myNewHistory->currIndex], tok);

    myNewHistory->currIndex++;

    return 1;
}

int history(char** args)
{
    int index;
    int numPipes = 0;
    char* line = NULL;
    char clearFlag[3] = "-c";
    char clearFlag2[3] = "-C";

    // print history if no arguments are passed
    if (args[1] == NULL)
    {
        for (index = 0; index < myNewHistory->currIndex; index++)
        {
            printf("%d %s ", index, myNewHistory->historyList[index]);
            printf("\n");
        }
    }
    else if (strcmp(args[1], clearFlag) == 0 || strcmp(args[1], clearFlag2) == 0) // works if either -c or -C is passed
    {
        for (index = 0; index < myNewHistory->size; index++)
        {
            myNewHistory->historyList[index] = NULL; // Clearing everything
        }
        myNewHistory->currIndex = 0; // Resetting the current index
    }
    else
    {
        index = atoi(args[1]); // using atoi to parse from char* to int
        if (index > myNewHistory->currIndex)
        {
            fprintf(stderr, "Invalid index, please try again\n");
            return 1;
        }
        line = myNewHistory->historyList[index];
        // calculate the number of pipes
        for (index = 0; line[index] != '\0'; index++)
        {
            if (line[index] == '|')
            {
                numPipes++;
            }
        }
        execute(line, numPipes, 1); 
    }

    return 1;
}
// built in func. simply changes directory
int cd(char** args)
{

    if (args[1] == NULL)
    {

        fprintf(stderr, "Invalid number of arguments for %s\n", args[0]);
    }
    else if (chdir(args[1]) != 0)
    {
        fprintf(stderr, "Could not execute command %s\n", args[0]);
    }
    return 1;
}
// built in func. simply exits the shell
int exitMyShell(char** args)
{
    exit(EXIT_SUCCESS);
    return 0;
}
