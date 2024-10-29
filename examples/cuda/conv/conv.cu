/*
 Copyright (c) Loizos Koutsantonis <loizos.koutsantonis@uni.lu>

 Description : CUDA code implementing convolution of an image with a
 LoG kernel.
 Implemented for educational purposes.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the NVIDIA Software License Agreement and CUDA
 Supplement to Software License Agreement.

 University of Luxembourg - HPC
 November 2020
*/

#include <cstdio>
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "CUSPIS.cuh"

#define pi 3.14159265359

/*
Function load_image:
Load BW Image from dat (Ascii) file (Host Function)
dim_x,dim_y: Image Dimensions
fname: fielename (char)
img: float vector containing the image pixels
*/
void load_image(char *fname, int dim_x, int dim_y, float *img) {
  FILE *fp;
  fp = fopen(fname, "r");

  int cnt = 0;
  for (int i = 0; i < dim_y; i++) {
    for (int j = 0; j < dim_x; j++) {
      fscanf(fp, "%f ", &img[i * dim_x + j]);
      cnt++;
    }

    while (fgetc(fp) != '\n') {
      continue;
    }
  }

  fclose(fp);
}

/*
Function save_image:
Save BW Image to dat (Ascii) file (Host Function)
dim_x,dim_y: Image Dimensions
fname: fielename (char)
img: float vector containing the image pixels
*/
void save_image(char *fname, int dim_x, int dim_y, float *img) {
  FILE *fp;

  fp = fopen(fname, "w");

  for (int i = 0; i < dim_y; i++) {
    for (int j = 0; j < dim_x; j++)
      fprintf(fp, "%10.3f ", img[i * dim_x + j]);
    fprintf(fp, "\n");
  }

  fclose(fp);
}

/*
Function calculate_kernel:
Calculate filter coefficients of LoG filter
and save them to a vector (Host Function)
kernel_size: Length of filter window in pixels (same for x and y)
sigma: sigma of the Gaussian kernel (float) given in pixels
kernel: float vector hosting the kernel coefficients
*/
void calculate_kernel(int kernel_size, float sigma, float *kernel) {

  int Nk2 = kernel_size * kernel_size;
  float x, y, center;

  center = (kernel_size - 1) / 2.0;

  for (int i = 0; i < Nk2; i++) {
    x = (float)(i % kernel_size) - center;
    y = (float)(i / kernel_size) - center;
    kernel[i] = -(1.0 / pi * pow(sigma, 4)) *
                (1.0 - 0.5 * (x * x + y * y) / (sigma * sigma)) *
                exp(-0.5 * (x * x + y * y) / (sigma * sigma));
  }
}

/*
Function conv_img_cpu:
Convolve image with the specified kernel  (Host Function)
img: float vector containing the original image pixels
kernel: float vector hosting the kernel coefficients
imgf: float vector containing the result of the convolution
dim_x,dim_y: Original Image Dimensions
kernel_size: Length of filter window in pixels (same for x and y)
*/
void conv_img_cpu(float *img, float *kernel, float *imgf, int dim_x, int dim_y,
                  int kernel_size) {

  float sum = 0;
  int center = (kernel_size - 1) / 2;
  ;
  int ii, jj;

  for (int i = 0; i < (dim_y - 0); i++){
    for (int j = 0; j < (dim_x - 0); j++) {
      sum = 0;
      for (int ki = 0; ki < kernel_size; ki++)
        for (int kj = 0; kj < kernel_size; kj++) {
          ii = kj + j - center;
          jj = ki + i - center;
          if ((i % dim_x) - center >= 0 && (i % dim_x) + center < dim_x &&
          (j % dim_y) - center >= 0 && (j % dim_y) + center < dim_y) {
            sum += img[jj * dim_x + ii] * kernel[ki * kernel_size + kj];
          }
          //sum += img[jj * dim_x + ii] * kernel[ki * kernel_size + kj];
        }
      imgf[i * dim_x + j] = sum;
    }
  }
}

/*
Function conv_img_cpu:
Convolve image with the specified kernel  (Device Function)
img: float vector containing the original image pixels
kernel: float vector hosting the kernel coefficients
imgf: float vector containing the result of the convolution
dim_x,dim_y: Original Image Dimensions
kernel_size: Length of filter window in pixels (same for x and y)
*/
__global__ void conv_img_gpu(float *img, float *kernel, float *imgf, int dim_x,
                             int dim_y, int kernel_size) {
  // each block is assigned to a row of an image, iy index of y value
  int iy = blockIdx.x + dim_y * (int)(threadIdx.x / dim_x);

  // each thread is assigned to a pixel of a row, ix index of x value
  int ix = threadIdx.x % dim_x;

  // idx global index (all blocks) of the image pixel
  int idx = iy * dim_x + ix;

  // center of kernel in both dimensions
  int center = (kernel_size - 1) / 2;

  // Auxiliary variables
  int ii, jj;
  float sum = 0.0;

  /*
  Convlution of image with the kernel
  Each thread computes the resulting pixel value
  from the convolution of the original image with the kernel;
  number of computations per thread = size_kernel^2
  The result is stored to imgf
  */

  for (int ki = 0; ki < kernel_size; ki++)
    for (int kj = 0; kj < kernel_size; kj++) {
      ii = kj + ix - center;
      jj = ki + iy - center;
      if ((ix % dim_x) - center >= 0 && (ix % dim_x) + center < dim_x &&
          (iy % dim_y) - center >= 0 && (iy % dim_y) + center < dim_y) {
        sum += img[jj * dim_x + ii] * kernel[ki * kernel_size + kj];
      }
    }

  imgf[idx] = sum;
}

void calculate_mean_std_dev(float arr[], int n) {
  float sum = 0.0;
  float variance = 0.0;
  float mean, std_dev;

  // Calculate the sum of elements
  for (int i = 0; i < n; i++) {
    sum += arr[i];
  }

  // Calculate the mean
  mean = sum / n;

  // Calculate the variance
  for (int i = 0; i < n; i++) {
    variance += pow(arr[i] - mean, 2);
  }

  // Calculate the standard deviation
  std_dev = sqrt(variance / n);

  printf("Mean: %f\nStd.Dev: %f\n", mean, std_dev);
}

void printDeviceProps() {
  cudaError_t error = cudaFree(0); // Initialize CUDA
  if (error != cudaSuccess) {
    fprintf(stderr, "Could not initialize CUDA: %s\n", cudaGetErrorName(error));
    return;
  }

  cudaDeviceProp deviceProp;
  error = cudaGetDeviceProperties(&deviceProp, 0); // Get properties of device 0
  if (error != cudaSuccess) {
    fprintf(stderr, "Could not get device properties: %s\n",
            cudaGetErrorName(error));
    return;
  }
  int maxGridDimX = deviceProp.maxGridSize[0];
  int maxGridDimY = deviceProp.maxGridSize[1];
  int maxGridDimZ = deviceProp.maxGridSize[2];
  int maxSurface1D = deviceProp.maxSurface1D;
  int maxThreadsPerBlock = deviceProp.maxThreadsPerBlock;
  int maxThreadsPerSM = deviceProp.maxBlocksPerMultiProcessor;
  int numSMs = deviceProp.multiProcessorCount;

  printf("Maximum grid dimensions: %d x %d x %d\n", maxGridDimX, maxGridDimY,
         maxGridDimZ);
  printf("Maximum surface 1D: %d\n", maxSurface1D);
  printf("Maximum threads per block: %d\n", maxThreadsPerBlock);
  printf("Maximum blocks per SM: %d\n", maxThreadsPerSM);
  printf("Number of SMs: %d\n", numSMs);
  printf("\n\n");
}

int main_gpu(int argc, char *argv[]) {
  float results[1000];
  CUSPIS::cuspisRedundancyPolicy policies[3] = {CUSPIS::cuspisRedundantThreads,
                                                CUSPIS::cuspisRedundantBlocks,
                                                CUSPIS::cuspisRedundantKernel};

  for (auto policy : policies) {

    std::string fname = "input.dat";

    for (int j = 0; j < 100; j++) {
      std::cout << "run " << j << "\r" << std::flush;
      cudaEvent_t start, stop;
      cudaEventCreate(&start);
      cudaEventCreate(&stop);
      float milliseconds = 0;
      int dim_x, dim_y;
      int kernel_size;
      float sigma;
      char finput[256], foutput[256];
      int Nblocks, Nthreads;

      sprintf(finput, fname.c_str());
      sprintf(foutput, "out.dat");

      dim_x = 64;
      dim_y = 64;

      kernel_size = 5;
      sigma = 0.8;

      /* Allocate CPU memory
          Vector Representation of Images and Kernel
          (Original Image, Kernel, Convoluted Image) */
      float *img, *imgf, *kernel;

      img = (float *)malloc(dim_x * dim_y * sizeof(float));
      imgf = (float *)malloc(dim_x * dim_y * sizeof(float));
      kernel = (float *)malloc(kernel_size * kernel_size * sizeof(float));

      /* Allocate GPU memory
          Vector Representation of Images and Kernel
          (Original Image, Kernel, Convoluted Image) */

      float *d_img, *d_imgf, *d_kernel;

      CUSPIS::cuspisMalloc(&d_img, dim_x * dim_y * sizeof(float));
      CUSPIS::cuspisMalloc(&d_imgf, dim_x * dim_y * sizeof(float));
      CUSPIS::cuspisMalloc(&d_kernel,
                            kernel_size * kernel_size * sizeof(float));

      load_image(finput, dim_x, dim_y, img);
      calculate_kernel(kernel_size, sigma, kernel);


      cudaEventRecord(start);
      CUSPIS::cuspisMemcpyToDevice(d_img, img, dim_x * dim_y * sizeof(float));
      CUSPIS::cuspisMemcpyToDevice(d_kernel, kernel,
                                    kernel_size * kernel_size * sizeof(float));

      Nblocks = dim_y;
      Nthreads = dim_x;

      CUSPIS::Kernel<float *, float *, float *, int, int, int> k(
          Nblocks, Nthreads, conv_img_gpu, policy);

      // conv_img_cpu(img, kernel, imgf, dim_x, dim_y, kernel_size);
      for (int i = 0; i < 1; i++) {
        k.launch(d_img, d_kernel, d_imgf, dim_x, dim_y, kernel_size);
      }
      cudaDeviceSynchronize();

      CUSPIS::cuspisMemcpyToHost(imgf, d_imgf, dim_x * dim_y * sizeof(float));

      cudaEventRecord(stop);
      cudaEventSynchronize(stop);
      cudaEventElapsedTime(&milliseconds, start, stop);

      save_image(foutput, dim_x, dim_y, imgf);

      free(img);
      free(imgf);
      free(kernel);

      CUSPIS::cuspisFree(&d_img);
      CUSPIS::cuspisFree(&d_imgf);
      CUSPIS::cuspisFree(&d_kernel);

      cudaDeviceReset();

      results[j] = milliseconds;
    }
    std::cout << "Policy " << policy << "\n";
    std::cout << "Input file " << fname << "\n";
    calculate_mean_std_dev(results, 100);
    std::cout << "\n";
  }
  return 0;
}


int main_cpu(int argc, char *argv[]) {
  float results[1000];

  std::string fname = "input.dat";

  for (int j = 0; j < 100; j++) {
    std::cout << "run " << j << "\r" << std::flush;
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    float milliseconds = 0;
    int dim_x, dim_y;
    int kernel_size;
    float sigma;
    char finput[256], foutput[256];

    sprintf(finput, fname.c_str());
    sprintf(foutput, "out.dat");

    dim_x = 64;
    dim_y = 64;

    kernel_size = 5;
    sigma = 0.8;

    /* Allocate CPU memory
        Vector Representation of Images and Kernel
        (Original Image, Kernel, Convoluted Image) */
    float *img, *imgf, *kernel;

    img = (float *)malloc(dim_x * dim_y * sizeof(float));
    imgf = (float *)malloc(dim_x * dim_y * sizeof(float));
    kernel = (float *)malloc(kernel_size * kernel_size * sizeof(float));

    load_image(finput, dim_x, dim_y, img);
    std::cout<< ("ciao\n");
    calculate_kernel(kernel_size, sigma, kernel);


    cudaEventRecord(start);

    conv_img_cpu(img, kernel, imgf, dim_x, dim_y, kernel_size);

    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&milliseconds, start, stop);

    save_image(foutput, dim_x, dim_y, imgf);

    free(img);
    free(imgf);
    free(kernel);

    results[j] = milliseconds;
  }
  std::cout << "Policy serial\n";
  std::cout << "Input file " << fname << "\n";
  calculate_mean_std_dev(results, 100);
  std::cout << "\n";
  
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cout << "Please use two arguments (./out [cpu/gpu])\n";
    return 0;
  }
  if (strcmp(argv[1], "cpu") == 0)
    main_cpu(argc, argv);
  else if(strcmp(argv[1], "gpu") == 0)
    main_gpu(argc, argv);
  else 
    std::cout << "Please specify cpu or gpu\n";
}