// patcher.c
// Simple binary patcher: replaces the first MOV EDX,0x21 (BA 21 00 00 00)
// with MOV EDX,0x00 (BA 00 00 00 00), forcing strncmp(...,0) so it always succeeds.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_binary> <output_binary>\n", argv[0]);
        return 1;
    }

    const char *in_path  = argv[1];
    const char *out_path = argv[2];
    FILE *fin  = fopen(in_path, "rb");
    if (!fin) { perror("fopen input"); return 1; }

    // Determine file size
    fseek(fin, 0, SEEK_END);
    long fsize = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    // Read entire file into buffer
    uint8_t *buf = malloc(fsize);
    if (!buf) { perror("malloc"); fclose(fin); return 1; }
    if (fread(buf, 1, fsize, fin) != fsize) {
        perror("fread");
        free(buf);
        fclose(fin);
        return 1;
    }
    fclose(fin);

    // Pattern: BA 21 00 00 00  (MOV EDX,0x21)
    // Replacement: BA 00 00 00 00 (MOV EDX,0x0)
    const uint8_t old_pat[5] = {0xBA, 0x21, 0x00, 0x00, 0x00};
    const uint8_t new_pat[5] = {0xBA, 0x00, 0x00, 0x00, 0x00};

    // Find first occurrence
    long patch_at = -1;
    for (long i = 0; i <= fsize - 5; i++) {
        if (memcmp(buf + i, old_pat, 5) == 0) {
            patch_at = i;
            break;
        }
    }

    if (patch_at < 0) {
        fprintf(stderr, "Error: pattern BA 21 00 00 00 not found!\n");
        free(buf);
        return 1;
    }

    // Apply the patch
    memcpy(buf + patch_at, new_pat, 5);
    printf("Patch applied at file offset 0x%lX\n", patch_at);

    // Write out the patched binary
    FILE *fout = fopen(out_path, "wb");
    if (!fout) { perror("fopen output"); free(buf); return 1; }
    if (fwrite(buf, 1, fsize, fout) != fsize) {
        perror("fwrite");
        fclose(fout);
        free(buf);
        return 1;
    }
    fclose(fout);
    free(buf);

    printf("Patched binary written to: %s\n", out_path);
    return 0;
}
