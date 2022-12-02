#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <stdbool.h>

#define MAX_NUM_COUNTER 100
#define MAX_NUM_THREADS 4096

////////////////////currently: need to look again at creating new threads. all methods here are taken from Gadi's recitation and need to be modified. 

struct thread_data_s 
{
    int limit;
    int answer;
} thread_data_s;

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

void thread_func(void *arg) //need to go through it, Gadis implemintation as part of creating new thread, gets an error 
{
    struct thread_data_s* td = (struct thread_data_s *)arg;
    int i, limit, *answer, sum = 0;

    limit = td->limit;
    for(i=0; i<limit;i++)
    {
        sum++;
    }
    
    td->answer = sum; 
    answer = malloc(sizeof(*answer));
    *answer = sum; 
    pthread_exit(answer);
}

int main(int argc, char **argv)
{
    FILE *read_file; 
    pthread_t tid; 
    pthread_attr_t attr; 
    struct thread_data_s thread_data;

    read_file = fopen(argv[1], "r"); 
    if (read_file == NULL)
    {
        printf("Error: File did not open\n");
        exit(-1);
    }

    if (argv!=5) //check that num of args in command are correct 
    {
        printf("Error: incompatible num of arguments\n");
        exit(-1);
    }
    thread_data.limit = atoi(argv[2]); //num threads that are created according to command

    create_num_counters_file(argv[3]);

    pthread_attr_init(&attr);
    //pthread_create(&tid, &attr, thread_func, (void *) &thread_data); //error in this thread_func, therefore commented out for now 
    pthread_join(tid, (void**)&result);
}