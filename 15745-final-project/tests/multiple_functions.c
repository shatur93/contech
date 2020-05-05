#include <stdio.h>
#include <stdlib.h>

extern int bar(int*);

int *helper(){
    int *x = calloc(4, sizeof(int));

    for (int i = 0; i < 3; i++){
      x[i] = 3*i;
    }

    return x;
}

int main(int argc, char **argv){
    int *x = helper();
    bar(x);
    return x[0] + x[1] + x[2];
}