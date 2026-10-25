/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "xiaozhi_font_store.h"
#include "ui/fonts/cjk_reply_resource.h"
#include "util/sha256_soft.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <io.h>
#define fsync _commit
#endif

static atomic_bool ready;
static size_t received;
static const char *path = CJK_REPLY_PATH;
static size_t size = CJK_REPLY_BYTES;
static const char *sha = CJK_REPLY_SHA256;

static bool verified(const char *name)
{
    struct stat st;
    if (stat(name, &st) || (size_t)st.st_size != size) return false;
    FILE *f = fopen(name, "rb");
    if (!f) return false;
    unsigned char *buf = malloc(4096), digest[32];
    sha256_soft_t hash; sha256_soft_init(&hash);
    size_t count = 0, n;
    if (buf) while ((n = fread(buf, 1, 4096, f))) {
        sha256_soft_update(&hash, buf, n); count += n;
    }
    bool ok = buf && !ferror(f) && count == size;
    free(buf); fclose(f);
    if (!ok) return false;
    sha256_soft_final(&hash, digest);
    char hex[65];
    for (unsigned i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return strcmp(hex, sha) == 0;
}

void xiaozhi_font_store_init(void)
{
    atomic_store(&ready, verified(path)); received = 0;
}
bool xiaozhi_font_available(void) { return atomic_load(&ready); }
const char *xiaozhi_font_path(void) { return path; }
size_t xiaozhi_font_received(void) { return received; }
size_t xiaozhi_font_pending_bytes(void)
{
    char tmp[160]; struct stat st;
    if (snprintf(tmp, sizeof(tmp), "%s.part", path) >= (int)sizeof(tmp) || stat(tmp, &st) || st.st_size < 0) return 0;
    return (size_t)st.st_size <= size ? (size_t)st.st_size : 0;
}

bool xiaozhi_font_append(size_t offset, const void *data, size_t len)
{
    if (xiaozhi_font_available() || !data || !len || len > 32768 || offset > size || len > size - offset) return false;
    char tmp[160];
    if (snprintf(tmp, sizeof(tmp), "%s.part", path) >= (int)sizeof(tmp)) return false;
    if (offset == 0) received = 0;
    if (offset != received) return false;
    FILE *f = fopen(tmp, offset ? "r+b" : "wb");
    if (!f) return false;
    bool ok = fseek(f, (long)offset, SEEK_SET) == 0 && fwrite(data, 1, len, f) == len && fflush(f) == 0 && fsync(fileno(f)) == 0;
    if (fclose(f)) ok = false;
    if (!ok) return false;
    received += len;
    if (received != size) return true;
    if (!verified(tmp)) { remove(tmp); received = 0; return false; }
    /* Never replace even a corrupt previous resource. The immutable versioned
     * path may be open in a font instance; retain it for diagnosis/recovery. */
    struct stat st;
    if (stat(path, &st) == 0 || rename(tmp, path)) return false;
    atomic_store(&ready, true);
    return true;
}

#ifdef XIAOZHI_FONT_TEST
void xiaozhi_font_test_paths(const char *p, size_t n, const char *s)
{
    path = p; size = n; sha = s; received = 0; atomic_store(&ready, false);
}
#endif
