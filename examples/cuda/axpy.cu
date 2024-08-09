#if defined(__CUDACC__) || defined(__CUDA_ARCH__) || defined(__CUDA_LIBDEVICE__)
#define __noinline__  __attribute__((noinline))
#endif /* __CUDACC__  || __CUDA_ARCH__ || __CUDA_LIBDEVICE__ */

#include "CUSPIS.cuh"
#include <iostream>

__global__ void axpy(float a, float* x, float* y) {
  y[threadIdx.x] = a * x[threadIdx.x];
}

int run_axpy(CUSPIS::cuspisRedundancyPolicy policy, float *host_x, int N) {
  volatile int blocks = 1;

  float a = 2.0f;
  float host_y[N];

  // Copy input data to device.
  float* device_x;
  float* device_y;
  CUSPIS::cuspisMalloc(&device_x, N * sizeof(float));
  CUSPIS::cuspisMalloc(&device_y, N * sizeof(float));
  CUSPIS::cuspisMemcpyToDevice(device_x, host_x, N * sizeof(float));

  // Launch the kernel.
  CUSPIS::Kernel<float, float*, float*> k(1, N, axpy, policy);
  k.launch(a, device_x, device_y);

  // Copy output data to host.
  cudaDeviceSynchronize();
  CUSPIS::cuspisMemcpyToHost(host_y, device_y, N * sizeof(float));

  CUSPIS::cuspisFree(&device_x);
  CUSPIS::cuspisFree(&device_y);

  return 0;
}

int main(int argc, char* argv[]) {
    int SIZES = 1024;

    if (argc > 1) {
        SIZES = atoi(argv[2]);
    }

    float avg = 0.0;
    FILE *fp_blocks = fopen("axpy_b.txt", "w");
    FILE *fp_thread = fopen("axpy_t.txt", "w");
    FILE *fp_kernel = fopen("axpy_k.txt", "w");

    float *host_d = (float*) malloc(sizeof(float) * SIZES);

    for (int i=0; i<SIZES; i++) {
        host_d[i] = i;
    }

    // warm-up
    run_axpy(CUSPIS::cuspisRedundantBlocks, host_d, SIZES);

    for (int i=0; i<SIZES; i++) {
        float time;
        cudaEvent_t start, stop;

        cudaEventCreate(&start);
        cudaEventCreate(&stop);
        cudaEventRecord(start, 0);

        run_axpy(CUSPIS::cuspisRedundantBlocks, host_d, i);

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
    run_axpy(CUSPIS::cuspisRedundantThreads, host_d, SIZES);

    for (int i=0; i<SIZES; i++) {
        float time;
        cudaEvent_t start, stop;

        cudaEventCreate(&start);
        cudaEventCreate(&stop);
        cudaEventRecord(start, 0);

        run_axpy(CUSPIS::cuspisRedundantThreads, host_d, i);

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
    run_axpy(CUSPIS::cuspisRedundantKernel, host_d, SIZES);

    for (int i=0; i<SIZES; i++) {
        float time;
        cudaEvent_t start, stop;

        cudaEventCreate(&start);
        cudaEventCreate(&stop);
        cudaEventRecord(start, 0);

        run_axpy(CUSPIS::cuspisRedundantKernel, host_d, i);

        cudaEventRecord(stop, 0);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&time, start, stop);

        fprintf(fp_kernel, "%3.5f\n", time);
        avg = avg + (time - avg)/(i+1);
    }
    fclose(fp_kernel);
    printf("avg (redundant kernels):\t%3.5f\n", avg);


}
