#include <stdio.h>
#include <stdlib.h>
__attribute__((annotate("to_duplicate")))
int* createMalloc(){
    int* arr = (int*)malloc(5 * sizeof(int));
    return arr;
}
int main() {
    int num_elements = 5;    
    int* arr = createMalloc();

    if (arr == NULL) {
        printf("Error: memory allocation failed!\n");
        return 1;
    }

    for (int i = 0; i < num_elements; i++) {
        arr[i] = i * 10; 
    }

    printf("Memory successfully allocated at address: %p\n", (void*)arr);
    // set arr = arr + 1

    printf("Reading values from the Heap:\n");
    
    for (int i = 0; i < num_elements; i++) {
        printf("arr[%d] = %d\n", i, arr[i]);
    }

    free(arr);
    printf("Memory freed. Execution finished.\n");

    return 0;
}