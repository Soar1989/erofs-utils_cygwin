/* SPDX-License-Identifier: GPL-2.0+ OR Apache-2.0 */
/*
 * Copyright (C) 2019 HUAWEI, Inc.
 *             http://www.huawei.com/
 * Created by Gao Xiang <gaoxiang25@huawei.com>
 */
#ifndef __EROFS_INTERNAL_H
#define __EROFS_INTERNAL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "list.h"
#include "err.h"

typedef unsigned short umode_t;

#include "erofs_fs.h"
#include <fcntl.h>
#include <sys/types.h> /* for off_t definition */
#include <sys/stat.h> /* for S_ISCHR definition */
#include <stdio.h>

#ifndef PATH_MAX
#define PATH_MAX        4096    /* # chars in a path name including nul */
#endif

#ifndef EROFS_MAX_BLOCK_SIZE
#define EROFS_MAX_BLOCK_SIZE	4096
#endif

#define EROFS_ISLOTBITS		5
#define EROFS_SLOTSIZE		(1U << EROFS_ISLOTBITS)

typedef u64 erofs_off_t;
typedef u64 erofs_nid_t;
/* data type for filesystem-wide blocks number */
typedef u32 erofs_blk_t;

#define NULL_ADDR	((unsigned int)-1)
#define NULL_ADDR_UL	((unsigned long)-1)

/* global sbi */
extern struct erofs_sb_info sbi;

#define erofs_blksiz(sbi)	(1u << (sbi)->blkszbits)
#define erofs_blknr(sbi, addr)  ((addr) >> (sbi)->blkszbits)
#define erofs_blkoff(sbi, addr) ((addr) & (erofs_blksiz(sbi) - 1))
#define erofs_pos(sbi, nr)      ((erofs_off_t)(nr) << (sbi)->blkszbits)
#define BLK_ROUND_UP(sbi, addr)	DIV_ROUND_UP(addr, erofs_blksiz(sbi))

struct erofs_buffer_head;

struct erofs_device_info {
	u32 blocks;
	u32 mapped_blkaddr;
};

struct erofs_xattr_prefix_item {
	struct erofs_xattr_long_prefix *prefix;
	u8 infix_len;
};

#define EROFS_PACKED_NID_UNALLOCATED	-1

struct erofs_sb_info {
	struct erofs_device_info *devs;
	char *devname;

	u64 total_blocks;
	u64 primarydevice_blocks;

	erofs_blk_t meta_blkaddr;
	erofs_blk_t xattr_blkaddr;

	u32 feature_compat;
	u32 feature_incompat;
	u64 build_time;
	u32 build_time_nsec;

	u8  extslots;
	unsigned char islotbits;
	unsigned char blkszbits;

	/* what we really care is nid, rather than ino.. */
	erofs_nid_t root_nid;
	/* used for statfs, f_files - f_favail */
	u64 inos;

	u8 uuid[16];
	char volume_name[16];

	u16 available_compr_algs;
	u16 lz4_max_distance;

	u32 checksum;
	u16 extra_devices;
	union {
		u16 devt_slotoff;		/* used for mkfs */
		u16 device_id_mask;		/* used for others */
	};
	erofs_nid_t packed_nid;

	u32 xattr_prefix_start;
	u8 xattr_prefix_count;
	struct erofs_xattr_prefix_item *xattr_prefixes;

	int devfd;
	u64 devsz;
	dev_t dev;
	unsigned int nblobs;
	unsigned int blobfd[256];

	struct list_head list;

	u64 saved_by_deduplication;
};

/* make sure that any user of the erofs headers has atleast 64bit off_t type */
extern int erofs_assert_largefile[sizeof(off_t)-8];

#define EROFS_FEATURE_FUNCS(name, compat, feature) \
static inline bool erofs_sb_has_##name(struct erofs_sb_info *sbi) \
{ \
	return sbi->feature_##compat & EROFS_FEATURE_##feature; \
} \
static inline void erofs_sb_set_##name(struct erofs_sb_info *sbi) \
{ \
	sbi->feature_##compat |= EROFS_FEATURE_##feature; \
} \
static inline void erofs_sb_clear_##name(struct erofs_sb_info *sbi) \
{ \
	sbi->feature_##compat &= ~EROFS_FEATURE_##feature; \
}

EROFS_FEATURE_FUNCS(lz4_0padding, incompat, INCOMPAT_ZERO_PADDING)
EROFS_FEATURE_FUNCS(compr_cfgs, incompat, INCOMPAT_COMPR_CFGS)
EROFS_FEATURE_FUNCS(big_pcluster, incompat, INCOMPAT_BIG_PCLUSTER)
EROFS_FEATURE_FUNCS(chunked_file, incompat, INCOMPAT_CHUNKED_FILE)
EROFS_FEATURE_FUNCS(device_table, incompat, INCOMPAT_DEVICE_TABLE)
EROFS_FEATURE_FUNCS(ztailpacking, incompat, INCOMPAT_ZTAILPACKING)
EROFS_FEATURE_FUNCS(fragments, incompat, INCOMPAT_FRAGMENTS)
EROFS_FEATURE_FUNCS(dedupe, incompat, INCOMPAT_DEDUPE)
EROFS_FEATURE_FUNCS(xattr_prefixes, incompat, INCOMPAT_XATTR_PREFIXES)
EROFS_FEATURE_FUNCS(sb_chksum, compat, COMPAT_SB_CHKSUM)
EROFS_FEATURE_FUNCS(xattr_filter, compat, COMPAT_XATTR_FILTER)

#define EROFS_I_EA_INITED	(1 << 0)
#define EROFS_I_Z_INITED	(1 << 1)

struct erofs_inode {
	struct list_head i_hash, i_subdirs, i_xattrs;

	union {
		/* (erofsfuse) runtime flags */
		unsigned int flags;
		/* (mkfs.erofs) queued sub-directories blocking dump */
		u32 subdirs_queued;
	};
	unsigned int i_count;
	struct erofs_sb_info *sbi;
	struct erofs_inode *i_parent;

	/* (mkfs.erofs) device ID containing source file */
	u32 dev;

	umode_t i_mode;
	erofs_off_t i_size;

	u64 i_ino[2];
	u32 i_uid;
	u32 i_gid;
	u64 i_mtime;
	u32 i_mtime_nsec;
	u32 i_nlink;

	union {
		u32 i_blkaddr;
		u32 i_blocks;
		u32 i_rdev;
		struct {
			unsigned short	chunkformat;
			unsigned char	chunkbits;
		};
	} u;

	char *i_srcpath;
	union {
		char *i_link;
		FILE *i_tmpfile;
	};
	unsigned char datalayout;
	unsigned char inode_isize;
	/* inline tail-end packing size */
	unsigned short idata_size;
	bool compressed_idata;
	bool lazy_tailblock;
	bool with_tmpfile;
	bool opaque;
	/* OVL: non-merge dir that may contain whiteout entries */
	bool whiteouts;

	unsigned int xattr_isize;
	unsigned int extent_isize;

	unsigned int xattr_shared_count;
	unsigned int *xattr_shared_xattrs;

	erofs_nid_t nid;
	struct erofs_buffer_head *bh;
	struct erofs_buffer_head *bh_inline, *bh_data;

	void *idata;

	/* (ztailpacking) in order to recover uncompressed EOF data */
	void *eof_tailraw;
	unsigned int eof_tailrawsize;

	union {
		void *compressmeta;
		void *chunkindexes;
		struct {
			uint16_t z_advise;
			uint8_t  z_algorithmtype[2];
			uint8_t  z_logical_clusterbits;
			uint8_t  z_physical_clusterblks;
			uint64_t z_tailextent_headlcn;
			unsigned int    z_idataoff;
#define z_idata_size	idata_size
		};
	};
#ifdef WITH_ANDROID
	uint64_t capabilities;
#endif
	erofs_off_t fragmentoff;
	unsigned int fragment_size;
};

static inline erofs_off_t erofs_iloc(struct erofs_inode *inode)
{
	struct erofs_sb_info *sbi = inode->sbi;

	return erofs_pos(sbi, sbi->meta_blkaddr) +
			(inode->nid << sbi->islotbits);
}

static inline bool is_inode_layout_compression(struct erofs_inode *inode)
{
	return erofs_inode_is_data_compressed(inode->datalayout);
}

static inline unsigned int erofs_bitrange(unsigned int value, unsigned int bit,
					  unsigned int bits)
{
	return (value >> bit) & ((1 << bits) - 1);
}

static inline unsigned int erofs_inode_version(unsigned int value)
{
	return erofs_bitrange(value, EROFS_I_VERSION_BIT,
			      EROFS_I_VERSION_BITS);
}

static inline unsigned int erofs_inode_datalayout(unsigned int value)
{
	return erofs_bitrange(value, EROFS_I_DATALAYOUT_BIT,
			      EROFS_I_DATALAYOUT_BITS);
}

#define IS_ROOT(x)	((x) == (x)->i_parent)

struct erofs_dentry {
	struct list_head d_child;	/* child of parent list */

	unsigned int type;
	char name[EROFS_NAME_LEN];
	union {
		struct erofs_inode *inode;
		erofs_nid_t nid;
	};
};

static inline bool is_dot_dotdot_len(const char *name, unsigned int len)
{
	if (len >= 1 && name[0] != '.')
		return false;

	return len == 1 || (len == 2 && name[1] == '.');
}

static inline bool is_dot_dotdot(const char *name)
{
	if (name[0] != '.')
		return false;

	return name[1] == '\0' || (name[1] == '.' && name[2] == '\0');
}

#include <stdio.h>
#include <string.h>

static inline const char *erofs_strerror(int err)
{
	static char msg[256];

	sprintf(msg, "[Error %d] %s", -err, strerror(-err));
	return msg;
}

enum {
	BH_Meta,
	BH_Mapped,
	BH_Encoded,
	BH_FullMapped,
	BH_Fragment,
	BH_Partialref,
};

/* Has a disk mapping */
#define EROFS_MAP_MAPPED	(1 << BH_Mapped)
/* Located in metadata (could be copied from bd_inode) */
#define EROFS_MAP_META		(1 << BH_Meta)
/* The extent is encoded */
#define EROFS_MAP_ENCODED	(1 << BH_Encoded)
/* The length of extent is full */
#define EROFS_MAP_FULL_MAPPED	(1 << BH_FullMapped)
/* Located in the special packed inode */
#define EROFS_MAP_FRAGMENT	(1 << BH_Fragment)
/* The extent refers to partial decompressed data */
#define EROFS_MAP_PARTIAL_REF	(1 << BH_Partialref)

struct erofs_map_blocks {
	char mpage[EROFS_MAX_BLOCK_SIZE];

	erofs_off_t m_pa, m_la;
	u64 m_plen, m_llen;

	unsigned short m_deviceid;
	char m_algorithmformat;
	unsigned int m_flags;
	erofs_blk_t index;
};

/*
 * Used to get the exact decompressed length, e.g. fiemap (consider lookback
 * approach instead if possible since it's more metadata lightweight.)
 */
#define EROFS_GET_BLOCKS_FIEMAP	0x0002
/* Used to map tail extent for tailpacking inline or fragment pcluster */
#define EROFS_GET_BLOCKS_FINDTAIL	0x0008

enum {
	Z_EROFS_COMPRESSION_SHIFTED = Z_EROFS_COMPRESSION_MAX,
	Z_EROFS_COMPRESSION_INTERLACED,
	Z_EROFS_COMPRESSION_RUNTIME_MAX
};

struct erofs_map_dev {
	erofs_off_t m_pa;
	unsigned int m_deviceid;
};

/* super.c */
int erofs_read_superblock(struct erofs_sb_info *sbi);
void erofs_put_super(struct erofs_sb_info *sbi);

/* namei.c */
int erofs_read_inode_from_disk(struct erofs_inode *vi);
int erofs_ilookup(const char *path, struct erofs_inode *vi);

/* data.c */
int erofs_pread(struct erofs_inode *inode, char *buf,
		erofs_off_t count, erofs_off_t offset);
int erofs_map_blocks(struct erofs_inode *inode,
		struct erofs_map_blocks *map, int flags);
int erofs_map_dev(struct erofs_sb_info *sbi, struct erofs_map_dev *map);
int erofs_read_one_data(struct erofs_inode *inode, struct erofs_map_blocks *map,
			char *buffer, u64 offset, size_t len);
int z_erofs_read_one_data(struct erofs_inode *inode,
			struct erofs_map_blocks *map, char *raw, char *buffer,
			erofs_off_t skip, erofs_off_t length, bool trimmed);
void *erofs_read_metadata(struct erofs_sb_info *sbi, erofs_nid_t nid,
			  erofs_off_t *offset, int *lengthp);

static inline int erofs_get_occupied_size(const struct erofs_inode *inode,
					  erofs_off_t *size)
{
	*size = 0;
	switch (inode->datalayout) {
	case EROFS_INODE_FLAT_INLINE:
	case EROFS_INODE_FLAT_PLAIN:
	case EROFS_INODE_CHUNK_BASED:
		*size = inode->i_size;
		break;
	case EROFS_INODE_COMPRESSED_FULL:
	case EROFS_INODE_COMPRESSED_COMPACT:
		*size = inode->u.i_blocks * erofs_blksiz(inode->sbi);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

/* data.c */
int erofs_getxattr(struct erofs_inode *vi, const char *name, char *buffer,
		   size_t buffer_size);
int erofs_listxattr(struct erofs_inode *vi, char *buffer, size_t buffer_size);

/* zmap.c */
int z_erofs_fill_inode(struct erofs_inode *vi);
int z_erofs_map_blocks_iter(struct erofs_inode *vi,
			    struct erofs_map_blocks *map, int flags);

#ifdef EUCLEAN
#define EFSCORRUPTED	EUCLEAN		/* Filesystem is corrupted */
#else
#define EFSCORRUPTED	EIO
#endif

#define CRC32C_POLY_LE	0x82F63B78
static inline u32 erofs_crc32c(u32 crc, const u8 *in, size_t len)
{
	int i;

	while (len--) {
		crc ^= *in++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? CRC32C_POLY_LE : 0);
	}
	return crc;
}

#define EROFS_WHITEOUT_DEV	0
static inline bool erofs_inode_is_whiteout(struct erofs_inode *inode)
{
	return S_ISCHR(inode->i_mode) && inode->u.i_rdev == EROFS_WHITEOUT_DEV;
}

#ifdef __cplusplus
}
#endif

#endif
