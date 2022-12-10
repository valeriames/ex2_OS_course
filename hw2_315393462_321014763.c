#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_NUM_COUNTER 100
#define MAX_NUM_THREADS 4096
#define MAX_LINE_LENGTH 1024

int num_of_files;
void parse_worker_line(char *command);
void execute_worker_command(char command[MAX_LINE_LENGTH]) ;
struct thread_data_s  //this is the data the thread gets when it is made
{
    int thread_id;
    int answer;
} thread_data_s;

struct job_node // I implemented the job queue as connected list
//with pointer to the head and pointer to the bottom, do you have better idea?
{
    char job_text[MAX_LINE_LENGTH];
    struct job_node* next;
} job_node;

struct job_node * head;
pthread_mutex_t mutex; //this is the mutex for the job queue
pthread_mutex_t file_mutexes[MAX_NUM_COUNTER]; //these are mutexes for the file
pthread_cond_t available_job=PTHREAD_COND_INITIALIZER;
pthread_cond_t dispatcher_wait=PTHREAD_COND_INITIALIZER;
FILE **file_array;
bool busy_list[MAX_NUM_THREADS]={false}; // this array tells us if a thread is working

FILE* *create_counter_files(int counter, int flag) //part of dispatcher initiallization //need to change counter to long long?  //flag: 0 for counter file, 1 for thread file
//created num counter file with "0" inside
{
    static FILE *file_array[MAX_NUM_COUNTER];
    char filename[12];
    char file_type[8]; 
    if (flag == 0) strcpy(file_type, "count"); 
    else if (flag ==1) strcpy(file_type, "thread");

    for (int i=0; i<counter; i++)
    {
        if (i < 10)
            snprintf(filename, 24, "%s0%d.txt",file_type, i);
        else 
            snprintf(filename, 24, "%s%d.txt",file_type, i);

        file_array[i] = fopen(filename, "w+");
        if (file_array[i] == NULL)
        {
            printf("Failed creating counter file\n"); 
            exit(1);
        }

        fputs("0\0", file_array[i]);
        rewind(file_array[i]);
        //char check[MAX_LINE_LENGTH];
        //fgets(check, MAX_LINE_LENGTH, file_array[i]);
        //printf("CHECKING FILE:%s\n\n\n", check);
    }
    
    return file_array;
}

struct job_node* delete_and_free_last(struct job_node *head)
{
    struct job_node *second_last=head, *last=head;
    if (second_last->next==NULL) //only one element in linked list
    {
        //printf("the only element is %s", head->job_text);
        last=head;
        return last;
    }
    while (second_last->next->next!=NULL)
        second_last=second_last->next;
    last=second_last->next;
    second_last->next=NULL;
    return last;
}
void * thread_func(void *arg) //need to go through it, Gadis implemintation as part of creating new thread, gets an error 
{
    struct thread_data_s* td = (struct thread_data_s *)arg;
    int i=0, thread_id, *answer;
    thread_id = td->thread_id;
   
    while (1)
    {
        
        if (pthread_mutex_lock(&mutex))
            printf("lock failed\n");    
        
        while (head == NULL) pthread_cond_wait(&available_job, &mutex);
        busy_list[thread_id]=true; //this thread is busy
        struct job_node *last = head;
        last=delete_and_free_last(head); //now last is the last node in the queue - the job we take
        
        
        char *line=malloc(MAX_LINE_LENGTH*sizeof(char));
        strcpy(line, last->job_text);
        //printf("thread %d woke up and took: %s\n", thread_id, line);
        // printf("the line we want to execute %s\n", last->job_text);
        // parse_worker_line(last->job_text);
        printf("the line we want to execute %s\n", line);
        parse_worker_line(line);
        free(line);
        free(last);
        if (head==last)
            head=NULL;
        //pthread_cond_signal(&empty_job_queue);
        //printf("this is %d\n", thread_id);
        if (pthread_mutex_unlock(&mutex))
            printf("unlock failed %d\n", thread_id);
        
        //we start working on the command only after unlocking the queue mutex
        
        busy_list[thread_id]=false;
        pthread_cond_signal(&dispatcher_wait);
    }
}

void initialize_dispatcher(int num_of_threads, int num_of_files)
{
    pthread_t tid; 
    pthread_attr_t attr; 
    struct thread_data_s thread_data;
    int flag = 1; //for create_counter_files to handle it as thread files
    //FILE *file_array[MAX_NUM_COUNTER];


    //pthread_mutex_lock(&mutex);
    for (int i=0; i<num_of_threads; i++)
        {
        thread_data.thread_id =i;
        pthread_create(&tid, NULL, thread_func, (void *) &thread_data); //error in this thread_func, therefore commented out for now 
        
        //printf("we created thread number %d!! :)\n", i);
        
        //pthread_join(tid, NULL);
        }
    create_counter_files(num_of_threads, flag);
}
char* parse_line(char *line, char *word, char *pattern)
{
    for (int i=0; i<1; i++)
    {
        strcpy(word,strsep(&line, pattern));
        if (word == NULL)
            {
                return NULL;
            }
        if (strlen(word) == 0)
            i--;
    }
    return line;
}

void increment_decrement_or_sleep(char *work, int integer)
{
    char num[MAX_LINE_LENGTH]; 

    if (integer>num_of_files-1)
    {
        printf("integer of worker command %s is %d and is bigger than number of count files %d\n", work, integer, num_of_files);
    }
    else
    {
        pthread_mutex_lock(&file_mutexes[integer]);
        rewind(file_array[integer]);
        fgets(num, MAX_LINE_LENGTH, file_array[integer]);
        //printf("counter value read is %s\n", num);
        long long int counter=strtol(num, NULL, 10);
        if (!strcmp(work, "increment"))
            {
                rewind(file_array[integer]);
                fprintf(file_array[integer], "%lld", counter+1);
                printf("counter in file %d changed from %s to %lld\n",integer, num, counter+1);
            }
        if (!strcmp(work, "decrement"))
            {
                rewind(file_array[integer]);
                fprintf(file_array[integer], "%lld", counter-1);
                printf("counter in file %d changed from %s to %lld\n", integer, num, counter-1);
            }
        pthread_mutex_unlock(&file_mutexes[integer]);
    }
    //fputs("valeria: check\n", *(file_array+integer));

}
void parse_worker_line(char *command)
{
    char *line_ptr;
    line_ptr = strtok(command, ";");
    
    while(line_ptr != NULL)
    {   

        //printf("next command is %s\n", line_ptr);
        execute_worker_command(line_ptr);
        line_ptr = strtok(NULL, ";");
    }

    
}
void execute_worker_command(char command[MAX_LINE_LENGTH]) 
{
    while(command[0] == 32) //whitespace in ascii, removes whitespaces at start of sentence 
    {
        //printf("entered loop %s\n", command);
        for (int j=0; j<strlen(command)-1; j++)
        {
            command[j] = command[j+1]; 
        }
        command[strlen(command)-1] = 0;
    }
    char *integers="1234567890";
    char *num = strpbrk(command, integers);
    //printf("check:%s and the length is %ld\n", command, strlen(command));
    if (!strcmp(command, "msleep"))
    {
        printf("thread slept for %s msec\n", num);
        sleep(atoi(num)/1000); 
    }
    if (strstr(command, "increment"))
    {
        //DO STUFF
        //printf("work is: %s, integer is: %s\n", "increment", num);
        char num_file[MAX_LINE_LENGTH];
        increment_decrement_or_sleep("increment", atoi(num));
    }
    else if (strstr(command, "decrement"))
    {
        //printf("work is: %s, integer is: %s\n", "decrement", num);
        increment_decrement_or_sleep("decrement", atoi(num));
    }

    // else if (strstr(comm_string, "repeat"))
    // {
    //     //printf("entered repeat: comm is:%s\n\n", command);
    //     //DO STUFF
    // }

    // // free(comm_string);
}

bool check_busy()
{
    for (int i=0; i<MAX_NUM_THREADS; i++)
            if (busy_list[i])
                    return true;
    return false;
}

void dispatcher_execute(char *word, char*line)
{
    struct job_node* ptr =NULL;
    if (!strcmp(word, "dispatcher_msleep"))
        {
            sleep(atoi(line)/1000); 
        }

    if (!strcmp(word, "dispatcher_wait"))
        {
            bool flag = false;
            pthread_mutex_lock(&mutex);
            while (head != NULL || check_busy()) pthread_cond_wait(&dispatcher_wait, &mutex);
            pthread_mutex_unlock(&mutex);
        }

    else if (!strcmp(word, "worker"))
        {
            pthread_mutex_lock(&mutex);

            ptr = (struct job_node*)malloc(sizeof(job_node));
            strcpy(ptr->job_text,line);
            if (head==NULL)
                {
                    ptr->next=NULL;
                }
            else
                {
                    ptr->next=head;
                }
            head = ptr;
            
            pthread_mutex_unlock(&mutex);
            pthread_cond_signal(&available_job);
        }
    else
        printf("this line is not recognized: %s\n", word);
}

void dispatcher_work(FILE *commands_file)
{
    char line[MAX_LINE_LENGTH];
    char word[MAX_LINE_LENGTH]; //this will be a list of words in the specific line
    int line_counter=0;
    while(fgets(line, MAX_LINE_LENGTH, commands_file))
    {
        strcpy(line,parse_line(line, word, " "));
        line_counter+=1;
        //printf("word is:%s, line is:%s\n", word,line); 
        dispatcher_execute(word, line);
    }
    //cmd file ended
    //we implement the waiting at end of file using the the dispatcher wait
    dispatcher_execute("dispatcher_wait", NULL);
    sleep(1);
    fclose(commands_file);
    printf("\n\nwe finished running !!!!\n\n");
}

int main(int argc, char **argv)
{
    FILE *read_file; 
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return 1;}
    for (int i=0; i<MAX_NUM_COUNTER; i++)
    {
    if (pthread_mutex_init(&file_mutexes[i], NULL) != 0) {
        printf("\n file mutex %d init has failed\n", i);
        return 1;}   
    }
    read_file = fopen(argv[1], "r"); 
    if (read_file == NULL)
    {
        printf("Error: File did not open\n");
        exit(-1);
    }

    if (argc!=5) //check that num of args in command are correct 
    {
        printf("Error: incompatible num of arguments\n");
        exit(-1);
    }
    int num_of_threads = atoi(argv[2]); //num threads that are created according to command
    num_of_files = atoi(argv[3]);
    //int *result;
    int flag = 0; //for create_counter_files to handle it for counter files 
    file_array=create_counter_files(num_of_files, flag);
    initialize_dispatcher(num_of_threads, num_of_files); //creates required number of threads
    dispatcher_work(read_file);

}
//to myself - the linked list is not working because the "job_data" is only a pointer and the actual string is in the stack
//solution - either have all arr
