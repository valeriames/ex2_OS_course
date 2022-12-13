
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

    if (num_of_files == 0 && num_of_files == 0) {
        if (log_handler == 0) exit(0);
        else if (log_handler == 1) {
            create_stats_file(0,0,0,0);
            exit(0);
        }
    }
    //int *result;
    file_array=create_num_counter_file(num_of_files);
    initialize_dispatcher(); //creates required number of threads
    dispatcher_work(read_file);

}
