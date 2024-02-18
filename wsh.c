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
#define LOAD_THRESHOLD .5
#define DEFAULT_HIST_LENGTH 5


void exit_command(char **args);
void cd_command(char **args);
void export_command(char **args);
void local_command(char **args);
void vars_command(char **args);
void history_command(char **args);


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
    &history_command
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
    int order;
    char *key;
    char *value;      //only storing string values here
    struct Entry *next;
} Entry;

typedef struct HashTable {
    Entry **buckets;
    int nBuckets;
} HashTable;

//UNSURE IF LINKED LIST IS BETTER THAN having order value in HashTable
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

int local_insertions;
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
    int nBuckets = 50;
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

void HashTable_free(HashTable *h) {
    for (int i = 0; i < h->nBuckets; i++) {
        Entry *v = h->buckets[i];
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

HashTable *resize_hash_table(HashTable *h){
    int newNBuckets = h->nBuckets * 2;
    HashTable *newh = malloc(sizeof(HashTable));
    if (newh == NULL){
        exit(EXIT_FAIL);
    }
    newh->nBuckets = newNBuckets;
    newh->buckets = calloc(newNBuckets, sizeof(Entry *));
    if (newh->buckets == NULL){
        exit(EXIT_FAIL);
    }
    for (int i = 0; i < h->nBuckets; i++){
        Entry *v = h->buckets[i];
        while (v != NULL) {
            unsigned long bucket = get_bucket(newh, v->key);

            Entry *newVal = malloc(sizeof(Entry));
            if (newVal == NULL){
                exit(EXIT_FAIL);
            }
            newVal->key = strdup(v->key);
            newVal->value = v->value;
            newVal->order = v->order;
            newh->buckets[bucket] = newVal;
            v = v->next;
        }
    }
    HashTable_free(h);
    local_vars = newh;
    return newh;
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

    if (((double)local_insertions / (double)h->nBuckets) >= LOAD_THRESHOLD) {
        h = resize_hash_table(h);
        bucket = get_bucket(h,key);
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
    newVal->order = local_insertions++;
    h->buckets[bucket] = newVal;
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


//////////////////////////////////////////////////////////////////////////////////
///                         HASH TABLE END                                     ///
/////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////
///                         CIRCULAR Queue Start                               ///
/////////////////////////////////////////////////////////////////////////////////


typedef struct queue {
    char **command;
    int head, tail, num_entries, size;
} queue;

//GLOBAL SET for HISTORY
queue *history;

queue *init_queue(int max_size) {
    queue *q = malloc(sizeof(queue));
    if (q == NULL){
        exit(EXIT_FAIL);
    }
    q->size = max_size;
    q->command = (char **)malloc(q->size * sizeof(char *));
    if (q->command == NULL){
        exit(EXIT_FAIL);
    }
    q->num_entries = 0;
    q->head = 0;
    q->tail = 0;
    return q;
}

int queue_empty(queue *q){
    return (q->num_entries == 0);
}

int queue_full(queue *q){
    return (q->num_entries == q->size);
}

void queue_destroy(queue *q) {
    free(q->command);
    free(q);
}

char *dequeue(queue *q){
    char *result;

    if (queue_empty(q)){
        return NULL;
    }

    result = q->command[q->head];
    q->head = (q->head + 1) % q->size;
    q->num_entries--;

    return result;
}

int enqueue(queue *q, char *value) {
     
    if (queue_full(q)) {
        if (q->size == 0){
            return 1;
        }
        dequeue(q);        
    }
    q->command[q->tail] = value;
    q->num_entries++;
    q->tail = (q->tail + 1) % q->size;
    return 1;
}

queue *resize_queue(queue *q, int new_size){
    queue *new_queue = init_queue(new_size);
    if (new_size == 0){
        return new_queue;
    }

    int count = 0;
    int num_vals = q->num_entries;
    while (count < num_vals){
        enqueue(new_queue, dequeue(q));
        count++;
    }

    queue_destroy(q);

    return new_queue;
}

void display_queue(queue *q){
    int i = q->tail - 1;
    if (i < 0) {
        i += q->size;
    }
    int count = 0;

    while (count < q->num_entries){
        printf("%d) %s\n",count + 1, q->command[i]);
        i = (i - 1 + q->size) % q->size;
        count++;
    }
}

char *get_queue(queue *q, int n){
    if (queue_empty(q)){
        return NULL;
    }
    int i = q->tail -1;
    if (i < 0) {
        i += q->size;
    }
    int count = 0;
    while (count < q->num_entries){
        if (count + 1 == n){
            return q->command[i];
        }else{
            i = (i - 1 + q->size) % q->size;
            count++;
        }
    }
    return NULL;
}



//////////////////////////////////////////////////////////////////////////////////
///                         CIRCULAR Queue END                                ///
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

    history = init_queue(DEFAULT_HIST_LENGTH);

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
        if (input_read == -1 ){
//            printf("\n");
            free(string);
            exit(0);
        }
        string[input_read - 1] = '\0';

        //REMEMBER TO FREE THIS
        char *input_string = strdup(string);
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
                        char *temp;
                        temp = getenv(++command);
                        if (temp == NULL){
                            if (local_vars == NULL){
                                temp = "";
                            } else{
                                temp = HashTable_get(local_vars, command);
                                if (temp == NULL){
                                    temp = "";
                                }
                            }
                        }
                        command = temp;
                    }
                    if (strcmp(command, "") != 0){
                        args[num_args++] = command;
                    }
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

            if (built_in_flag){
                break;
            }else{
                char *recent_cmd = get_queue(history, 1);
                if (queue_empty(history)){
                    enqueue(history, input_string);
                }else if (recent_cmd != NULL && strcmp(get_queue(history, 1),input_string) != 0){
                    enqueue(history, input_string);
                }
            }

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
                printf("execvp: No such file or directory\n");
                //why are we exit failing here, based on test I do, but
                //to me makes more sense to just continue giving prompts
                exit(EXIT_FAIL);

            }else {
                //need to close previous pipe, if have a new one
                if (i > 0){
                    close(pipefds[i-1][0]);
                    close(pipefds[i-1][1]);
                }
            }
            free(args);
        }
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
    if (args [1] == NULL || args[2] != NULL){
        exit(EXIT_FAIL);
    }
    if (local_vars == NULL){
        local_vars = HashTable_new();
        local_insertions = 0;
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
        HashTable_set(local_vars, strdup(local_name), strdup(local_value));
    }
    free(input_copy);
}


void vars_command(char **args){
    if (args[1] != NULL){
        exit(EXIT_FAIL);
    }
    if (local_vars == NULL){
        return;
    }
    int hash_size = local_vars->nBuckets;

    char *local_list[hash_size];
    //without this loop I get free(): invalid pointer error
    for (int i = 0; i < hash_size; i++) {
        local_list[i] = ""; // Set each pointer to an empty string
    }

    for (int i = 0; i < hash_size; i++){
        Entry *v = local_vars->buckets[i];
        while (v != NULL){
            
            int index = v->order;
            int name_len = strlen(v->key);
            int value_len = strlen(v->value);
            int max_length = name_len + value_len + 10;
            //256 for now idk if this is good
            local_list[index] = (char *)malloc(max_length * sizeof(char));

            if (local_list[index] == NULL){
                exit(EXIT_FAIL);
            }

            snprintf(local_list[index], max_length, "%s=%s", v->key, v->value);

            v = v->next;
        }
    }

    for (int i = 0; i < hash_size; i++){
        if (strcmp(local_list[i], "") != 0){
            printf("%s\n",local_list[i]);
            free(local_list[i]);
        }
    }
}


void history_command(char **args){
    if (args[1] == NULL){
        display_queue(history);
    }else if (strcmp(args[1], "set") == 0){
        history = resize_queue(history, atoi(args[2]));
    }else if (args[1] != NULL){
        int n = atoi(args[1]);
        if (n > history->size){
            return;
        }
        char *history_command = get_queue(history, n);
        if (history_command == NULL){
            return;
        }

        //2/17/2024 THIS FOLLOWING PIECE OF CODE IS DISGUSTING(but it works), REFACTOR IF FINISH OTHER HW
        
        char *token;
        char **commands = malloc(INITIAL_ALLOC_SIZE * sizeof(char*));
        int num_pipes = 0;
        int max_pipes = INITIAL_ALLOC_SIZE;


        char *temp = history_command;

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
                    if (command[0] == '$'){
                        char *temp;
                        temp = getenv(++command);
                        if (temp == NULL){
                            if (local_vars == NULL){
                                temp = "";
                            } else{
                                temp = HashTable_get(local_vars, command);
                                if (temp == NULL){
                                    temp = "";
                                }
                            }
                        }
                        command = temp;
                    }
                    if (strcmp(command, "") != 0){
                        args[num_args++] = command;
                    }
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
        for (int i = 0; i < num_pipes; i++) {
            wait(NULL);
        }
        free(commands);
    }
}
