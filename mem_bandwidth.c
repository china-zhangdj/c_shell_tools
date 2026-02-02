// mem_bandwidth.c
// 简单内存带宽测试工具（约100MB工作集）
// 编译: gcc -O3 mem_bandwidth.c -o mem_bandwidth -lrt -march=armv7-a -mcpu=cortex-a9 -mfpu=vfpv3-d16 -mfloat-abi=hard
// 运行: ./mem_bandwidth

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>

#define SIZE_MB         100
#define BYTES_TOTAL     ((size_t)SIZE_MB * 1024 * 1024)
#define ITERATIONS      20

static inline double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// 优化后的读测试：使用指针步进，减少数据依赖
double test_read(const char *buf, size_t size) {
    double start = get_time_sec();
    volatile uint64_t sink = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        const uint64_t *ptr = (const uint64_t *)buf;
        size_t n = size / sizeof(uint64_t);
        // 展开循环，减少分支开销
        for (size_t j = 0; j < n; j += 8) {
            sink += ptr[j];
            sink += ptr[j+1];
            sink += ptr[j+2];
            sink += ptr[j+3];
            sink += ptr[j+4];
            sink += ptr[j+5];
            sink += ptr[j+6];
            sink += ptr[j+7];
        }
    }
    double end = get_time_sec();
    (void)sink;
    return (double)ITERATIONS * size / (end - start);
}

double test_write(char *buf, size_t size) {
    double start = get_time_sec();
    for (int i = 0; i < ITERATIONS; i++) {
        // 使用汇编层优化的 memset
        memset(buf, (int)i, size);
    }
    double end = get_time_sec();
    return (double)ITERATIONS * size / (end - start);
}

double test_copy(const char *src, char *dst, size_t size) {
    double start = get_time_sec();
    for (int i = 0; i < ITERATIONS; i++) {
        const uint64_t *s = (const uint64_t *)src;
        uint64_t *d = (uint64_t *)dst;
        size_t n = size / 64; // 每次处理 64 字节
        
        while(n--) {
            // 使用内联汇编进行 NEON 拷贝
            __asm__ __volatile__ (
                "pld [%0, #128]      \n\t" // 预取前方数据
                "vld1.8 {d0-d3}, [%0]! \n\t" // 读取 32 字节
                "vld1.8 {d4-d7}, [%0]! \n\t" // 读取下 32 字节
                "vst1.8 {d0-d3}, [%1]! \n\t" // 写入 32 字节
                "vst1.8 {d4-d7}, [%1]! \n\t" // 写入下 32 字节
                : "+r"(s), "+r"(d)
                :
                : "d0","d1","d2","d3","d4","d5","d6","d7","memory"
            );
        }
    }
    double end = get_time_sec();
    return (double)ITERATIONS * size / (end - start);
}

int main(void) {
    printf("Zynq-7000 内存带宽测试\n");

    // 使用 posix_memalign 代替 aligned_alloc 以增强 PetaLinux 兼容性
    void *a, *b;
    if (posix_memalign(&a, 4096, BYTES_TOTAL) != 0 || 
        posix_memalign(&b, 4096, BYTES_TOTAL) != 0) {
        perror("内存分配失败");
        return 1;
    }

    char *buf_a = (char *)a;
    char *buf_b = (char *)b;

    memset(buf_a, 0x55, BYTES_TOTAL);
    memset(buf_b, 0xAA, BYTES_TOTAL);

    printf("开始测试 (工作集: %d MiB, 重复: %d 次)...\n", SIZE_MB, ITERATIONS);

    double rbw = test_read(buf_a, BYTES_TOTAL);
    double wbw = test_write(buf_b, BYTES_TOTAL);
    double cbw = test_copy(buf_a, buf_b, BYTES_TOTAL);

    printf("\n结果汇总:\n");
    printf("顺序读:   %8.2f MB/s\n", rbw / (1024*1024));
    printf("顺序写:   %8.2f MB/s\n", wbw / (1024*1024));
    printf("顺序拷贝: %8.2f MB/s\n", cbw / (1024*1024));

    free(a);
    free(b);
    return 0;
}
