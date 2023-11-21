#include "ft_utils.h"

void DataCorruption_Handler() {
    while(1);
}

void SigMismatch_Handler() {
    while(1);
}

__attribute__((annotate("to_duplicate"))) void memcpy_to_duplicate(void *__restrict__ __dest, const void *__restrict__ __src, size_t __n) {
    memcpy(__dest, __src, __n);
}


__attribute__((annotate("to_duplicate"))) void memset_to_duplicate(void *__restrict__ __dest, const int __src, size_t __n) {
    memset(__dest, __src, __n);
}