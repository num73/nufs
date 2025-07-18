# How to Build a Userspace File System

## Userspace File System

The userspace file system we are talking here is not the userspace file system
framework like FUSE. It is a file system implemented by intercepting system calls.

### Why Userspace File System

1. Easier to implement.
2. High performace.


### Why NUFS

It provide the basic framework you will need to build a userspace file system.
Some basic tools are provided.

## Posix Syscall

The following table lists the POSIX system calls you need to support: 

| System Call | Description |
|-------------|-------------|
| `SYS_open` | Open a file and return a file descriptor |
| `SYS_openat` | Open a file relative to a directory file descriptor |
| `SYS_creat` | Create a file (equivalent to open with O_CREAT\|O_WRONLY\|O_TRUNC) |
| `SYS_read` | Read data from a file descriptor |
| `SYS_pread64` | Read data from a specific offset in a file without changing file pointer |
| `SYS_write` | Write data to a file descriptor |
| `SYS_pwrite64` | Write data to a specific offset in a file without changing file pointer |
| `SYS_close` | Close a file descriptor |
| `SYS_lseek` | Move file pointer to a specified position |
| `SYS_mkdir` | Create a directory |
| `SYS_rmdir` | Remove an empty directory |
| `SYS_rename` | Rename a file or directory |
| `SYS_fallocate` | Preallocate space for a file |
| `SYS_stat` | Get file status information |
| `SYS_newfstatat` | Get file status information relative to a directory file descriptor |
| `SYS_lstat` | Get status information of a symbolic link itself (without following the link) |
| `SYS_fstat` | Get file status information through a file descriptor |
| `SYS_truncate` | Truncate a file to a specified length by path |
| `SYS_ftruncate` | Truncate a file to a specified length by file descriptor |
| `SYS_unlink` | Delete a file |
| `SYS_symlink` | Create a symbolic link |
| `SYS_access` | Check file access permissions |
| `SYS_fsync` | Synchronize file data and metadata to storage device |
| `SYS_fdatasync` | Synchronize file data to storage device (excluding metadata) |
| `SYS_sync` | Synchronize all filesystem buffers to storage device |
| `SYS_fcntl` | File control operations (such as setting file status flags) |
| `SYS_mmap` | Map a file into memory |
| `SYS_munmap` | Unmap memory mapping |

```c

static int hook(long syscall_number, long arg0, long arg1, long arg2, long arg3,
                long arg4, long arg5, long *result) {
    switch (syscall_number) {
    case SYS_open:
        return shim_do_open((const char *)arg0, (int)arg1, (mode_t)arg2,
                            (int *)result);
    case SYS_openat:
        return shim_do_openat((int)arg0, (const char *)arg1, (int)arg2,
                              (mode_t)arg3, (int *)result);
    case SYS_creat:
        return shim_do_create((char *)arg0, (mode_t)arg1, (int *)result);
    case SYS_read:
        return shim_do_read((int)arg0, (void *)arg1, (size_t)arg2,
                            (ssize_t *)result);
    case SYS_pread64:
        return shim_do_pread64((int)arg0, (void *)arg1, (size_t)arg2,
                               (off_t)arg3, (ssize_t *)result);
    case SYS_write:
        return shim_do_write((int)arg0, (void *)arg1, (size_t)arg2,
                             (ssize_t *)result);
    case SYS_pwrite64:
        return shim_do_pwrite64((int)arg0, (void *)arg1, (size_t)arg2,
                                (off_t)arg3, (ssize_t *)result);
    case SYS_close:
        return shim_do_close((int)arg0, (int *)result);
    case SYS_lseek:
        return shim_do_lseek((int)arg0, (off_t)arg1, (int)arg2,
                             (off_t *)result);
    case SYS_mkdir:
        return shim_do_mkdir((const char *)arg0, (mode_t)arg1, (int *)result);
    case SYS_rmdir:
        return shim_do_rmdir((const char *)arg0, (int *)result);
    case SYS_rename:
        return shim_do_rename((const char *)arg0, (const char *)arg1,
                              (int *)result);
    case SYS_fallocate:
        return shim_do_fallocate((int)arg0, (int)arg1, (off_t)arg2, (off_t)arg3,
                                 (int *)result);
    case SYS_stat:
        return shim_do_stat((const char *)arg0, (struct stat *)arg1,
                            (int *)result);
    case SYS_newfstatat:
        return shim_do_fstatat((int)arg0, (const char *)arg1,
                               (struct stat *)arg2, (int)arg3, (int *)result);
    case SYS_lstat:
        return shim_do_lstat((const char *)arg0, (struct stat *)arg1,
                             (int *)result);

    case SYS_fstat:
        return shim_do_fstat((int)arg0, (struct stat *)arg1, (int *)result);
    case SYS_truncate:
        return shim_do_truncate((const char *)arg0, (off_t)arg1, (int *)result);
    case SYS_ftruncate:
        return shim_do_ftruncate((int)arg0, (off_t)arg1, (int *)result);
    case SYS_unlink:
        return shim_do_unlink((const char *)arg0, (int *)result);
    case SYS_symlink:

        return shim_do_symlink((const char *)arg0, (const char *)arg1,
                               (int *)result);
    case SYS_access:
        return shim_do_access((const char *)arg0, (int)arg1, (int *)result);
    case SYS_fsync:
        return shim_do_fsync((int)arg0, (int *)result);
    case SYS_fdatasync:
        return shim_do_fdatasync((int)arg0, (int *)result);
    case SYS_sync:
        return shim_do_sync((int *)result);
    case SYS_fcntl:
        return shim_do_fcntl((int)arg0, (int)arg1, (void *)arg2, (int *)result);
    case SYS_mmap:
        return shim_do_mmap((void *)arg0, (size_t)arg1, (int)arg2, (int)arg3,
                            (int)arg4, (off_t)arg5, (void **)result);
    case SYS_munmap:
        return shim_do_munmap((void *)arg0, (size_t)arg1, (int *)result);
    }

    return 1;
}

```

## Benchmark

### Performance Metrices

#### IOPS

IOPS means input/output operations per second. 

#### Throughput

#### Latency

Latency refers to the time taken for an operation to complete.

### Tools

#### FIO

#### Filebench

#### YCSB


