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


void exit_command(char **args);
void cd_command(char **args);
void export_command(char **args);
void local_command(char **args);
void vars_command(char **args);
//void history_command(char **args);


char *builtin_commands[] = {
    "exit",
    "cd",
    "export",
    "local",
    "vars",
    "history"
};

void (*builtin_functions[])(char **) = {
    &exit_command,
    &cd_command,
    &export_command,
    &local_command,
    &vars_command,
//    &history_command
};

int num_builtin_commands() {
    return sizeof(builtin_commands) / sizeof(char *);
}


//////////////////////////////////////////////////////////////////////////////////
///                         HASH TABLE STUFF(for local)                        ///
///      I followed this article which was a huge help:                        ///         
///      https://theleo.zone/posts/hashmap-in-c/                               /// 
/////////////////////////////////////////////////////////////////////////////////

typedef struct Entry {
    char *key;
    char *value;      //only storing string values here
    struct Entry *next;
} Entry;

typedef struct HashTable {
    Entry **buckets;
    int nBuckets;
} HashTable;

//UNSURE IF LINKED LIST IS NEEDED: TESTING
/*
typedef struct Node{
    void *data;
    struct Node *next;
} Node;

typedef struct list {
    int size;
    Node *head;
} List;
//GLOBALS
*/

//List *local_list;

/*

List *LinkedList_set(){
    List *new_list = malloc(sizeof(List));
    new_list->size = 0;
    new_list->head = NULL;
    return new_list;
}

void LinkedList_add(List *list, void *data){
    Node *new_node = (Node*)malloc(sizeof(Node));
    new_node->data = data;
    new_node->next = list->head;
    list->head = new_node;
    list->size++;
}

void* LinkedList_delete(List *list){
    if (list->size == 0){
        return NULL;
    }
    Node *node_to_remove = list->head;
}

void LinkedList_free(List *list){
    Node *current_node = list->head;
    while(current_node != NULL){
        Node *next_node = current_node->next;
        free(current_node);
        current_node = next_node;
    }
    free(list);
}
*/
//create global HashTable for local_vars
HashTable *local_vars;
//hash function, I honestly have no idea why this is such a good hash function but it is apparently very good
// djb2 by Daniel Bernstein: http://www.cse.yorku.ca/~oz/hash.html

unsigned long hash(char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
            hash = ((hash << 5) + hash) + c; 
    }
    
    return hash;
}

unsigned long get_bucket(HashTable *h, char *key) {
    return hash(key) % h->nBuckets;
}

HashTable *HashTable_new() {
    int nBuckets = 100;
    HashTable *h = malloc(sizeof(HashTable));
    if (h == NULL){
        exit(EXIT_FAIL);
    }
    h->nBuckets = nBuckets;
    h->buckets = calloc(nBuckets, sizeof(Entry *)); // ensure all items initially NULL'd out
    if (h->buckets == NULL){
        exit(EXIT_FAIL);
    }
    return h;
}

void HashTable_set(HashTable *h, char *key, void *val) {
    unsigned long bucket = get_bucket(h, key);
    Entry *v = h->buckets[bucket];
    while (v != NULL) {
        if (strcmp(v->key, key) == 0) {
            v->value = val;
            return;
        }
        v = v->next;
    }

    Entry *newVal = malloc(sizeof(Entry));
    if (newVal == NULL){
        exit(EXIT_FAIL);
    }

    newVal->key = strdup(key);
    if (newVal->key == NULL){
        exit(EXIT_FAIL);
    }
    newVal->value = val;
    newVal->next = h->buckets[bucket];
    h->buckets[bucket] = newVal;
    printf("'%s' : '%s'\n", newVal->key, newVal->value);
    printf("'%s' : '%s'\n", h->buckets[bucket]->key, h->buckets[bucket]->value);
}

void *HashTable_get(HashTable *h, char *key) {
    unsigned long bucket = get_bucket(h, key);
    Entry *v = h->buckets[bucket];

    while (v != NULL){
        if (strcmp(v->key, key) == 0) {
            return v->value;
        }
        v = v->next;
    }
    return NULL;
}

void *HashTable_delete(HashTable *h, char *key) {
    unsigned long bucket = get_bucket(h, key);
    Entry *prev = NULL;
    Entry *v = h->buckets[bucket];

    while (v!= NULL){
        if (strcmp(v->key, key) == 0) {
            if (prev == NULL) {
                //if head, set head next
                h->buckets[bucket] = v->next;
            } else {
                //in middle or end
                prev->next = v->next;
            }
            free(v->key);
            free(v);
            return 0;
        }
        prev = v;
        v = v->next;
    }
    //if no entry, return 0
    return 0;
}

void printHashTable(HashTable *h){
    Entry *v = h->buckets[0];
    while (v != NULL) {
        printf("%s=%s\n", v->key,v->value);
        v = v->next;
    }
}

void HashTable_free(HashTable *h) {
    for (int i = 0; i < h->nBuckets; i++) {
        Entry *v = h->buckets[b];
        while (v != NULL) {
            Entry *next = v->next;
            free(v->key);
            free(v);
            v = next;
        }
    }
    free(h->buckets);
    free(h);
}


//////////////////////////////////////////////////////////////////////////////////
///                         HASH TABLE END                                     ///
/////////////////////////////////////////////////////////////////////////////////


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
    //create file pointer and replace stdin
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
                    //handle shell var
                    if (command[0] == '$'){
                        command = getenv(++command);
                        if (command == NULL){
                            //add local here once local is working
                            command = "\0";
                        }
                    }
                    args[num_args++] = command;
                }

                command = strsep(&temp_commands[i], " ");
            }

            args[num_args] = NULL;
            int built_in_flag = 0;

            if (num_pipes - 1 == 0){
                for(int i = 0; i<num_builtin_commands(); i++){
                    if (strcmp(args[0], builtin_commands[i]) == 0){
                        (*builtin_functions[i])(args);
                        built_in_flag = 1;
                        break;
                    }
                }
            }

            if (built_in_flag) break;
            

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
        free(string);
        free(commands);
    }
}

void exit_command(char **args) {
    //don't have to check after 1, 2 couldn't possibly have value if 1 is NULL
    if (args[1] != NULL){
        exit(EXIT_FAIL);
    }
    exit(0);
}

void cd_command(char **args) {
    if (args[1] == NULL || args[2] != NULL){
        exit(EXIT_FAIL);
    }
    int cd_rc = chdir(args[1]);

    if (cd_rc == -1) {
        exit(EXIT_FAIL);
    }
}

void export_command(char **args){
    char *var_name;
    char *var_value;
    char *token;
    
    char *input_copy = strdup(args[1]);
    char *temp = input_copy;
    token = strsep(&temp, "=");
    var_name = token;
    var_value = temp;

    if(strcmp(var_value, "") == 0){
        int unset_rc = unsetenv(var_name);
        if (unset_rc == -1){
            free(input_copy);
            exit(EXIT_FAIL);
        }
    }else{
        int setenv_rc = setenv(var_name, var_value, 1);
        if (setenv_rc == -1){
            exit(EXIT_FAIL);
        }
    }
    free(input_copy);
}

void local_command(char **args){
    if (local_vars == NULL){
        local_vars = HashTable_new();
        if (local_vars == NULL){
            exit(EXIT_FAIL);
        }
    }
    char *local_name;
    char *local_value;
    char *token;
    char *input_copy = strdup(args[1]);
    char *temp = input_copy;
    token = strsep(&temp, "=");
    local_name = token;
    local_value = temp;

    if(strcmp(local_value, "") == 0){
        HashTable_delete(local_vars,local_name);
    }else {
        HashTable_set(local_vars, local_name, local_value);
    }
    free(input_copy);
}


void vars_command(char **args){
   printHashTable(local_vars); 
}
