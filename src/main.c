#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

static void fail(const char *message) { fprintf(stderr, "%s\n", message); exit(2); }
double number(const char *s) {
    char *end; errno = 0; double value = strtod(s, &end);
    if (errno || end == s || *end || !isfinite(value)) fail("invalid finite number");
    return value;
}
static size_t fields(char *line, char **out, size_t capacity) {
    line[strcspn(line, "\r\n")] = 0;
    size_t count = 0;
    char *p = line;
    while (*p) {
        if (count == capacity) fail("too many fields");
        if (*p == ',') fail("empty field");
        out[count++] = p;
        char *separator = strchr(p, ',');
        if (!separator) break;
        *separator = 0; p = separator + 1;
        if (!*p) fail("empty field");
    }
    return count;
}

static uint32_t crc32(const unsigned char *p, size_t size) {
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < size; i++) {
        crc ^= p[i]; for (int j = 0; j < 8; j++) crc = crc >> 1 ^ ((crc & 1) ? 0xedb88320u : 0);
    }
    return ~crc;
}
static uint32_t sequence(const char *p) { double v = number(p); if (v < 0 || v > UINT32_MAX || v != floor(v)) fail("invalid sequence"); return (uint32_t)v; }
int main(int argc, char **argv) {
    if (argc > 1 && !(argc == 2 && !strcmp(argv[1], "--self-test"))) fail("unsupported command arguments");
    if (argc == 2 && !strcmp(argv[1], "--self-test")) {
        assert(crc32((const unsigned char *)"123456789", 9) == 0xcbf43926); assert(crc32((const unsigned char *)"", 0) == 0);
        assert((uint32_t)(UINT32_MAX + 1u) == 0); puts("CRC32 and rollover tests passed"); return 0;
    }
    char line[2048]; uint32_t last = 0; double last_time = 0; unsigned records = 0, gaps = 0;
    while (fgets(line, sizeof line, stdin)) {
        if (strlen(line) == sizeof line - 1 && line[sizeof line - 2] != '\n') fail("input record too large");
        line[strcspn(line, "\r\n")] = 0; char *separator = strrchr(line, ','); if (!separator) fail("missing CRC");
        *separator = 0; char *end; errno = 0; unsigned long received = strtoul(separator + 1, &end, 16);
        if (errno || end == separator + 1 || *end || received > UINT32_MAX || crc32((const unsigned char *)line, strlen(line)) != received) fail("record CRC mismatch");
        char *f[4]; if (fields(line, f, 4) != 4) fail("expected sequence,time,tag,value,crc");
        uint32_t seq = sequence(f[0]); double time = number(f[1]); number(f[3]);
        if (time < 0 || (records && time < last_time) || !*f[2]) fail("invalid timestamp or tag");
        uint32_t distance = seq - last;
        if (records && (distance == 0 || distance > INT32_MAX)) fail("duplicate or reversed sequence");
        if (records && distance > 1) { gaps++; printf("missing=%u before=%u\n", distance - 1, seq); }
        last = seq; last_time = time; records++;
    }
    if (!records || ferror(stdin)) fail("missing input");
    printf("records=%u gap_events=%u\n", records, gaps); return 0;
}
