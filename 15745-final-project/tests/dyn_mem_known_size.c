#include <stdio.h>
#include <stdlib.h>

extern int bar(int*);

int main(int argc, char **argv){
    int *x = calloc(4, sizeof(int));

    for (int i = 0; i < 3; i++){
      x[i] = 3*i;
    }

    bar(x);
    return x[0] + x[1] + x[2];
}