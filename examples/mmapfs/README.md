# MmapFS — mmap-based Userspace Filesystem Example

MmapFS is a complete example of how to implement a userspace filesystem
using the NUFS framework.  It maps a raw file (or block device) into
memory with `mmap(2)` and manages all metadata and data in the mapped
region, providing POSIX-compatible file operations and basic crash
consistency via a write-ahead journal.

## Features

| Feature | Details |
|---------|---------|
| **POSIX interface** | `open`, `read`, `write`, `close`, `mkdir`, `rmdir`, `rename`, `stat`, `lseek`, `truncate`, `unlink`, `symlink`, `fsync`, `fdatasync`, `fallocate`, `fcntl`, `access` |
| **Directory tree** | Hierarchical directories with `.` / `..` entries, path resolution, fixed-size directory entries (256 B each) |
| **Block mapping** | 12 direct + 1 single-indirect + 1 double-indirect block pointers per inode (max file ≈ 4 GB) |
| **Crash consistency** | Redo-log write-ahead journal (32 blocks); atomic metadata commits with `msync` barriers |
| **Free-space management** | Bitmap-based allocation for both inodes and data blocks |
| **Thread safety** | Global mutex protects all filesystem operations |

## Disk Layout

```
+-------------------+  Block 0
|    Superblock      |
+-------------------+  Block 1
|    Journal         |  (32 blocks)
|    (WAL)           |
+-------------------+  Block 33
|  Inode Bitmap      |  (1 block)
+-------------------+  Block 34
|  Data Bitmap       |  (variable)
+-------------------+
|  Inode Table       |  (256 blocks default → 8192 inodes)
+-------------------+
|  Data Blocks       |  (remaining space)
+-------------------+
```

## Building

From the repository root:

```sh
mkdir -p build && cd build
cmake .. && make
```

This produces:

* `lib/libnufs_mmapfs.so` — shared library for `LD_PRELOAD`
* `bin/mkfs_mmapfs` — standalone formatting tool

## Usage

### 1. Create a Device Image

```sh
# Create a 64 MB filesystem image
./bin/mkfs_mmapfs /tmp/mmapfs.img 64
```

### 2. Run an Application

```sh
# All accesses under /mnt/nufs/ are handled by mmapfs
LD_PRELOAD=./lib/libnufs_mmapfs.so /bin/ls /mnt/nufs/
```

The backing device path defaults to `/tmp/mmapfs.img`.  Override it with
the `MMAPFS_DEVICE` environment variable:

```sh
MMAPFS_DEVICE=/dev/pmem0 MMAPFS_SIZE=1024 \
    LD_PRELOAD=./lib/libnufs_mmapfs.so ./my_app
```

`MMAPFS_SIZE` is in megabytes and is only used when creating a new image.

### 3. Automatic Formatting

If the backing file does not exist or is not formatted, the library
automatically formats it during initialisation (inside `nufs_init`).
The standalone `mkfs_mmapfs` tool is useful when you need to
pre-format a device before running an application.

## Crash Consistency

The journal guarantees atomic metadata updates.  The protocol is:

1. **Begin** – allocate a new transaction in the journal area.
2. **Log** – copy the *new* contents of each modified metadata block
   into the journal.
3. **Commit** – `msync` the journal, then set the committed flag and
   `msync` the descriptor block.  This is the atomic commit point.
4. **Apply** – copy the journal blocks to their real on-disk locations
   and `msync` them.
5. **Checkpoint** – clear the journal descriptor.

On recovery (unclean shutdown) the library checks the journal during
`nufs_init`.  If a committed but un-checkpointed transaction is found,
it is replayed.

## Source Files

| File | Description |
|------|-------------|
| `mmapfs.h` | On-disk / in-memory structures and function prototypes |
| `device.c` | `mmap`-based device layer (open, block access, sync) |
| `alloc.c` | Bitmap allocation, filesystem formatting |
| `inode.c` | Inode operations (lookup, block mapping, truncate) |
| `dir.c` | Directory operations, path resolution |
| `journal.c` | Write-ahead journal |
| `nufs_ops.c` | Implements all `nufs_*` POSIX API functions |
| `mkfs.c` | Standalone formatting tool |
