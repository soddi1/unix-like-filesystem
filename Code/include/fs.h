#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <sys/types.h>

#include "disk.h"

#define INODE_SIZE 64
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)
#define INODE_DIRECT_POINTERS 11
#define INODE_INDIRECT_POINTERS_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t))

#define DIRECTORY_ENTRY_SIZE 32
#define DIRECTORY_NAME_SIZE 28
#define DIRECTORY_ENTRIES_PER_BLOCK (BLOCK_SIZE / DIRECTORY_ENTRY_SIZE)
#define DIRECTORY_DEPTH_LIMIT 10

#define FLAGS_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t))

struct superblock
{
    uint32_t s_blocks_count;
    uint32_t s_inodes_count;
    uint32_t s_block_bitmap;
    uint32_t s_inode_bitmap;
    uint32_t s_inode_table_block_start;
    uint32_t s_data_blocks_start;
};

struct inode
{
    uint64_t i_size;
    uint32_t i_is_directory;
    uint32_t i_direct_pointers[INODE_DIRECT_POINTERS];
    uint32_t i_single_indirect_pointer;
    uint32_t i_double_indirect_pointer;
};

struct directory_entry
{
    uint32_t inode_number;
    char name[DIRECTORY_NAME_SIZE];
};

struct directory_block
{
    struct directory_entry entries[DIRECTORY_ENTRIES_PER_BLOCK];
};

union block
{
    struct superblock superblock;         
    struct inode inodes[INODES_PER_BLOCK];         
    uint32_t bitmap[FLAGS_PER_BLOCK];      
    struct directory_block directory_block;              
    uint8_t data[BLOCK_SIZE];
    uint32_t pointers[INODE_INDIRECT_POINTERS_PER_BLOCK];
};

int fs_format();

int fs_mount();

int fs_create(char *path, int is_directory);

int fs_remove(char *path);

int fs_read(char *path, void *buf, size_t count, off_t offset);

int fs_write(char *path, void *buf, size_t count, off_t offset);

int fs_list(char *path);

void fs_stat();

#endif