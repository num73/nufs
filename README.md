# Num73 Userspace File System (NUFS) #


NUFS (Num73 Userspace File System) is a userspace filesystem template. 

I'm working on some file system research. I implemented a user-space file system by intercepting glibc functions in less than a week. The file system can have its performance parameters tested using FIO. I thought implementing a user-space file system was a very simple task. However, when I want to use YCSB to test the performance of RockDB running on this file system, it just don't work. I realized that either my file system did not intercept all the file functions used by RocksDB, or there were bugs in my code. I have absolutely no idea about debugging.I saw that the open-source code structure in a paper (PolyStore, FAST'25) on user-space file systems is very clear, and I thought I could refer to its code to write my own file system framework. If I need to work on file systems again in the future, I just need to fill in the content within this framework. 


# Dependencies #

## Runtime dependencies ##

 * libcapstone -- the disassembly engine used under the hood

## Build dependencies ##

# How to build #

Building nufs requires cmake.

Example:

```sh

mkdir -p build
cd build
cmake ..

```


# Synopsis #

```sh
LD_PRELOAD=./libnufs.so ./application
```


# Description: #



## gtest #

Unit testing is a very useful tool in software development, and everyone who has used it speaks highly of it. The directory structure of nufs includes the glib2 unit testing framework gtest. You can choose to use it or not.

## Third ## 

NUFS has added dependencies on some commonly used tool libraries, so there's no need to reinvent the wheel. 

### syscall_intercept ###

https://github.com/pmem/syscall_intercept

### Melon ### 

http://doc.melonc.io/

### glib2 ###

https://docs.gtk.org/glib/

If you don't need glib2, you can modify the `CMakeLists.txt` under the root directory.

> note: The glib2 and Melon are not compatible with each other!! There may be bugs if you use both of them.


# Notice! #

> **You have to deal with the `printf` or other log functions very carefully, no `printf` or `log functions` are suggested in `static __attribute__((constructor)) void init(void)`**


> **If you encounter `segmentation fault`, it may be due to the problem of circular calls to intercepted functions.**