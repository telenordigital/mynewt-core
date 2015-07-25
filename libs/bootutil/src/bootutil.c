#include <string.h>
#include <inttypes.h>
#include "hal/hal_flash.h"
#include "ffs/ffs.h"
#include "bootutil/crc32.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"

#define BOOT_PATH_CUR   "/boot/cur"
#define BOOT_PATH_NEXT  "/boot/next"

int
boot_crc_is_valid(uint32_t addr, const struct image_header *hdr)
{
    uint32_t crc_off;
    uint32_t crc_len;
    uint32_t crc;

    crc_off = offsetof(struct image_header, crc32) + sizeof hdr->crc32;
    crc_len = hdr->hdr_size - crc_off + hdr->img_size;
    crc = crc32(0, (void *)(addr + crc_off), crc_len);

    return crc == hdr->crc32;
}

static int
boot_vect_read_one(struct image_version *ver, const char *path)
{
    struct ffs_file *file;
    uint32_t len;
    int rc;

    rc = ffs_open(&file, path, FFS_ACCESS_READ);
    if (rc != 0) {
        rc = BOOT_EBADVECT;
        goto done;
    }

    len = sizeof *ver;
    rc = ffs_read(file, ver, &len);
    if (rc != 0) {
        rc = BOOT_EFILE;
        goto done;
    }
    if (len != sizeof *ver) {
        rc = BOOT_EBADVECT;
        goto done;
    }

    rc = 0;

done:
    ffs_close(file);
    return rc;
}

int
boot_vect_read_cur(struct image_version *out_ver)
{
    int rc;

    rc = boot_vect_read_one(out_ver, BOOT_PATH_CUR);
    if (rc != 0) {
        return rc; // XXX
    }

    return 0;
}

int
boot_vect_rotate(void)
{
    int rc;

    rc = ffs_rename(BOOT_PATH_NEXT, BOOT_PATH_CUR);
    switch (rc) {
    case 0:
        /* Last-known-good moved into the current position. */
        return 0;

    case FFS_ENOENT:
        /* There was no last-known-goot specified; no change. */
        return 0;

    default:
        /* File system error. */
        return BOOT_EFILE;
    }
}

int
boot_vect_repair(void)
{
    int rc;

    rc = ffs_rename(BOOT_PATH_NEXT, BOOT_PATH_CUR);

    switch (rc) {
    case 0:
        return 0;

    case FFS_ENOENT:
        ffs_unlink(BOOT_PATH_CUR);
        return 0;

    default:
        return BOOT_EFILE;
    }
}

static int
boot_read_image_header(struct image_header *out_hdr, uint32_t flash_address)
{
    int rc;

    rc = flash_read(out_hdr, flash_address, sizeof *out_hdr);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    if (out_hdr->magic != IMG_MAGIC) {
        return BOOT_EBADIMAGE;
    }

    return 0;
}

void
boot_read_image_headers(struct image_header *out_headers, int *out_num_headers,
                        const uint32_t *addresses, int num_addresses)
{
    int rc;
    int i;

    for (i = 0; i < num_addresses; i++) {
        rc = boot_read_image_header(out_headers + i, addresses[i]);
        if (rc != 0) {
            break;
        }
    }

    *out_num_headers = i;
}

