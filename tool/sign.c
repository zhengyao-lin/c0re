//　format bootloader sector

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#define err(...) \
    fprintf(stderr, "sign error: "); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n");
    
#define trace err

int main(int argc, char **argv)
{
    struct stat st;
    
    if (argc != 3) {
        err("usage: %s <input filename> <output filename>", argv[0]);
        return -1;
    }
    
    if (stat(argv[1], &st) != 0) {
        err("error opening file '%s': %s", argv[1], strerror(errno));
        return -1;
    }
    
    trace("'%s' size: %ld bytes", argv[1], (long)st.st_size);
    
    if (st.st_size > 510) {
        err("size %ld greater than expected size 510", (long)st.st_size);
        return -1;
    }
    
    char buf[512];
    memset(buf, 0, sizeof(buf));
    
    FILE *ifp = fopen(argv[1], "rb");
    
    if (!ifp) {
        err("unable to open file %s", argv[1]);
        return -1;
    }
    
    int size = fread(buf, 1, st.st_size, ifp);
    
    if (size != st.st_size) {
        err("read '%s' error, size is %d", argv[1], size);
        return -1;
    }
    
    fclose(ifp);
    buf[510] = 0x55;
    buf[511] = 0xAA;
    FILE *ofp = fopen(argv[2], "wb+");
    size = fwrite(buf, 1, 512, ofp);
    
    if (size != 512) {
        err("write '%s' error, size is %d", argv[2], size);
        return -1;
    }
    
    fclose(ofp);
    trace("build 512 bytes boot sector: '%s' success!", argv[2]);
    
    return 0;
}
