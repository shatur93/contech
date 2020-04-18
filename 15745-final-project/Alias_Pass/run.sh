make
clang -Xclang -O1 -emit-llvm -c $1.c -o $1.bc
#opt -mem2reg $2.bc -o $2-m2r.bc
opt -load ./AAPass.so -my-aa-pass $1.bc -o out -my-print-must-aliases
