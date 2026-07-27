#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* run this program using the console pauser or add your own getch, system("pause") or input loop */

void fill(int arr[], int size, int max)
{
  int count;
  srand(6);

  for (count = 0; count < size; count++) 
    arr[count] = (rand() % max + 1);  
}

void display(int arr[], int size)
{
  int count;
  printf("\n");
  for (count = 0; count < size; count++) 
    printf ("%4d", arr[count]);
}

void bubbleSort(int arr[], int size)
{
  int x, y;
  int temp;
  for (x = 0; x < size - 1; x++) {
    for (y = 0; y < size - 1; y++)
      if (arr[y] > arr[y + 1]) {
        temp = arr[y];  
        arr[y] = arr[y + 1];
        arr[y + 1] = temp;
    }
  }
}

int search(int arr[], int start, int end, int value)
{ 
  for (int i = 0; i < end; i++){
    if (arr[i] == value){
      return i;
    }
  }
  return -1;
}

int main(void) {
  int numPositions = 21;    
  int max          = 100; 
  int searchVal    = 0; 
  int foundIndex   = -1;

  int numbers[numPositions]; 
  fill(numbers, numPositions, max);
  bubbleSort(numbers, numPositions);
  display(numbers, numPositions);
  
  printf("\nEnter the number to search for (input 6): "); 
  scanf("%d", &searchVal);
  foundIndex = search(numbers, 0, numPositions, searchVal);
  
  if (foundIndex == -1)
    printf("\nThe number %d is not present in the array", searchVal);
  else
    printf("\nThe number %d is present at position %d", searchVal, foundIndex);

  display(numbers, numPositions);

  printf("\n\n");
  system("PAUSE");

  return 0;
}