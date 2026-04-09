#!/bin/bash

# 该将自己写的编译器syc编译测试用例，生成的汇编文件，将其和运行库链接起来，生成可执行文件
# 然后利用qemu运行该可执行文件

if [ ! -f "./syc" ]; then
    echo "错误：当前目录下未找到编译器 syc！"
    echo "请先编译生成 syc 后再运行测试脚本。"
    exit 1
fi
#######################################################################################################################
exec > func_test.txt 2>&1

total_start=$(date +%s%3N)

for f in functional/*.sy; do
    input="${f%.sy}.in"
    echo "========== $f =========="

    # clang --target=riscv64-unknown-linux-gnu -march=rv64gc -mabi=lp64d -Wno-implicit-function-declaration -S -O0 -o output.s -x c "$f" > /dev/null 2>&1
    # ./syc -O2 "$f" > /dev/null 2>&1
    ./syc -O2 "$f"

    if [ $? -ne 0 ]; then
        continue  # 失败，跳过后续，直接下一个
    fi

    # clang --target=riscv64-linux-gnu -march=rv64gc -mabi=lp64d -static -o a.out sylib.s output.s
    clang --target=riscv64-linux-gnu -march=rv64gc -mabi=lp64d -static -o a.out output.s lib/libsysy_riscv.a

    if [ -f "$input" ]; then
        qemu-riscv64 ./a.out < "$input"
    else
        qemu-riscv64 ./a.out
    fi

    echo ""
    echo ""
done

rm output.s a.out

total_end=$(date +%s%3N)
echo "========== 总耗时: $((total_end - total_start)) ms =========="
#######################################################################################################################
exec > perf_test.txt 2>&1

total_start=$(date +%s%3N)

for f in performance/*.sy; do
    input="${f%.sy}.in"
    echo "========== $f =========="

    # clang --target=riscv64-unknown-linux-gnu -march=rv64gc -mabi=lp64d -Wno-implicit-function-declaration -S -O0 -o output.s -x c "$f" > /dev/null 2>&1
    # ./syc -O2 "$f" > /dev/null 2>&1
    ./syc -O2 "$f"

    if [ $? -ne 0 ]; then
        continue  # 失败，跳过后续，直接下一个
    fi

    # clang --target=riscv64-linux-gnu -march=rv64gc -mabi=lp64d -static -o a.out sylib.s output.s
    clang --target=riscv64-linux-gnu -march=rv64gc -mabi=lp64d -static -o a.out output.s lib/libsysy_riscv.a

    if [ -f "$input" ]; then
        qemu-riscv64 ./a.out < "$input"
    else
        qemu-riscv64 ./a.out
    fi

    echo ""
    echo ""
done

rm output.s a.out

total_end=$(date +%s%3N)
echo "========== 总耗时: $((total_end - total_start)) ms =========="

