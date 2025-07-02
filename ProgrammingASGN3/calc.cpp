#include <iostream>      // For basic I/O  
#include <cstdlib>       // For atoi, exit  
#include <cstring>       // For memset, memcpy  
#include <unistd.h>      // For fork, exec  
#include <sys/mman.h>    // For shared memory (mmap)  
#include <sys/wait.h>    // For waitpid  
#include <fcntl.h>       // For shared memory (shm_open)  
#include <semaphore.h>   // For synchronization  
#include <cmath>         // For abs()
#include <stdio.h>

using namespace std;

long long diagonalSum(int* M, int n) {
    long long sum = 0;
    for (int i = 0; i < n; i++) {
        sum += M[i * n + i]; // Access main diagonal elements
    }
    return sum;
}


long double determinant(int* M, int n) {
    double** temp = new double*[n]; // Create a dynamic 2D array
    for (int i = 0; i < n; i++) {
        temp[i] = new double[n];
        for (int j = 0; j < n; j++) {
            temp[i][j] = static_cast<double>(M[i * n + j]); // Convert int* to double
        }
    }

    double det = 1.0;

    for (int i = 0; i < n; i++) {
        int pivot = i;
        for (int j = i + 1; j < n; j++) {
            if (abs(temp[j][i]) > abs(temp[pivot][i])) {
                pivot = j;
            }
        }
        if (pivot != i) {
            swap(temp[i], temp[pivot]); // Swap entire row
            det *= -1;
        }
        if (temp[i][i] == 0) {
            det = 0;
            break;
        }
        det *= temp[i][i];
        for (int j = i + 1; j < n; j++) {
            double factor = temp[j][i] / temp[i][i];
            for (int k = i + 1; k < n; k++) {
                temp[j][k] -= factor * temp[i][k];
            }
        }
    }

    // Free allocated memory
    for (int i = 0; i < n; i++) {
        delete[] temp[i];
    }
    delete[] temp;

    return det;
}


void synch(int par_id, int par_count, int*ready)
{
    
    int synchid =ready[par_count]+1;
    ready[par_id]=synchid;
    int breakout =0;
    while(1)
    {
        
        breakout=1; 
        for(int i=0; i<par_count; i++)
        {
            
            if(ready[i]<synchid)
            {
                breakout =0;
                break;
            }
        }
        if(breakout==1)
        {
            ready[par_count]= synchid;
            break;
        }
    }
}

void matrixMultiply(int *A, int *B, int *C, int size, int program_id, int par_count, int* ready, int start_row, int end_row) {

    // Perform matrix multiplication for assigned rows (size is the numbers rows and colums since it is an nxn matrix)
    for (int i = start_row; i < end_row; i++) { // start at the start row and end at the end row
        for (int j = 0; j < size; j++) {
            C[i * size + j] = 0;
            for (int k = 0; k < size; k++) {
                C[i * size + j] += A[i * size + k] * B[k * size + j];
            }
        }
    }

    synch(program_id, par_count, ready); // Synchronize processes
}


int main(int argc, char* argv[])
{

    int program_id = atoi(argv[1]); // Process ID
    int par_count = atoi(argv[2]);  // Number of processes

    sleep(1);

    int matrixSize = 100; // for 100x100 matrix
    int totalSize = 3 * matrixSize * matrixSize * sizeof(int) + (par_count + 1) * sizeof(int);
    int* sharedMem = NULL;
    int fd, shm_A, shm_B, shm_M, shm_ready;

    if (program_id == 0)  // First process creates shared memory
    {
        shm_A = shm_open("/matrixA", O_RDWR | O_CREAT, 0777);
        ftruncate(shm_A, matrixSize * matrixSize * sizeof(int));

        shm_B = shm_open("/matrixB", O_RDWR | O_CREAT, 0777);
        ftruncate(shm_B, matrixSize * matrixSize * sizeof(int));

        shm_M = shm_open("/matrixM", O_RDWR | O_CREAT, 0777);
        ftruncate(shm_M, matrixSize * matrixSize * sizeof(int));

        shm_ready = shm_open("/ready", O_RDWR | O_CREAT, 0777);
        ftruncate(shm_ready, (par_count + 1) * sizeof(int));
    }
    else  // other processes
    {
        sleep(1); // Allow process 0 to set up memory
        shm_A = shm_open("/matrixA", O_RDWR, 0777);
        shm_B = shm_open("/matrixB", O_RDWR, 0777);
        shm_M = shm_open("/matrixM", O_RDWR, 0777);
        shm_ready = shm_open("/ready", O_RDWR, 0777);
    }

    int* A = (int*)mmap(NULL, matrixSize * matrixSize * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_A, 0);
    int* B = (int*)mmap(NULL, matrixSize * matrixSize * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_B, 0);
    int* M = (int*)mmap(NULL, matrixSize * matrixSize * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_M, 0);
    int* ready = (int*)mmap(NULL, (par_count + 1) * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_ready, 0);

    close(shm_A);
    close(shm_B);
    close(shm_M);
    close(shm_ready);

    if (program_id == 0)  // Initialize matrices
    {
        srand(time(0));
        for (int i = 0; i < matrixSize; i++)
            for (int j = 0; j < matrixSize; j++)
            {
                A[i * matrixSize + j] = rand() % 10;
                B[i * matrixSize + j] = rand() % 10;
            }
        memset(ready, 0, (par_count + 1) * sizeof(int)); // memset to # of par_count since we need to keep track of all the par_id
    }
    timespec time, res;
    // Now we need to find the matrix multiplcation and det, using gather()
    synch(program_id, par_count, ready); // ensure all process have attached to shared mem


    int rows_per_proc = matrixSize / par_count;
    int extra_rows = matrixSize % par_count;  // Handles cases where rows are not evenly divisible

    // Assign extra rows to the first `extra_rows` processes
    int start_row = program_id * rows_per_proc + min(program_id, extra_rows);
    int end_row = start_row + rows_per_proc + (program_id < extra_rows ? 1 : 0);
    cout <<  "starting row is: " <<start_row << endl;
    cout << "end row is: " << end_row << endl;

    if (program_id == 0) // on the first program start keeping track of the time
        {
            clock_gettime(CLOCK_MONOTONIC, &time);
        }

    // Find M by: M = A * B
    matrixMultiply(A, B, M, matrixSize, program_id, par_count, ready, start_row, end_row);
    synch(program_id, par_count, ready);
    // MAKE SURE MATRIX MULTIPLICATION IS WORKING
    // cout << "Matrix A (Result of A * B):" << endl;
    // for (int i = 0; i < matrixSize; i++) {
    //     for (int j = 0; j < matrixSize; j++) {
    //         cout << A[i * matrixSize + j] << " ";
    //     }
    //     cout << endl;
    // }

    // cout << "Matrix B (Result of A * B):" << endl;
    // for (int i = 0; i < matrixSize; i++) {
    //     for (int j = 0; j < matrixSize; j++) {
    //         cout << B[i * matrixSize + j] << " ";
    //     }
    //     cout << endl;
    // }

    // //print out the matrix for small matrices
    // cout << "Matrix M (Result of A * B):" << endl;
    // for (int i = 0; i < matrixSize; i++) {
    //     for (int j = 0; j < matrixSize; j++) {
    //         cout << M[i * matrixSize + j] << " ";
    //     }
    //     cout << endl;
    // }


    // now solve for B using M and A: B = M * A

    matrixMultiply(A, M, B, matrixSize, program_id, par_count, ready, start_row, end_row);

    synch(program_id, par_count, ready);
    // solve for M again: M = A * B
    matrixMultiply(A, B, M, matrixSize, program_id, par_count, ready, start_row, end_row);

    synch(program_id, par_count, ready);
    if (program_id == 0) { // Only process 0 prints the matrix

    //print out the matrix for small matrices
    // cout << "Matrix M (Result of A * B):" << endl;
    // for (int i = 0; i < matrixSize; i++) {
    //     for (int j = 0; j < matrixSize; j++) {
    //         cout << M[i * matrixSize + j] << " ";
    //     }
    //     cout << endl;
    // }

        clock_gettime(CLOCK_MONOTONIC, &res);
        double timeElapsed = (res.tv_sec - time.tv_sec) * 1000.0 + (res.tv_nsec - time.tv_nsec) / 1e6;
        cout << "Time taken with " << par_count << " processes: " << timeElapsed << " milliseconds" << endl;

        long long diagonal_sum = diagonalSum(M, matrixSize);
        cout << "Diagonal sum of M: " << diagonal_sum << endl;
    }

    munmap(A, matrixSize * matrixSize * sizeof(int));
    munmap(B, matrixSize * matrixSize * sizeof(int));
    munmap(M, matrixSize * matrixSize * sizeof(int));
    munmap(ready, (par_count + 1) * sizeof(int));
    
    if (program_id == 0)  // Process 0 waits for all processes, then cleans up
    {
    
        shm_unlink("/matrixA");
        shm_unlink("/matrixB");
        shm_unlink("/matrixM");
        shm_unlink("/ready");
    }

    return 0;
    
}
