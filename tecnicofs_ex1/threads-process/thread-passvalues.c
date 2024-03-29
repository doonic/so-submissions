#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

// TODO 
// Passing arguments to threads


// problem we're aiming to solve:
//create 10 thread, each taking an unique prime from the primes array
// and print it on the screen  


int primes[10] = {2,3,5,7,11,13,17,19,23,29};


void* routine(void* arg) {
    
    sleep(1);
    int index = *(int*)arg;
    printf("%d ",primes[index]);
    free(arg);
}

int main(int argc, char* argv[]) {

    pthread_t th[10];
    int i;
    for(i = 0; i < 10; i++) {

        int* a = malloc(sizeof(int));
        *a = i;
        if(pthread_create(&th[i], NULL, &routine, a) != 0) {

            perror("Failed to create thread");
        }
    }

    for(i = 0; i < 10; i++) {

        if(pthread_join(th[i], NULL) != 0){
            perror("Failed to join thread");
        }
    }

    return 0;
}