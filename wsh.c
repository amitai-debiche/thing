#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define INITIAL_ALLOC_SIZE 10
#define INCREMENT_SIZE_MULTIPLE 2
#define NO_PIPE -1
#define EXIT_FAIL -1

void do_piping(int pipe_in, int pipe_out) {
    if (pipe_in != NO_PIPE) {
        if (dup2(pipe_in, 0) < 0) {
            exit(EXIT_FAIL);
        }
        close(pipe_in);
    }

    if (pipe_out != NO_PIPE) {
        if (dup2(pipe_out, 1) < 0) {
            exit(EXIT_FAIL);
        }
        close(pipe_out);
    }
}

int main(int argc, char *argv[])
{
    //TODO: IMPLEMENT NUMBER OF ARGS CHECKED, IF ARG == 2, HANDLE SCRIPT CASE
    if (argc == 2){
        //handle script
    }
    else if (argc > 2) {
        exit(EXIT_FAIL);
    }


    
    while(1)
    {
        char *string = NULL;
        size_t size = 0;
        ssize_t input_read;
        char *token;
        char **commands = malloc(INITIAL_ALLOC_SIZE * sizeof(char*));
        int num_pipes = 0;
        int max_pipes = INITIAL_ALLOC_SIZE;

        printf("wsh> ");
        input_read = getline(&string, &size, stdin);
        
        //eof input
        if (input_read == -1){
            printf("\n");
            free(string);
            exit(0);
        }
        string[input_read - 1] = '\0';

        char *temp = string;

        token = strsep(&temp, "|");
       
        while(token != NULL) {
            if (num_pipes >= max_pipes){
                max_pipes *= INCREMENT_SIZE_MULTIPLE;
                commands = realloc(commands, max_pipes * sizeof(char*));
                if (!commands) {
                    exit(EXIT_FAIL);
                }
            }
            commands[num_pipes++] = token;
            token = strsep(&temp, "|");
        }
        commands[num_pipes] = NULL;

        int pipefds[num_pipes - 1][2];
        for (int i = 0; i < num_pipes - 1 ; i++) {
            if (pipe(pipefds[i]) == -1) {
                exit(EXIT_FAIL);
            }
        }
        
 
        char **temp_commands = commands;
        for (int i = 0; i < num_pipes; i++) {
            char **args = malloc(INITIAL_ALLOC_SIZE * sizeof(char*));
            int max_args = INITIAL_ALLOC_SIZE;
            char *command = strsep(&temp_commands[i], " ");
            int num_args = 0;
            while (command != NULL){
                if (num_args >= max_args) {
                    max_args *= INCREMENT_SIZE_MULTIPLE;
                    args = realloc(args, max_args * sizeof(char*));
                    if (!args) {
                        exit(EXIT_FAIL);
                    }
                }
                if (strcmp(command, "") != 0) {
                    args[num_args++] = command;
                }

                command = strsep(&temp_commands[i], " ");
            }

            args[num_args] = NULL;
            int pid = fork();
            if (pid == 0){
                if (num_pipes - 1 > 0){
                    if(i == 0){
                        close(pipefds[i][0]);
                        do_piping(NO_PIPE, pipefds[i][1]);
                        close(pipefds[i][1]);
                    }else if (i > 0 && i < num_pipes - 1) {
                        close(pipefds[i-1][1]);
                        close(pipefds[i][0]);
                        do_piping(pipefds[i-1][0], pipefds[i][1]);
                    }else if (i == num_pipes - 1) {
                        do_piping(pipefds[i-1][0], NO_PIPE);
                        close(pipefds[i-1][1]);
                        close(pipefds[i-1][0]);
                    }
                }
                execvp(args[0], args);
            }else {
                //need to close previous pipe, if have a new one
                if (i > 0){
                    close(pipefds[i-1][0]);
                    close(pipefds[i-1][1]);
                }
            }
            free(args);
        }
        close(pipefds[num_pipes-1][0]);
        close(pipefds[num_pipes-1][1]);
        for (int i = 0; i < num_pipes; i++) {
           wait(NULL);
        }
/*
            int pid = fork();
            if (pid < 0){
                perror("fork failed");
                exit(EXIT_FAIL);
            }else if (pid == 0) { //child
                if (i > 0){
                    dup2(pipefds[i-1][0], 0);
                    close(pipefds[i-1][1]);
                }
                if (i < num_pipes - 1) {
                    dup2(pipefds[i][1], 1);
                }
                execvp(args[0], args);
            }
            close(pipefds[i][1]);
            close(pipefds[i][0]);
            free(args);
        }

        for (int i = 0; i < num_pipes - 1; i++) {
            close(pipefds[i][0]);
            close(pipefds[i][1]);
        }

        for (int i = 0; i < num_pipes; i++){
            int status;
            int waitpid_rc = waitpid(-1, &status, 0);
            if (waitpid_rc == -1){
                exit(EXIT_FAIL);
            }
        }

*/
        free(string);
        free(commands);
    }
}




