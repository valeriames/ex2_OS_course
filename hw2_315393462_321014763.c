#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>

#define MAX_NUM_COUNTER 100
#define MAX_NUM_THREADS 4096
#define MAX_LINE_LENGTH 1024

int num_of_files;
void parse_worker_line(char *command, int thread_id);
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
FILE **file_array, **thread_array, *dispatcher_file;
bool busy_list[MAX_NUM_THREADS]={false}; // this array tells us if a thread is working
time_t start_time, end_time;
int log_handler; 

FILE* *create_num_counter_file(int counter) //part of dispatcher initiallization //need to change counter to long long?  //flag: 0 for counter file, 1 for thread file
//created num counter file with "0" inside
{
    static FILE *file_txt_array[MAX_NUM_COUNTER];
    char filename[12];
 
    for (int i=0; i<counter; i++)
    {
        if (i < 10)
            snprintf(filename, 24, "count0%d.txt", i);
        else 
            snprintf(filename, 24, "count%d.txt", i);

        file_txt_array[i] = fopen(filename, "w+");
        if (file_txt_array[i] == NULL)
        {
            printf("Failed creating counter file\n"); 
            exit(1);
        }

        
        fputs("0\0", file_txt_array[i]);
        rewind(file_txt_array[i]);

        //char check[MAX_LINE_LENGTH];
        //fgets(check, MAX_LINE_LENGTH, file_array[i]);
        //printf("CHECKING FILE:%s\n\n\n", check);
    }
    
    return file_txt_array;
}

FILE **create_thread_files(int num)
{
    static FILE *file_txt_array[MAX_NUM_COUNTER];
    char filename[12];

    if (num < 10)
            snprintf(filename, 24, "thread0%d.txt", num);
        else 
            snprintf(filename, 24, "thread%d.txt", num);

        file_txt_array[num] = fopen(filename, "w+");
        if (file_txt_array[num] == NULL)
        {
            printf("Failed creating counter file\n"); 
            exit(1);
        }

        
    fputs("0\0", file_txt_array[num]);
    rewind(file_txt_array[num]);

    return file_txt_array;

}

FILE* create_dispatcher_file()
{
    FILE *dispatcher_file; 
    dispatcher_file = fopen("dispatcher.txt", "w+");
    if (dispatcher_file == NULL)
    {
        printf("Failed creating counter file\n"); 
        exit(1);
    }
    return dispatcher_file;
}

void print_to_thread_file(int thread_id, long long time, char* command, char* start_or_end)
{
    fprintf(thread_array[thread_id], "TIME %lld: %s job %s\n" , time, start_or_end, command); //for now time is 0sec
}

void print_to_dispatcher_file (char* line )
{
    time(&end_time);
    long long diff =(long long)difftime(end_time, 0);
    fprintf(dispatcher_file, "TIME %lld: read cmd line: %s\n", diff/1000, line);
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
        
        
        char line[MAX_LINE_LENGTH];
        char saved_line[MAX_LINE_LENGTH]; //save original line 
        
        strcpy(line, last->job_text);
        strcpy(saved_line, line);
        //printf("thread %d woke up and took: %s\n", thread_id, line);
        // printf("the line we want to execute %s\n", last->job_text);
        // parse_worker_line(last->job_text);
        
        free(last);
        if (head==last)
            head=NULL;
        //pthread_cond_signal(&empty_job_queue);
        //printf("this is %d\n", thread_id);
        if (pthread_mutex_unlock(&mutex))
            printf("unlock failed %d\n", thread_id);
        
        //we start working on the command only after unlocking the queue mutex
        time(&end_time);
        printf("the line we want to execute %s\n", line);
        
        parse_worker_line(line, thread_id);
        busy_list[thread_id]=false;
        time(&end_time);
        long long elapsed = (long long)difftime(end_time, start_time)/1000;
        if (log_handler == 1) print_to_thread_file(thread_id, elapsed, saved_line, "END");

        pthread_cond_signal(&dispatcher_wait);
    }
}

void initialize_dispatcher(int num_of_threads, int num_of_files)
{
    pthread_t tid; 
    pthread_attr_t attr; 
    struct thread_data_s thread_data;   
    //pthread_mutex_lock(&mutex);
    for (int i=0; i<num_of_threads; i++)
    {
        thread_data.thread_id =i;
        pthread_create(&tid, NULL, thread_func, (void *) &thread_data); //error in this thread_func, therefore commented out for now 
        if (log_handler == 1) thread_array = create_thread_files(i);
        //printf("we created thread number %d!! :)\n", i);
        
        //pthread_join(tid, NULL);
    }
    
    if (log_handler == 1) dispatcher_file = create_dispatcher_file();
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
                fprintf(file_array[integer], "%lld\n", counter+1);
                printf("counter in file %d changed from %s to %lld\n",integer, num, counter+1);
            }
        if (!strcmp(work, "decrement"))
            {
                rewind(file_array[integer]);
                fprintf(file_array[integer], "%lld\n", counter-1);
                printf("counter in file %d changed from %s to %lld\n", integer, num, counter-1);
            }
        pthread_mutex_unlock(&file_mutexes[integer]);
    }
    //fputs("valeria: check\n", *(file_array+integer));

}



void parse_worker_line(char *command, int thread_id)
{
    char *line_ptr;

    long long elapsed = (long long)difftime(end_time, start_time)/1000;
    if (log_handler == 1) print_to_thread_file(thread_id, elapsed, command, "START"); 

    char *remain=command;
    line_ptr = strtok_r(command, ";", &remain);
    
    while(line_ptr != NULL)
    {   

        //printf("next command is %s\n", line_ptr);
        execute_worker_command(line_ptr);
        line_ptr = strtok_r(NULL, ";", &remain);
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
    if (strstr(command, "msleep"))
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

}

bool check_busy()
{
    for (int i=0; i<MAX_NUM_THREADS; i++)
            if (busy_list[i])
                    return true;
    return false;
}

void dispatcher_execute(char *word, char *line)
{
    struct job_node* ptr =NULL;
    if (log_handler ==1) print_to_dispatcher_file(line);
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

void print_results()
{
    char *str;
    for (int i=0; i<num_of_files; i++)
    {
        rewind(file_array[i]);
        printf("for file number %d the counter value is %s\n", i, fgets(str, 10, file_array[i]));
    }
}

void close_files()
{
    for (int i=0;i<num_of_files; i++)
    {
        fclose(file_array[i]);
    }
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
    //sleep(5);
    fclose(commands_file);
    
    printf("\n\nwe finished running !!!!\n\n");
    close_files();
    //print_results();
}

int main(int argc, char **argv)
{
    time(&start_time);
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
    log_handler = atoi(argv[4]);
    //int *result;
    file_array=create_num_counter_file(num_of_files);
    initialize_dispatcher(num_of_threads, num_of_files); //creates required number of threads
    dispatcher_work(read_file);

}
//to myself - the linked list is not working because the "job_data" is only a pointer and the actual string is in the stack
//solution - either have all arr
