#include <stdio.h>
#include <stdlib.h>

extern int bar(int*);

int main(int argc, char **argv){
    int *x = calloc(argc, sizeof(int));

    for (int i = 0; i < argc; i++){
      x[i] = 3*i;
    }

    bar(x);
    int sum = 0;
    for (int i = 0; i < argc; i++){
      sum += x[i];
    }
    
    return sum;
}