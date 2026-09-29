#include <fcntl.h>
#include <string.h>
#include <unistd.h>

__attribute__((constructor)) static void loader_probe(void) {
    static const char message[] = "loaded\n";
    int fd = open("/var/mobile/nukewireless_loader_probe.log",
                  O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) return;
    write(fd, message, sizeof(message) - 1);
    close(fd);
}
