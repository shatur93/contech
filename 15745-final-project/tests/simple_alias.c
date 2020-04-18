#include <stdio.h>
#include <stdlib.h>
int main(){
    int *x = calloc(4, sizeof(int));
    //for (int i = 0; i < 4; i++){
    //   x[i] = 3*i;
    //}

    printf("%d, %d, %d\n", x[0], x[1], x[2]);
    //int k = 0x50;
    //int j = 2;

    //int *x = &k;
    //int *y = x;
    //int *z = &j; 
    //printf("x = %p, y = %p, z = %p\n", x, y, z);
    //return k + j;
    return 0;
}
