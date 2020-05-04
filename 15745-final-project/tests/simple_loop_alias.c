#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv){
    int *x = calloc(4, sizeof(int));
    // int x[3];
    for (int i = 0; i < 3; i++){
      x[i] = 3*i;
    }

    // struct { int x; } A;
    // struct {int y; } B;
    // struct { struct A; struct B; } C;

    // int x = C.A.x
    // int y = C.B.y

    // printf("%d %d\n", temp.b_temp.c_temp.d);
    //int k = 0x50;
    //int j = 2;

    //int *x = &k;
    //int *y = x;
    //int *z = &j; 
    //printf("x = %p, y = %p, z = %p\n", x, y, z);
    //return k + j;
    return x[0] + x[1] + x[2];
}

// struct c {
//     int d;
// };

// struct b {
//     short a;
//     struct c c_temp;
// };

// struct hello {
//     int a;
//     struct b b_temp;
// };


// struct A { int x; };
// struct B { int y; };
// struct C { struct A a; struct B b; };
// extern int bar(int*);
// extern int baz(struct B*);

// int foo(struct C* c)
// {
//   bar(&c->a.x);
//   baz(&c->b);
//   return c->a.x - c->b.y;
// }

// int foo(struct C* c)
// {
//   c = (struct C*) malloc(sizeof(struct C));
//   bar(&c->a.x);
//   baz(&c->b);
//   return c->a.x - c->b.y;
// }

// int hello()
// {
//   struct C c;
//   foo(&c);
//   return c.a.x - c.b.y;
// }


