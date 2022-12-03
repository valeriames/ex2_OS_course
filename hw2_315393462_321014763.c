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

////////////////////currently: need to look again at creating new threads. all methods here are taken from Gadi's recitation and need to be modified. 

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
struct job_node * tail;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t available_job=PTHREAD_COND_INITIALIZER;

void create_num_counters_file(int num_counters) //part of dispatcher initiallization //need to change num_counters to long long? 
//created num counter file with "0" inside
{
    FILE *num_cntr_file;
    char filename[12];
    if (num_counters < 10)
    {
        snprintf(filename, 12, "count0%d.txt", num_counters);
        printf("counter file that was made: %s\n", filename);
        //num_cntr_file = fopen()
    }
    else 
    {
        snprintf(filename, 12, "count%d.txt", num_counters);
        printf("counter file that was made: %s\n", filename);
    }

    num_cntr_file = fopen(filename, "w");
    if (num_cntr_file == NULL)
    {
        printf("Failed creating counter file\n"); 
        exit(1);
    }

    fputs("0\0", num_cntr_file);
}
struct job_node* find_job_to_take(struct job_node *head)
{
    while (head->next!=NULL)
        head=head->next;
    printf("the last is %s\n", head->job_text);
    return head;
}
void * thread_func(void *arg) //need to go through it, Gadis implemintation as part of creating new thread, gets an error 
{
    struct thread_data_s* td = (struct thread_data_s *)arg;
    int i=0, thread_id, *answer;
    thread_id = td->thread_id;
   
    while (1)
    {
        pthread_mutex_lock(&mutex);
        while (head == NULL) pthread_cond_wait(&available_job, &mutex);
        struct job_node *last = head;
        last=find_job_to_take(last); //now last is the last node in the queue - the job we take
        printf("thread %d woke up and took: %s\n", thread_id, last->job_text);
        free(last);
        pthread_mutex_unlock(&mutex);
    }
}

void initialize_dispatcher(int num_of_threads)
{
    pthread_t tid; 
    pthread_attr_t attr; 
    struct thread_data_s thread_data;
    pthread_mutex_lock(&mutex);
    for (int i=0; i<num_of_threads; i++)
        {
        thread_data.thread_id =i;
        pthread_create(&tid, NULL, thread_func, (void *) &thread_data); //error in this thread_func, therefore commented out for now 
        
        printf("we created thread number %d!! :)\n", i);
        
        //pthread_join(tid, (void**)&result);
        }
}
char* parse_line(char *line, char *word)
{
    for (int i=0; i<1; i++)
    {
        strcpy(word,strsep(&line, " "));
        if (word == NULL)
            {
                return NULL;
            }
        if (strlen(word) == 0)
            i--;
    }
    return line;
}

void dispatcher_work(FILE *commands_file)
{
    char line[MAX_LINE_LENGTH];
    char word[MAX_LINE_LENGTH]; //this will be a list of words in the specific line
    int line_counter=0;
    struct job_node* ptr =NULL;
    while(fgets(line, MAX_LINE_LENGTH, commands_file))
    {
        strcpy(line,parse_line(line, word));
        line_counter+=1;
        if (!strcmp(word, "dispatcher_msleep"))
            //sleep(atoi(words[1])/1000); // TODO: check the '\n' doesn't breaks it
        if (!strcmp(word, "dispatcher_wait"))
            {
                printf("not supported yet");
            }
        if (!strcmp(word, "worker"))
            {
                //pthread_mutex_lock(&mutex);
                printf ("we inserted %d worker jobs\n", line_counter);
                
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
                
                pthread_cond_signal(&available_job);
                pthread_mutex_unlock(&mutex);
            }
        else
            printf("this line is not recognized: %s\n", word);
    }
    printf("\n\nList elements are - \n");
    struct job_node* temp=head;
    while(temp != NULL) {
        printf("%s --->",temp->job_text);
        temp = temp->next;}
    //sleep(15);
    // I want to wait for all workers to finish here, how do we implement that?
    fclose(commands_file);
}
int main(int argc, char **argv)
{
    FILE *read_file; 
    
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
    int *result;
    create_num_counters_file(atoi(argv[3]));
    initialize_dispatcher(num_of_threads); //currently only creates required number of threads
    
    dispatcher_work(read_file);

}
//to myself - the linked list is not working because the "job_data" is only a pointer and the actual string is in the stack
//solution - either have all arr
