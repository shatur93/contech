make $2
clang -Xclang -O3 -emit-llvm -c $1.c -o $1.bc
opt -load ./Alias${2^}/AA${2^}Pass.so -my-aa-$2-pass $1.bc -o out -my-print-partial-aliases -my-print-must-aliases
