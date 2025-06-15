#ifndef DISK_H
#define DISK_H

#include <stdio.h>
#include <stdint.h>

#define BLOCK_SIZE 4096

int disk_init(char *filename, int nblocks);

int disk_size();

int disk_read(uint32_t blocknum, void *buf);

int disk_write(uint32_t blocknum, void *buf);

int disk_close();

#endif