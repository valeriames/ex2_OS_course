#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>
#include <limits.h>


#define MAX_NUM_COUNTER 100
#define MAX_NUM_THREADS 4096
#define MAX_LINE_LENGTH 1024

void parse_worker_line(char *command, int thread_id);
void execute_worker_command(char command[MAX_LINE_LENGTH]) ;

struct thread_data_s  //this is the data the thread gets when it is made
{
    int thread_id;
    int answer;
    long long end_time; 
} thread_data_s;

struct job_node // I implemented the job queue as connected list
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
struct timeval start_time, end_time, dispatcher_start_time;
int log_handler, num_of_files, num_of_threads, line_counter, thread_j = 0; 
long long thread_end, thread_start, dispatcher_start;
long long *thread_end_counter, *dispatcher_count;
long long sum_jobs=0, min_job = LLONG_MAX, max_job = LLONG_MIN, average_job=0; 

FILE* *create_num_counter_file(int counter) //part of dispatcher initiallization
{
    static FILE *file_txt_array[MAX_NUM_COUNTER];
    char filename[12];
 
    for (int i=0; i<counter; i++)//naming of file
    {
        if (i < 10) 
            snprintf(filename, 24, "count0%d.txt", i);
        else 
            snprintf(filename, 24, "count%d.txt", i);

        file_txt_array[i] = fopen(filename, "w+");
        if (file_txt_array[i] == NULL)
        {
            printf("Failed creating counter file num %d\n", i); 
            exit(1);
        }

        
        fputs("0\0", file_txt_array[i]);
        rewind(file_txt_array[i]);
    }
    
    return file_txt_array;
}

FILE **create_thread_files(int num) //creating all thread txt files 
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
            printf("Failed creating thread file num %d\n", num); 
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
        printf("Failed creating dispatcher file\n"); 
        exit(1);
    }
    return dispatcher_file;
}

void create_stats_file(long long sum_jobs_time, long long min_job, long long average_job, long long max_job)
{
    gettimeofday(&end_time, 0);
    long long elapsed = (end_time.tv_sec - start_time.tv_sec)*1000LL + (end_time.tv_usec - start_time.tv_usec)/1000; //total running time 
    FILE *stats_file; 
    stats_file = fopen("stats.txt", "w+");
    if (stats_file == NULL)
    {
        printf("Failed creating stats file\n"); 
        exit(1);        
    }

    fprintf(stats_file, "total running time: %lld milliseconds\n\
sum of jobs turnaround time: %lld milliseconds\n\
min job turnaround time: %lld milliseconds\n\
average job turnaround time: %f milliseconds\n\
max job turnaround time: %lld milliseconds\n"\
, elapsed, sum_jobs_time, min_job, (double)average_job, max_job);

    fclose(stats_file);
}

long long print_to_thread_file(int thread_id, char* command, char* start_or_end) //writes runtimes to thread txt files and returns the time 
{
    gettimeofday(&end_time, 0);
    long long elapsed = (end_time.tv_sec - start_time.tv_sec)*1000LL + (end_time.tv_usec - start_time.tv_usec)/1000;
    fprintf(thread_array[thread_id], "TIME %lld: %s job %s" , elapsed, start_or_end, command); //for now time is 0sec

    return elapsed;
}

long long print_to_dispatcher_file (char* line )//writes runtimes to dispatcher txt files and returns the time 
{
    gettimeofday(&dispatcher_start_time, 0);
    long long elapsed = (dispatcher_start_time.tv_sec - start_time.tv_sec)*1000LL + (dispatcher_start_time.tv_usec - start_time.tv_usec)/1000;
    if (line != NULL) fprintf(dispatcher_file, "TIME %lld: read cmd line: %s", elapsed, line);

    return elapsed;
}

void prepare_stats(long long end_time)
{
    if (end_time>max_job) max_job=end_time;
    if (end_time<min_job) min_job=end_time;
    sum_jobs+=end_time;
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

void * thread_func(void *arg)  //gets thread to run job from instruction file 
{
    struct thread_data_s* td = (struct thread_data_s *)arg;
    int thread_id, *answer, index=0;
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
        
        free(last);
        if (head==last)
            head=NULL;
       
        if (pthread_mutex_unlock(&mutex))
            printf("unlock failed %d\n", thread_id);
        
        //we start working on the command only after unlocking the queue mutex
        //printf("the line we want to execute %s\n", line);
        
        parse_worker_line(line, thread_id);
        busy_list[thread_id]=false;

        if (log_handler == 1) //if log =1 it saves all timing data of when thread was done 
        {
            td->end_time = print_to_thread_file(thread_id, saved_line, "END");
            printf("td->end_time is: %lld\n", td->end_time);
        }
        else 
        {
            gettimeofday(&end_time, 0);
            td->end_time = (end_time.tv_sec - start_time.tv_sec)*1000LL + (end_time.tv_usec - start_time.tv_usec)/1000;
        }

        //printf("before modification: thread end time is: %lld \n", td->end_time);
        td->end_time = td->end_time - dispatcher_count[index];
        //printf("after modification: thread time is: %lld \n             dispatcher time is: %lld \n", td->end_time, dispatcher_count[0]);
        index++;

        prepare_stats(td->end_time);


        pthread_cond_signal(&dispatcher_wait);
    }

}

void initialize_dispatcher()
{
    pthread_t tid; 
    pthread_attr_t attr; 
    struct thread_data_s thread_data;   
    //pthread_mutex_lock(&mutex);
    for (int i=0; i<num_of_threads; i++)
    {
        thread_data.thread_id =i;
        pthread_create(&tid, NULL, thread_func, (void *) &thread_data); 
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

void increment_decrement_or_sleep(char *work, int integer) //implemintation of worker commands 
{
    char num[MAX_LINE_LENGTH]; 
    char *line_command; 

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
                //printf("counter in file %d changed from %s to %lld\n",integer, num, counter+1);
            }
        else if (!strcmp(work, "decrement"))
            {
                rewind(file_array[integer]);
                fprintf(file_array[integer], "%lld\n", counter-1);
                //printf("counter in file %d changed from %s to %lld\n", integer, num, counter-1);
            }

        pthread_mutex_unlock(&file_mutexes[integer]);
    }
}



void parse_worker_line(char *command, int thread_id) //parsing between different command in a single line 
{
    char *line_ptr;
    char *temp_line; 

    if (log_handler == 1) //if log = 1 writes into thread when job started 
    {
        thread_start = print_to_thread_file(thread_id, command, "START"); 
    }

    char *remain=command;
    char *next_commands; 
    
    line_ptr = strtok_r(command, ";", &remain);
    printf("REMAIN IS: %s\n", line_ptr);
    
    while(line_ptr != NULL)
    {   
        if (strstr(line_ptr, "repeat"))
        {
            //remain = strtok(remain, ";");
            //printf("current remain is:%s\n\n", remain);
            char *integers="1234567890";
            char *num = strpbrk(line_ptr, integers);
            strcpy(temp_line, remain); 
            
            
            temp_line = strtok_r(temp_line, ";", &remain);
            while(temp_line != NULL)
            {
                //printf("\n\n\n\n00000000000000000000000000000\n\n\ntemp line is: %s\n00000000000000000000000000000\n\n\n\n", temp_line);
                for (int i = 0; i<atoi(num); i++) execute_worker_command(temp_line);
                temp_line=strtok_r(NULL, ";", &remain);
            }
            
        }
        //printf("next command is %s\n", line_ptr);
        execute_worker_command(line_ptr);
        line_ptr = strtok_r(NULL, ";", &remain);
        
    }
}


void execute_worker_command(char command[MAX_LINE_LENGTH]) 
{
    char *remain=command;
    char line_command_ptr; 
    
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

    if (log_handler ==1) 
    {
        dispatcher_start = print_to_dispatcher_file(line);
    }
    else
    {
        gettimeofday(&dispatcher_start_time, 0);
        dispatcher_start = (dispatcher_start_time.tv_sec - start_time.tv_sec)*1000LL + (dispatcher_start_time.tv_usec - start_time.tv_usec)/1000;
    }

    if (strstr(word, "dispatcher_msleep"))
        {
            sleep(atoi(line)/1000);
        }

    else if (strstr(word, "dispatcher_wait"))
        {
            
            bool flag = false;
            pthread_mutex_lock(&mutex);
            
            while (head != NULL || check_busy()){pthread_cond_wait(&dispatcher_wait, &mutex);}
            pthread_mutex_unlock(&mutex);
        }

    else if (strstr(word, "worker"))
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

void close_files(int file_num, FILE** list_of_files)
{
    for (int i=0;i<file_num; i++)
    {
        fclose(list_of_files[i]);
    }
}

void dispatcher_work(FILE *commands_file)
{
    char line[MAX_LINE_LENGTH];
    char word[MAX_LINE_LENGTH]; //this will be a list of words in the specific line
    line_counter=0;

    
    dispatcher_count = malloc(sizeof(long long)); //documentation of how much time passed since start of running untill the time we called the dispatcher again 

    while(fgets(line, MAX_LINE_LENGTH, commands_file))
    {
        //printf("line counter is:%d\n\n", line_counter);
        
        if (strstr(line, "dispatcher_wait;"))
            dispatcher_execute("dispatcher_wait;", NULL);
        else
            {
            strcpy(line,parse_line(line, word, " "));
            dispatcher_execute(word, line);
            }
        
        
        if (strstr(word, "dispatcher") == NULL)
        {
            line_counter+=1;
            dispatcher_count = realloc(dispatcher_count, sizeof(long long)* line_counter);
            dispatcher_count[line_counter-1] = dispatcher_start;
        }
        

    }
    //cmd file ended
    //we implement the waiting at end of file using the the dispatcher wait
    dispatcher_execute("dispatcher_wait", NULL);
    
    fclose(commands_file);
    
    printf("\n\nwe finished running !!!!\n\n now creating stats.txt\n\n");

    
    average_job = sum_jobs/((long long)line_counter);
    create_stats_file(sum_jobs, min_job, average_job, max_job);
    print_results();
    close_files(num_of_files, file_array);
    if (log_handler == 1)
    {
        close_files(num_of_threads, thread_array); //segmentation fault
        fclose(dispatcher_file); //segmentation fault
    }
    free(dispatcher_count);
    
}
