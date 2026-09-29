#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s /path/to/library.dylib\n", argv[0]);
        return 2;
    }
    dlerror();
    void *handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char *error = dlerror();
        fprintf(stderr, "dlopen failed: %s\n", error ? error : "unknown error");
        return 1;
    }
    puts("dlopen ok");
    dlclose(handle);
    return 0;
}
