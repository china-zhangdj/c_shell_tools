// mem_bandwidth.c
// 简单内存带宽测试工具（约100MB工作集）
// 编译: gcc -O3 -march=native mem_bandwidth.c -o mem_bandwidth -lrt
// 运行: ./mem_bandwidth

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define SIZE_MB         100                  // 测试内存大小（MiB）
#define BYTES_TOTAL     ((size_t)SIZE_MB * 1024 * 1024)  // 104857600 bytes
#define ITERATIONS      20                   // 每项测试重复次数（减少测量误差）

// 高精度计时
static inline double get_time_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// 顺序读测试
double test_read(const char *buf, size_t size)
{
    double start = get_time_sec();
    volatile uint64_t sink = 0;  // 防止编译器优化掉读操作
    for (int i = 0; i < ITERATIONS; i++) {
        for (size_t j = 0; j < size; j += 64) {  // 64字节对齐，模拟缓存行
            sink += ((uint64_t*)buf)[j / 8];
        }
    }
    double end = get_time_sec();
    (void)sink;  // 避免未使用警告
    return (double)ITERATIONS * size / (end - start);
}

// 顺序写测试
double test_write(char *buf, size_t size)
{
    double start = get_time_sec();
    for (int i = 0; i < ITERATIONS; i++) {
        memset(buf, (char)i, size);  // 用不同值防止零页优化
    }
    double end = get_time_sec();
    return (double)ITERATIONS * size / (end - start);
}

// 顺序拷贝测试（memcpy）
double test_copy(const char *src, char *dst, size_t size)
{
    double start = get_time_sec();
    for (int i = 0; i < ITERATIONS; i++) {
        memcpy(dst, src, size);
    }
    double end = get_time_sec();
    return (double)ITERATIONS * size / (end - start);
}

int main(void)
{
    printf("内存带宽测试（工作集：%d MiB）\n", SIZE_MB);
    printf("正在分配内存...\n");

    char *buf_a = aligned_alloc(4096, BYTES_TOTAL);
    char *buf_b = aligned_alloc(4096, BYTES_TOTAL);

    if (!buf_a || !buf_b) {
        perror("内存分配失败");
        return 1;
    }

    // 预热 + 初始化（避免首次访问页面开销）
    memset(buf_a, 0x55, BYTES_TOTAL);
    memset(buf_b, 0xaa, BYTES_TOTAL);

    printf("开始测试（每个项目重复 %d 次）...\n\n", ITERATIONS);

    double read_bw  = test_read(buf_a, BYTES_TOTAL);
    double write_bw = test_write(buf_b, BYTES_TOTAL);
    double copy_bw  = test_copy(buf_a, buf_b, BYTES_TOTAL);

    printf("顺序读带宽   : %.2f MB/s\n", read_bw / (1024*1024));
    printf("顺序写带宽   : %.2f MB/s\n", write_bw / (1024*1024));
    printf("顺序拷贝带宽 : %.2f MB/s  (约等于读+写)\n", copy_bw / (1024*1024));

    free(buf_a);
    free(buf_b);
    return 0;
}
