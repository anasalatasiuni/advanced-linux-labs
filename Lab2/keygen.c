#include <stdio.h>
#include <stdint.h>
#include <cpuid.h>
#include <openssl/md5.h>

int main(void) {
    unsigned int eax, ebx, ecx, edx;
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        fprintf(stderr, "CPUID not supported on this CPU\n");
        return 1;
    }

    // 1. Byte-swap
    uint32_t hw1 = __builtin_bswap32(eax);
    uint32_t hw2 = __builtin_bswap32(edx);

    // 2. Format PSN
    char psn[17];
    snprintf(psn, sizeof(psn), "%08X%08X", hw1, hw2);

    // 3. MD5
    unsigned char digest[16];
    MD5((unsigned char*)psn, 16, digest);

    // 4. Build reversed‐digest hex string
    char license[33] = {0};
    for (int i = 0; i < 16; i++) {
        sprintf(license + i*2, "%02x", digest[15 - i]);
    }

    // 5. Output
    printf("%s", license);
    return 0;
}
