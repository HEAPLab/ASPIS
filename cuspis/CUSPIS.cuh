#ifndef CUSPIS_H
#define CUSPIS_H

#include <cstdio>
#include <iostream>
#include <driver_types.h>
#include <cuda_runtime_api.h>
#include <list>
#include <vector>

namespace CUSPIS {

    constexpr int NUM_REPLICAS = 2;
    static_assert(NUM_REPLICAS <= 3 && NUM_REPLICAS >=1, "The number of replicas selected is not supported!");

    enum cuspisRedundancyPolicy {
        cuspisRedundantThreads, // use NUM_REPLICAS times the number of threads
        cuspisRedundantBlocks,  // use NUM_REPLICAS times the number of blocks
        cuspisRedundantKernel   // execute the kernel NUM_REPLICAS times
    };

    class MemAllocation {
        private: 
            size_t size;
            void *data;

        public: 
            MemAllocation(void *data, size_t size) :
                data(data), size(size) {}

            /**
             * Returns the size of the data stored in `ptr` if the pointer is in this memory allocation object, 0 otherwise.
             */
            size_t isInMemAllocation(void *ptr) {
                if (data <= ptr && ptr < (void*)(((char*)(data)) + size)) {
                    return size;
                }
                else {
                    return 0;
                }
            }
    };

    std::list<MemAllocation> allocations;

    template <typename ... Types>
    class Kernel {
        private:
            int numBlocks; 
            int numThreads;
            void (*kernelFunc)(Types ...args);
            cuspisRedundancyPolicy policy;

            void modify () {
            }

            template <typename T>
            void modify (T **x) {
                for (auto Elem : allocations) {
                    auto size = Elem.isInMemAllocation(*x);
                    if (size) {
                        *x = (T*)(((char*)*x) + size);
                        break;
                    }
                }
            }

            template <typename T, typename... Args>
            void modify (T *x, Args... args) {
                modify(args...);
            }

            template <typename T, typename... Args>
            void modify (T **x, Args... args) {
                for (auto Elem : allocations) {
                    auto size = Elem.isInMemAllocation(*x);
                    if (size) {
                        *x = (T*)(((char*)*x) + size);
                        break;
                    }
                }

                modify(args...);
            }

        public:
            Kernel(int numBlocks, int numThreads, void (*kernel)(Types ...args), cuspisRedundancyPolicy policy) :
                numBlocks(numBlocks), numThreads(numThreads), kernelFunc(kernel), policy(policy) {}

            void launch(Types ...args) {
                if constexpr (NUM_REPLICAS == 1) {
                    kernelFunc<<<numBlocks, numThreads>>>(args...);
                    return;
                }

                else if (policy == cuspisRedundantThreads) {
                    kernelFunc<<<numBlocks, numThreads*NUM_REPLICAS>>>(args...);
                }
                else if (policy == cuspisRedundantBlocks) {
                    kernelFunc<<<numBlocks*NUM_REPLICAS, numThreads>>>(args...);
                }
                else if (policy == cuspisRedundantKernel) {
                    kernelFunc<<<numBlocks, numThreads>>>(args...);
                    modify(&args...);
                    kernelFunc<<<numBlocks, numThreads>>>(args...);
                    if constexpr (NUM_REPLICAS == 3) {
                        modify(&args...);
                        kernelFunc<<<numBlocks, numThreads>>>(args...);
                    }
                }
            }
    };

    cudaError_t DataCorruption_Handler(void* dst, int index) {
        fprintf(stderr, "Data corruption detected, aborting.\n");
        exit(-1);
    }

    /** 
     * Wrapper of the cudaMalloc() function. 
     * 
     * Allocates `NUM_REPLICAS` times the size required by the malloc 
     */
    template <class T> cudaError_t __attribute__((annotate("cuspis"))) cuspisMalloc(T **devPtr, size_t size) {
        auto res = cudaMalloc(devPtr, size * NUM_REPLICAS);
        allocations.push_back(MemAllocation(*devPtr, size));
        return res;
    }

    /** 
     * Wrapper of the cudaFree() function. 
     */
    template <class T> cudaError_t __attribute__((annotate("cuspis"))) cuspisFree(T **devPtr) {  
        // TODO fix this garbage below -----------v
        auto i=allocations.begin();
        for (auto Elem : allocations) {
            auto size = Elem.isInMemAllocation(*devPtr);
            if (size) {
                allocations.erase(i);
                break;
            }
            i++;
        }
        return cudaFree(devPtr);
    }

    /**
     * Wrapper of the cudaMemcpy function, from host to device.
     * 
     * Copies two times the data from the host to the device.
     */
    inline cudaError_t __attribute__((annotate("cuspis"))) cuspisMemcpyToDevice(void *dst, const void *src, size_t count) {
        for (int i=0; i<NUM_REPLICAS; i++) {
            cudaMemcpy((char*)dst + (count*i), src, count, cudaMemcpyHostToDevice);
        }
        return cudaSuccess;
    }

    /**
     * Wrapper of the cudaMemcpy function, from device to host.
     * 
     * Copies the data from the device to the host.
     */
    inline cudaError_t __attribute__((annotate("cuspis"))) cuspisMemcpyToHost(void *dst, const void *src, size_t count) {
        if constexpr (NUM_REPLICAS == 1)
            return cudaMemcpy(dst, src, count, cudaMemcpyDeviceToHost);

        std::vector<char> dst_cpy(count);

        auto ret = cudaMemcpy(dst, src, count, cudaMemcpyDeviceToHost);
        if (ret != cudaSuccess) {
            return ret;
        }

        ret = cudaMemcpy(dst_cpy.data(), (char*)src + count, count, cudaMemcpyDeviceToHost);
        if (ret != cudaSuccess) {
            return ret;
        }

        for (int i=0; i<count; i++) { // TODO can we use a memcmp
            if (((char*)(dst))[i] != (dst_cpy)[i]) {
                if constexpr (NUM_REPLICAS == 2)
                    return DataCorruption_Handler(dst, i);
                else if constexpr (NUM_REPLICAS == 3) // fai il check anche sulla seconda (opzionale)
                    return cudaMemcpy(dst, ((char*)(src)) + 2*count, count, cudaMemcpyDeviceToHost);
            }
        }

        return ret;

    }
}

#endif