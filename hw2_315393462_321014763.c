#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>
#include <limits.h>

#include "hw2_315393462_321014763.h"

int main(int argc, char **argv)
{
    gettimeofday(&start_time, 0);

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

    if (argc!=5) 
    {
        printf("Error: incompatible num of arguments\n");
        exit(-1);
    }
    num_of_threads = atoi(argv[2]); //num threads that are created according to command
    num_of_files = atoi(argv[3]);
    log_handler = atoi(argv[4]);
    //int *result;
    file_array=create_num_counter_file(num_of_files);
    initialize_dispatcher(); //creates required number of threads
    dispatcher_work(read_file);

}
