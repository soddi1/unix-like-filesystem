#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk.h"

static FILE *disk;               // disk file pointer
static uint32_t number_of_blocks = 0; // number of blocks in the disk
static int reads = 0;            // number of reads from the disk
static int writes = 0;           // number of writes to the disk

int disk_init(char *filename, int nblocks)
{
    disk = fopen(filename, "r+");

    if (disk == NULL)
    {
        disk = fopen(filename, "w+");
        if (disk == NULL)
        {
            return -1;
        }
        char *block = calloc(BLOCK_SIZE, sizeof(char));

        for (int i = 0; i < nblocks; i++)
        {
            fwrite(block, BLOCK_SIZE, 1, disk);
        }

        free(block);
    }

    number_of_blocks = nblocks;

    return 0;
}

int disk_size()
{
    // Return the number of blocks.
    return number_of_blocks;
}

static int sanity_check(uint32_t blocknum, const void *buf)
{
    if (blocknum >= number_of_blocks)
    {
        printf("ERROR: Block number must be less than %d.\n", number_of_blocks);
        return -1;
    }

    if (buf == NULL)
    {
        printf("ERROR: Buffer cannot be NULL.\n");
        return -1;
    }

    return 0;
}

int disk_read(uint32_t blocknum, void *buf)
{
    if (sanity_check(blocknum, buf) != 0)
    {
        return -1;
    }

    fseek(disk, blocknum * BLOCK_SIZE, SEEK_SET);
    int blocks_read = fread(buf, BLOCK_SIZE, 1, disk);

    if (blocks_read != 1)
    {
        printf("ERROR: Could not read block %d.\n", blocknum);
        return -1;
    }

    reads++;
    return BLOCK_SIZE;
}

int disk_write(uint32_t blocknum, void *buf)
{
    if (sanity_check(blocknum, buf) != 0)
    {
        return -1;
    }

    fseek(disk, blocknum * BLOCK_SIZE, SEEK_SET);

    int blocks_written = fwrite(buf, BLOCK_SIZE, 1, disk);

    if (blocks_written != 1)
    {
        printf("ERROR: Could not write block %d.\n", blocknum);
        return -1;
    }

    writes++;

    return BLOCK_SIZE;
}

int disk_close()
{
    if (disk == NULL)
    {
        printf("ERROR: Disk is not open.\n");
        return -1;
    }
    else if (fclose(disk) != 0)
    {
        printf("ERROR: Could not close disk.\n");
        return -1;
    }

    printf("Reads (Blocks): %d\n", reads);
    printf("Writes (Blocks): %d\n", writes);
    printf("Disk closed.\n");

    disk = NULL;

    return 0;
}