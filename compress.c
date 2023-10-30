#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WINDOW_SIZE 4096   // 窗口大小
#define MAX_LOOKAHEAD_SIZE 16  // 向前看缓冲区大小

typedef struct {
    int offset;  // 指向匹配字符串在滑动窗口中的偏移量
    int length;  // 匹配字符串的长度
    char nextChar;  // 下一个字符
} Match;

void compressFile(const char* inputFile, const char* outputFile) {
    FILE* input = fopen(inputFile, "rb");
    FILE* output = fopen(outputFile, "wb");
    if (input == NULL || output == NULL) {
        printf("Failed to open files\n");
        return;
    }

    unsigned char window[MAX_WINDOW_SIZE];
    unsigned char lookahead[MAX_LOOKAHEAD_SIZE];

    int windowPos = 0;
    int lookaheadPos = 0;

    // 初始化窗口和向前看缓冲区
    memset(window, 0, sizeof(window));
    fread(lookahead, 1, MAX_LOOKAHEAD_SIZE, input);
    int bytesRead = ftell(input);

    while (bytesRead > 0) {
        Match longestMatch = {0, 0, lookahead[0]};

        // 在窗口中查找最长匹配
        for (int i = windowPos - 1; i >= 0 && i >= windowPos - MAX_WINDOW_SIZE; --i) {
            int len = 0;
            while (len < MAX_LOOKAHEAD_SIZE && lookahead[len] == window[(i + len) % MAX_WINDOW_SIZE]) {
                ++len;
            }
            if (len > longestMatch.length) {
                longestMatch.offset = windowPos - i - 1;
                longestMatch.length = len;
                longestMatch.nextChar = lookahead[len];
            }
        }

        // 写入最长匹配的偏移和长度
        fwrite(&longestMatch, sizeof(Match), 1, output);

        // 更新窗口和向前看缓冲区
        for (int i = 0; i < longestMatch.length + 1; ++i) {
            window[windowPos] = lookahead[i];
            windowPos = (windowPos + 1) % MAX_WINDOW_SIZE;
            if (bytesRead > 0) {
                if (fread(lookahead, 1, 1, input) == 1) {
                    bytesRead = ftell(input);
                } else {
                    bytesRead = 0;
                }
            }
        }
    }

    fclose(input);
    fclose(output);
}

void decompressFile(const char* compressedFile, const char* outputFile) {
    FILE* input = fopen(compressedFile, "rb");
    FILE* output = fopen(outputFile, "wb");
    if (input == NULL || output == NULL) {
        printf("Failed to open files\n");
        return;
    }

    unsigned char window[MAX_WINDOW_SIZE];
    unsigned char lookahead[MAX_LOOKAHEAD_SIZE];

    int windowPos = 0;
    int lookaheadPos = 0;

    // 初始化窗口和向前看缓冲区
    memset(window, 0, sizeof(window));
    fread(lookahead, 1, MAX_LOOKAHEAD_SIZE, input);
    int bytesRead = ftell(input);

    while (!feof(input)) {
        Match match;

        // 从压缩文件读取匹配信息
        fread(&match, sizeof(Match), 1, input);

        // 从窗口中复制匹配字符串到输出文件
        for (int i = 0; i < match.length; ++i) {
            unsigned char ch = window[(windowPos - match.offset + i) % MAX_WINDOW_SIZE];
            fwrite(&ch, 1, 1, output);
        }

        // 写入下一个字符
        fwrite(&match.nextChar, 1, 1, output);

        // 更新窗口和向前看缓冲区
        for (int i = 0; i < match.length + 1; ++i) {
            window[windowPos] = match.nextChar;
            windowPos = (windowPos + 1) % MAX_WINDOW_SIZE;
            if (bytesRead > 0) {
                if (fread(lookahead, 1, 1, input) == 1) {
                    bytesRead = ftell(input);
                } else {
                    bytesRead = 0;
                }
            }
        }
    }

    fclose(input);
    fclose(output);
}

int main() {
    const char* inputFile = "test/boot.log";
    const char* compressedFile = "compressed.bin";
    const char* decompressedFile = "decompressed.txt";

    // 压缩文件
    compressFile(inputFile, compressedFile);
    printf("File compressed successfully.\n");

    // 解压文件
    decompressFile(compressedFile, decompressedFile);
    printf("File decompressed successfully.\n");

    return 0;
}