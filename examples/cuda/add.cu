#include "CUSPIS.cuh"
#include <cstdio>
#include <cuda_device_runtime_api.h>
#include <cuda_runtime_api.h>
#include <driver_types.h>

__global__ void add(int* a, int* b, int* c, int N) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    if (i<N*CUSPIS::NUM_REPLICAS) {
    //if (i<N) {
        c[i] = a[i] + b[i];
    }
}

int perform_add(CUSPIS::cuspisRedundancyPolicy policy) {
    const int N = 1024;

    int *h_a, *h_b, *h_c;
    int *d_a, *d_b, *d_c;

    // host memory
    h_a = (int*) malloc(N * sizeof(int));
    h_b = (int*) malloc(N * sizeof(int));
    h_c = (int*) malloc(N * sizeof(int));

    // init data
    for (int i=0; i<N; i++) {
        h_a[i] = i;
        h_b[i] = i*2;
    }

    // device memory
    CUSPIS::cuspisMalloc(&d_a, N * sizeof(int));
    CUSPIS::cuspisMalloc(&d_b, N * sizeof(int));
    CUSPIS::cuspisMalloc(&d_c, N * sizeof(int));

    // copy to device
    CUSPIS::cuspisMemcpyToDevice(d_a, h_a, N * sizeof(int));
    CUSPIS::cuspisMemcpyToDevice(d_b, h_b, N * sizeof(int));

    // create and launch the kernel
    CUSPIS::Kernel<int*, int*, int*, int> k((N+255)/256, 256, add, policy);

    k.launch(d_a, d_b, d_c, N);

    cudaDeviceSynchronize();

    // copy back to host
    CUSPIS::cuspisMemcpyToHost(h_c, d_c, N * sizeof(int));

    CUSPIS::cuspisFree(&d_a);
    CUSPIS::cuspisFree(&d_b);
    CUSPIS::cuspisFree(&d_c);
    free(h_a);
    free(h_b);
    free(h_c);

    return 0;
}


int main(int argc, char* argv[]) {
    int NUM_ITER = 1024;

    if (argc > 1) {
        NUM_ITER = atoi(argv[1]);
    }

    float avg = 0.0;
    FILE *fp_blocks = fopen("add_b.txt", "w");
    FILE *fp_thread = fopen("add_t.txt", "w");
    FILE *fp_kernel = fopen("add_k.txt", "w");

    // warm-up
    perform_add(CUSPIS::cuspisRedundantBlocks);

    for (int i=0; i<NUM_ITER; i++) {
        float time;
        cudaEvent_t start, stop;

        cudaEventCreate(&start);
        cudaEventCreate(&stop);
        cudaEventRecord(start, 0);

        perform_add(CUSPIS::cuspisRedundantBlocks);

        cudaEventRecord(stop, 0);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&time, start, stop);

        fprintf(fp_blocks, "%3.5f\n", time);
        avg = avg + (time - avg)/(i+1);
    }
    fclose(fp_blocks);
    printf("avg (redundant blocks):\t\t%3.5f\n", avg);

    avg = 0.0;
    cudaDeviceReset();

    // warm-up
    perform_add(CUSPIS::cuspisRedundantThreads);

    for (int i=0; i<NUM_ITER; i++) {
        float time;
        cudaEvent_t start, stop;

        cudaEventCreate(&start);
        cudaEventCreate(&stop);
        cudaEventRecord(start, 0);

        perform_add(CUSPIS::cuspisRedundantThreads);

        cudaEventRecord(stop, 0);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&time, start, stop);

        fprintf(fp_thread, "%3.5f\n", time);
        avg = avg + (time - avg)/(i+1);
    }
    fclose(fp_thread);
    printf("avg (redundant threads):\t%3.5f\n", avg);

    avg = 0.0;
    cudaDeviceReset();

    // warm-up
    perform_add(CUSPIS::cuspisRedundantKernel);

    for (int i=0; i<NUM_ITER; i++) {
        float time;
        cudaEvent_t start, stop;

        cudaEventCreate(&start);
        cudaEventCreate(&stop);
        cudaEventRecord(start, 0);

        perform_add(CUSPIS::cuspisRedundantKernel);

        cudaEventRecord(stop, 0);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&time, start, stop);

        fprintf(fp_kernel, "%3.5f\n", time);
        avg = avg + (time - avg)/(i+1);
    }
    fclose(fp_kernel);
    printf("avg (redundant kernels):\t%3.5f\n", avg);


}