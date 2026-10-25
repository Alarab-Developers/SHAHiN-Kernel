/* =========================================================
 * tools/arpc.c
 * مترجم ARP على المضيف — يُنتج ملفات .تطبيق
 * ========================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* إعلان مسبق فقط — لا نضمّ مجمع_الترجمة.h لتفادي رؤوس النواة */
int arp_aot_compile(const char *script,
                    uint32_t    size,
                    void      **out_binary,
                    size_t     *out_size);

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s input.arp [-o output.تطبيق]\n", argv[0]);
        return 1;
    }

    const char *in_path  = argv[1];
    const char *out_path = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_path = argv[i + 1];
            i++;
        }
    }

    /* اسم المخرج الافتراضي */
    char auto_out[512];
    if (!out_path) {
        strncpy(auto_out, in_path, sizeof(auto_out) - 1);
        auto_out[sizeof(auto_out) - 1] = '\0';
        char *dot = strrchr(auto_out, '.');
        if (dot) *dot = '\0';
        strncat(auto_out, ".تطبيق",
                sizeof(auto_out) - strlen(auto_out) - 1);
        out_path = auto_out;
    }

    /* اقرأ المصدر */
    FILE *f = fopen(in_path, "rb");
    if (!f) { perror(in_path); return 1; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fprintf(stderr, "empty file: %s\n", in_path);
        fclose(f);
        return 1;
    }

    char *src = (char *)malloc((size_t)size);
    if (!src) { fclose(f); return 1; }
    if (fread(src, 1, (size_t)size, f) != (size_t)size) {
        fclose(f); free(src); return 1;
    }
    fclose(f);

    /* ترجم */
    void  *binary = NULL;
    size_t binary_size = 0;

    if (arp_aot_compile(src, (uint32_t)size, &binary, &binary_size) < 0) {
        fprintf(stderr, "compile failed: %s\n", in_path);
        free(src);
        return 1;
    }
    free(src);

    /* اكتب */
    FILE *o = fopen(out_path, "wb");
    if (!o) {
        perror(out_path);
        free(binary);
        return 1;
    }
    if (fwrite(binary, 1, binary_size, o) != binary_size) {
        fclose(o); free(binary); return 1;
    }
    fclose(o);
    free(binary);

    printf("[arpc] %s -> %s (%zu bytes)\n",
           in_path, out_path, binary_size);
    return 0;
}
