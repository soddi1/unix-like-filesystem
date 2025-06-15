# simple-ext4-fs

![C](https://img.shields.io/badge/C-Language-blue)
![Filesystem](https://img.shields.io/badge/Filesystem-Simulator-orange)
![Operating Systems](https://img.shields.io/badge/OS-Project-purple)

A simplified file system designed to emulate the structure and behaviour of **ext4**, implemented in **C**. This project simulates a real file system with key features such as a **superblock**, **bitmaps**, **inode table**, and **hierarchical directories**, all backed by a **virtual disk image**.

---

## Features

- Superblock, inode bitmap, block bitmap, and inode/data blocks layout
- Directory and file support with hierarchical path handling
- File read/write using direct and indirect block pointers
- File deletion and recursive directory deletion
- Block-level and inode-level bitmaps
- Custom shell to interact with the virtual filesystem
- Dockerised environment for consistency
