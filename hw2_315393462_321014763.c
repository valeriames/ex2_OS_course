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
pthread_mutex_t mutex;
pthread_cond_t available_job=PTHREAD_COND_INITIALIZER;
pthread_cond_t empty_job_queue=PTHREAD_COND_INITIALIZER;

FILE* *create_num_counters_file(int num_counters) //part of dispatcher initiallization //need to change num_counters to long long? 
//created num counter file with "0" inside
{
    static FILE *cntr_file_array[MAX_NUM_COUNTER];
    char filename[12];
    for (int i=0; i<num_counters; i++)
    {
        if (i < 10)
        {
            snprintf(filename, 12, "count0%d.txt", i);
            printf("counter file that was made: %s\n", filename);
        }
        else 
        {
            snprintf(filename, 12, "count%d.txt", i);
            printf("counter file that was made: %s\n", filename);
        }

        cntr_file_array[i] = fopen(filename, "w");
        if (cntr_file_array[i] == NULL)
        {
            printf("Failed creating counter file\n"); 
            exit(1);
        }

        fputs("0\0", cntr_file_array[i]);
    }
    
    return cntr_file_array;
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
        struct job_node *last = head;
        last=delete_and_free_last(head); //now last is the last node in the queue - the job we take
        printf("thread %d woke up and took: %s\n", thread_id, last->job_text);
        free(last);
        if (head==last)
            head=NULL;
        //pthread_cond_signal(&empty_job_queue);
        //printf("this is %d\n", thread_id);
        if (pthread_mutex_unlock(&mutex))
            printf("unlock failed %d\n", thread_id);

    }
}

void initialize_dispatcher(int num_of_threads, int num_of_files, FILE ** file_array)
{
    pthread_t tid; 
    pthread_attr_t attr; 
    struct thread_data_s thread_data;

    //FILE *cntr_file_array[MAX_NUM_COUNTER];

    //check that you can access all files:
    //for (int j = 0; j< num_of_files; j++)
        //fputs("check\0", file_array[j]);

    //pthread_mutex_lock(&mutex);
    for (int i=0; i<num_of_threads; i++)
        {
        thread_data.thread_id =i;
        pthread_create(&tid, NULL, thread_func, (void *) &thread_data); //error in this thread_func, therefore commented out for now 
        
        printf("we created thread number %d!! :)\n", i);
        
        //pthread_join(tid, NULL);
        }
    
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

void increment_or_decrement(FILE **file_array, char *work, int integer)
{
    char num[MAX_LINE_LENGTH]; 
    char updated_num[MAX_LINE_LENGTH];
    fgets(num, MAX_LINE_LENGTH, file_array[integer]);
    printf("num is: %s\n", &num);
    //strcpy(updated_num,parse_line(num, updated_num, 0));


    //fputs("valeria: check\n", *(file_array+integer));

}

void execute_worker_command(FILE **file_array, char command[MAX_LINE_LENGTH]) 
{
    //printf("check:%s\n", command[0]);
    char *work, *integer; 
    char* comm_string = malloc(sizeof(char)*strlen(command));
    if (comm_string == NULL)
    {
        printf("failed allocating mem for worker\n");
        exit(1);
    }

    else 
    {
        strcpy(comm_string, command);
        while(comm_string[0] == 32) //whitespace in ascii, removes whitespaces at start of sentence 
        {
            //printf("entered loop %s\n", comm_string);
            for (int j=0; j<strlen(comm_string)-1; j++)
            {
                comm_string[j] = comm_string[j+1]; 
            }
            comm_string[strlen(comm_string)-1] = 0;
        }

        if (strstr(comm_string, "increment") || strstr(comm_string, "decrement"))
        {
            //DO STUFF
            work = strtok(comm_string, " ");
            integer = strtok(NULL, " "); 
            //printf("work is: %s, integer is: %s\n", work,integer);

            increment_or_decrement(file_array, work, atoi(integer));

        }
    
        else if (strstr(comm_string, "repeat"))
        {
            //printf("entered repeat: comm is:%s\n\n", command);
            //DO STUFF
        }
    }

    free(comm_string);
}

void dispatcher_work(FILE *commands_file, FILE **file_array)
{
    char line[MAX_LINE_LENGTH];
    char word[MAX_LINE_LENGTH]; //this will be a list of words in the specific line
    int line_counter=0;
    struct job_node* ptr =NULL;
    while(fgets(line, MAX_LINE_LENGTH, commands_file))
    {
        strcpy(line,parse_line(line, word, " "));
        line_counter+=1;
        printf("word is:%s, line is:%s\n", word,line);
 
        if (!strcmp(word, "dispatcher_msleep")) //i think that it should appear with format such as: "worker msleep 5; increment 3;" 
                                                //so the comparison should probably inside "if" condition on "worker"
                                                //according to: moodle.tau.ac.il/mod/forum/discuss.php?d=20668
        {
            //printf("num is: %d\n", atoi(line));
            sleep(atoi(line)/1000); 
        }

        if (!strcmp(word, "dispatcher_wait")) //same note as privous on msleep
            {
                printf("not supported yet");
            }

        else if (!strcmp(word, "worker"))
            {
                pthread_mutex_lock(&mutex);
                printf ("we inserted %d worker jobs\n", line_counter);

                char *line_ptr;
                char line_exec[MAX_LINE_LENGTH]; 
                line_ptr = strtok(line, ";");
                
                while(line_ptr != NULL)
                {   
                    strcpy(line_exec, line_ptr);
                    execute_worker_command(file_array, line_exec);
                    line_ptr = strtok(NULL, ";");
                }


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
    // printf("\n\nList elements are - \n");
    // struct job_node* temp=head;
    // while(temp != NULL) {
    //     printf("%s --->",temp->job_text);
    //     temp = temp->next;}
    
    sleep(5);
    // I want to wait for all workers to finish here, how do we implement that?
    fclose(commands_file);
}
int main(int argc, char **argv)
{
    FILE *read_file; 
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return 1;}
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
    int num_of_files = atoi(argv[3]);
    int *result;
    
    FILE **file_array;
    file_array=create_num_counters_file(num_of_files);
    initialize_dispatcher(num_of_threads, num_of_files, file_array); //currently only creates required number of threads
    //fputs("valeria check \n", *file_array);
    dispatcher_work(read_file, file_array);

}
//to myself - the linked list is not working because the "job_data" is only a pointer and the actual string is in the stack
//solution - either have all arr
