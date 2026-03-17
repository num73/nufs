# Examples

This directory contains example filesystem implementations built on the
NUFS framework.

| Example | Description |
|---------|-------------|
| [mmapfs](mmapfs/) | A userspace filesystem that uses `mmap` on a raw file/device to provide POSIX file operations with basic crash-consistency guarantees. |

## Quick Start

```sh
# Build everything (from the repository root)
mkdir -p build && cd build
cmake .. && make

# Format a 64 MB device image
./bin/mkfs_mmapfs /tmp/mmapfs.img 64

# Run your application with the mmapfs filesystem
LD_PRELOAD=./lib/libnufs_mmapfs.so ./your_application
```

Files accessed under `/mnt/nufs/` are transparently handled by the
example filesystem; all other paths fall through to the real kernel.
