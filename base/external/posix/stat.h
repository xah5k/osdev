#pragma once
#include <stdint.h>

typedef int64_t k_dev_t;
typedef uint64_t k_ino_t;
typedef uint32_t k_mode_t;
typedef uint64_t k_nlink_t;
typedef uint32_t k_uid_t;
typedef uint32_t k_gid_t;
typedef int64_t k_off_t;
typedef int64_t k_blksize_t;
typedef int64_t k_blkcnt_t;
typedef int64_t k_time_t;

typedef struct {
    k_dev_t     st_dev;
    k_ino_t     st_ino;
    k_nlink_t   st_nlink;
    k_mode_t    st_mode;
    k_uid_t     st_uid;
    k_gid_t     st_gid;
    int32_t     __pad0;
    k_dev_t     st_rdev;
    k_off_t     st_size;
    k_blksize_t st_blksize;
    k_blkcnt_t  st_blocks;
    k_time_t    st_atime;
    uint64_t    st_atimensec;
    k_time_t    st_mtime;
    uint64_t    st_mtimensec;
    k_time_t    st_ctime;
    uint64_t    st_ctimensec;
    int64_t     __glibc_reserved[3]; // dawg what is glibc even reserving
} posixstat;

#define K_S_IFCHR 0x02000
#define K_S_IFREG 0x08000
#define K_S_IFDIR 0x04000