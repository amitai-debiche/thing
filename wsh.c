#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>



int main(int argc, char *argv[])
{
    while(1)
    {
        char *string = NULL;
        size_t size = 0;
        ssize_t input_read;

        char *command;
        printf("wsh> ");
        input_read = getline(&string, &size, stdin);
        printf("getline returned %ld\n", input_read);
        if (input_read < 0){
            printf("couldn't read the input");
            free(string);
            exit(1);
        }
        string[input_read - 1] = '\0';

        char *args[input_read];
        command = strsep(&string, " ");
        int i = 0;
        
        while(command != NULL) {
            args[i++] = command;
            printf("Token:%s\n", command);
            command = strsep(&string, " ");
        }

        args[i] = NULL;

        if (execvp(args[0], args) == -1) {
            printf("execvp");
            return 1;
        }

        free(string);
    }
}





