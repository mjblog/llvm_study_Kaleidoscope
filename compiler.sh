CLANG=/home/majiang/hd/opensource/llvmbuild/install/usr/local/bin/clang
TOY_COMPILER=./toy_compiler

${TOY_COMPILER} $1
#${CLANG} $1.ll -c -o $1.o 添加直接生成的能力后不再需要
${CLANG}++ $1.o  -o $1.out -L ./ -lcore_support
