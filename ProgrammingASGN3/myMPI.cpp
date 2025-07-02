#include <iostream>      // For basic I/O  
#include <cstdlib>       // For atoi, exit  
#include <cstring>       // For memset, memcpy  
#include <unistd.h>      // For fork, exec  
#include <sys/mman.h>    // For shared memory (mmap)  
#include <sys/wait.h>    // For waitpid  
#include <fcntl.h>       // For shared memory (shm_open)  
#include <semaphore.h>   // For synchronization  
#include <stdio.h>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <program> <par_count>\n";
        return 1;
    }

    int par_count = atoi(argv[2]);

    // Allocate arguments once outside the loop
    char* args[4];
    for (int j = 0; j < 3; j++) 
        args[j] = (char*)malloc(100);

    // move right into left, like ASM
    strcpy(args[0], argv[1]);    // Program name (./calc)
    sprintf(args[2], "%d", par_count); // Total number of processes
    args[3] = NULL; // Null-terminate argument list

    for (int i = 0; i < par_count; i++) {
        sprintf(args[1], "%d", i); // Process ID (par_id), update for each child

        if (fork() == 0) { // Child process
            execv(args[0], args); // ./calc takes over the child process, therefore this runs ./calc
            perror("execv failed"); // Print error if execv fails
            exit(1); // Prevent child from continuing if execv fails
        }
    }

    // Free memory in parent process after making all children
    for (int j = 0; j < 3; j++) 
        free(args[j]);

    // Wait for all child processes to finish
    for (int i = 0; i < par_count; i++) 
        wait(0);

    return 0;
}
