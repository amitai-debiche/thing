#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define INITIAL_ALLOC_SIZE 10
#define INCREMENT_SIZE_MULTIPLE 2
#define NO_PIPE -1

void do_piping(int pipe_in, int pipe_out) {
    if (pipe_in != NO_PIPE) {
        if (dup2(pipe_in, 0) < 0) {
            exit(1);
        }
        close(pipe_in);
    }

    if (pipe_out != NO_PIPE) {
        if (dup2(pipe_out, 1) < 0) {
            exit(1);
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
        exit(1);
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
        
        //ASK LOUIS OFFICE HOURS, DO I WANT TO ALSO CHECK HERE FOR FEOF?
        //AND RETURN SOME ERROR NOTICE IF -1 BUT NOT EOF
        if (input_read == -1){
            printf("\n");
            free(string);
            exit(0);
        }
        string[input_read - 1] = '\0';

        char *temp = string;

        token =  strsep(&temp, "|");
       
        while(token != NULL) {
            if (num_pipes >= max_pipes){
                max_pipes *= INCREMENT_SIZE_MULTIPLE;
                commands = realloc(commands, max_pipes * sizeof(char*));
                if (!commands) {
                    perror("Mem allocation failed");
                    exit(1);
                }
            }
            commands[num_pipes++] = token;
            token = strsep(&temp, "|");
        }

        int pipefds[num_pipes -1][2];
        for (int i = 0; i < num_pipes - 1 ; i++) {
            if (pipe(pipefds[i]) == -1) {
                exit(1);
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
                        perror("Mem allocation failed");
                        exit(1);
                    }
                }
                args[num_args++] = command;
                command = strsep(&temp_commands[i], " ");
            }

            args[num_args] = NULL;
//            for (int i = 0; i < num_args; i++){
 //               printf("%sx\n", args[i]);
  //          }
  //
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
                        close(pipefds[i-1][1]);
                        do_piping(pipefds[i-1][0], NO_PIPE);
                    }
                }
                execvp(args[0], args);
            }else {
                //need this otherwise parent still has pipfds[1] open if even first command exits
                if (i > 0){
                    close(pipefds[i-1][0]);
                    close(pipefds[i-1][1]);
                }
            }
            int status;
            waitpid(pid, &status, 0);
        }

        for (int i = 0; i < num_pipes - 1; i++) {
            close(pipefds[i][0]);
            close(pipefds[i][1]);
        }
        //for (int i = 0; i < num_pipes; i++) {
         //   wait(NULL);
        //}
/*
            int pid = fork();
            if (pid < 0){
                perror("fork failed");
                exit(1);
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
                exit(1);
            }
        }

*/
        free(string);
        free(commands);
    }
}




