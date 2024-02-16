#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>



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
        char *command;

        printf("wsh> ");
        input_read = getline(&string, &size, stdin);
        printf("%ld", input_read);
        
        //ASK LOUIS OFFICE HOURS, DO I WANT TO ALSO CHECK HERE FOR FEOF?
        //AND RETURN SOME ERROR NOTICE IF -1 BUT NOT EOF
        if (input_read == -1){
            printf("\n");
            free(string);
            exit(0);
        }
        string[input_read - 1] = '\0';

        char *input_args[input_read];
        command = strsep(&string, " ");
        int i = 0;
       
        while(command != NULL) {
            input_args[i++] = command;
            command = strsep(&string, " ");
        }

        input_args[i] = NULL;
        int rc = fork();
        if (rc == 0) {
            execvp(input_args[0], input_args);
        }else {
            int status;
            waitpid(rc, &status, 0);
        }
        free(string);
    }
}





