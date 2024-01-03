/*
 *  Copyright (C) 2023 CS416 Rutgers CS
 *	Tiny File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX];

bitmap_t inode_bitmap;
bitmap_t disk_bitmap;

static struct superblock *superblock;

int get_avail_ino()
{
	// Step 1: Read inode bitmap from disk
	if (!bio_read(superblock->i_bitmap_blk, inode_bitmap))
	{
		fprintf(stderr, "%s at line %d: Cannot read inode bitmap.\n", __func__, __LINE__);
		return -1;
	}
	// Step 2: Traverse inode bitmap to find an available slot
	for (int i = 0; i < MAX_INUM; i++)
	{
		if (!(get_bitmap(inode_bitmap, i)))
		{
			// Step 3: Update inode block bitmap and write to disk
			set_bitmap(inode_bitmap, i);
			bio_write(superblock->i_bitmap_blk, inode_bitmap);
			return i; // i node number
		}
	}
	return -1;
}

int get_avail_blkno()
{

	// Step 1: Read disk bitmap from disk
	if (!bio_read(superblock->d_bitmap_blk, disk_bitmap))
	{
		fprintf(stderr, "%s at line %d: Cannot read disk bitmap.\n", __func__, __LINE__);
		return -1;
	}
	// Step 2: Traverse disk bitmap to find an available slot
	for (int i = 0; i < MAX_DNUM; i++)
	{
		if (!(get_bitmap(disk_bitmap, i)))
		{
			// Step 3: Update disk block bitmap and write to disk
			set_bitmap(disk_bitmap, i);
			bio_write(superblock->d_bitmap_blk, disk_bitmap);
			return i + superblock->d_start_blk; // disk bitmap number
		}
	}
	return -1;
}

int readi(uint16_t ino, struct inode *inode)
{
	// Step 1: Get the inode's on-disk block number
	int inode_disk_location = superblock->i_start_blk + (ino * sizeof(struct inode)) / BLOCK_SIZE;

	// Step 2: Get offset of the inode in the inode on-disk block
	struct inode *block = malloc(BLOCK_SIZE);
	if (!block)
	{
		return -ENOMEM;
	}
	bio_read(inode_disk_location, block);

	// Step 3: Read the block from disk and then copy into inode structure
	int offset = ino % MAX_INODES_IN_DISK_BLOCK;
	*inode = block[offset];

	free(block);
	return 0;
}

int writei(uint16_t ino, struct inode *inode)
{
	// Step 1: Get the inode's on-disk block number
	int inode_disk_location = superblock->i_start_blk + (ino * sizeof(struct inode)) / BLOCK_SIZE;

	// Step 2: Get offset of the inode in the inode on-disk block
	struct inode *block = malloc(BLOCK_SIZE);
	if (!block)
	{
		return -ENOMEM;
	}
	bio_read(inode_disk_location, block);

	// Step 3: Write the block to disk.
	int offset = ino % MAX_INODES_IN_DISK_BLOCK;
	block[offset] = *inode;

	bio_write(inode_disk_location, block);
	free(block);

	return 0;
}

int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent)
{
	// Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode directory_inode;
	if (readi(ino, &directory_inode) < 0)
	{
		fprintf(stderr, "%s at line %d: Could not find directory inode\n", __func__, __LINE__);
		return -ENOENT;
	}
	struct dirent *found_directory = malloc(BLOCK_SIZE);
	if (!found_directory)
	{
		return -ENOMEM;
	}
	int found_directory_status = false;

	// Step 2: Get data block of current directory from inode
	for (int i = 0; i < ARRAY_SIZE(directory_inode.direct_ptr); i++)
	{
		if (!directory_inode.direct_ptr[i])
		{
			break;
		}
		if (bio_read(directory_inode.direct_ptr[i], found_directory) < 0)
		{
			break;
		}
		for (int j = 0; j < MAX_DIRENTS_IN_DISK_BLOCK; j++)
		{

			// Step 3: Read directory's data block and check each directory entry.
			// If the name matches, then copy directory entry to dirent structure
			if (found_directory[j].valid && strcmp(found_directory[j].name, fname) == 0)
			{
				*dirent = found_directory[j];
				found_directory_status = true;
				return 0;
			}
		}
	}
	free(found_directory);
	return -ENONET;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len)
{
	struct dirent de;

	if (dir_find(dir_inode.ino, fname, name_len, &de) == 0)
	{
		fprintf(stderr, "%s at line %d: file already exists in the directory\n", __func__, __LINE__);
		return -EEXIST;
	}

	struct dirent *found_directory = malloc(BLOCK_SIZE);
	if (!found_directory)
	{
		return -ENOMEM;
	}
	bool found = false;
	for (int i = 0; i < ARRAY_SIZE(dir_inode.direct_ptr); i++)
	{
		int entry_index;
		if (!dir_inode.direct_ptr[i])
		{
			dir_inode.direct_ptr[i] = get_avail_blkno();
			dir_inode.vstat.st_blocks++;
		}

		if (bio_read(dir_inode.direct_ptr[i], found_directory) < 0)
			break;

		for (int j = 0; j < MAX_DIRENTS_IN_DISK_BLOCK; j++)
		{
			if (!found_directory[j].valid)
			{
				found_directory[j].valid = 1;
				found_directory[j].ino = f_ino;
				strcpy(found_directory[j].name, fname);

				// fill up directory inode
				time(&dir_inode.vstat.st_mtime);
				dir_inode.size += sizeof(struct dirent);
				dir_inode.vstat.st_size += sizeof(struct dirent);

				/* Update directory inode */
				writei(dir_inode.ino, &dir_inode);

				/* Write directory entry */
				bio_write(dir_inode.direct_ptr[i], found_directory);
				return 0;
			}
		}
	}
	free(found_directory);
	return -ENOSPC;
}

static int dir_remove(struct inode dir_inode, const char *fname, size_t name_len)
{
}

static int get_node_by_path(const char *path, uint16_t ino, struct inode *inode)
{
	fprintf(stdout, "Inside get_node_by_path: %s\n", path);
	struct dirent de = {0};

	// Special case for root path
	if (strcmp(path, "/") == 0)
	{
		return readi(0, inode);
	}

	char* path_copy = strdup(path);
	if (!path_copy)
	{
		return -ENOMEM;
	}


	char* path_part = strtok(path_copy, "/");

	while (path_part != NULL)
	{
		// Skip empty path parts
		if (*path_part == '\0')
		{
			path_part = strtok(NULL, "/");
			continue;
		}
		fprintf(stdout, "de.ino: %d, path_start: %s\n", de.ino, path_part);
		if (dir_find(de.ino, path_part, strlen(path_part), &de) < 0)
		{
			return -ENOENT;
		}

		path_part = strtok(NULL, "/");
	}
	return readi(de.ino, inode);
}

int rufs_mkfs()
{

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

	// write superblock information
	superblock = malloc(sizeof(struct superblock));
	if (superblock == NULL)
	{
		return -ENOMEM;
	}
	superblock->magic_num = MAGIC_NUM;
	superblock->max_inum = MAX_INUM;
	superblock->max_dnum = MAX_DNUM;
	superblock->i_bitmap_blk = 1;
	superblock->d_bitmap_blk = 2;
	superblock->i_start_blk = 3;
	superblock->d_start_blk = 3 + ((sizeof(struct inode) * MAX_INUM) / BLOCK_SIZE);
	bio_write(0, superblock);

	// initialize inode bitmap
	inode_bitmap = calloc(1, BLOCK_SIZE);

	// initialize data block bitmap
	disk_bitmap = calloc(1, BLOCK_SIZE);

	if (!inode_bitmap || !disk_bitmap)
	{
		return -ENOMEM;
	}
	// update bitmap information for root directory
	set_bitmap(inode_bitmap, 0);
	set_bitmap(disk_bitmap, 0);

	bio_write(superblock->i_bitmap_blk, inode_bitmap);
	bio_write(superblock->d_bitmap_blk, disk_bitmap);

	// update inode for root directory
	struct inode *inode_root = malloc(BLOCK_SIZE);
	if (!inode_root)
	{
		return -ENOMEM;
	}
	inode_root->ino = 0;
	inode_root->valid = 1;
	inode_root->size = sizeof(struct dirent) * 2;
	inode_root->type = DIR_TYPE;
	inode_root->link = 2;
	inode_root->direct_ptr[0] = superblock->d_start_blk;

	for (int i = 1; i < ARRAY_SIZE(inode_root->direct_ptr); i++)
		inode_root->direct_ptr[i] = 0;

	for (int i = 0; i < ARRAY_SIZE(inode_root->indirect_ptr); i++)
		inode_root->indirect_ptr[i] = 0;

	struct stat *stat_root = &inode_root->vstat;
	time_t create_time;

	stat_root->st_ino = 0;
	stat_root->st_atime = time(&create_time);
	stat_root->st_mtime = time(&create_time);
	stat_root->st_mode = S_IFDIR | 0755;
	stat_root->st_nlink = 2;
	stat_root->st_blocks = 1;
	stat_root->st_blksize = BLOCK_SIZE;
	stat_root->st_size = inode_root->size;
	stat_root->st_uid = getuid();
	stat_root->st_gid = getgid();

	struct dirent *root_dirent = malloc(BLOCK_SIZE);
	if (!root_dirent)
	{
		return -ENOMEM;
	}
	root_dirent[0].ino = 0;
	root_dirent[0].len = strlen("..");
	strcpy(root_dirent[0].name, "..");
	root_dirent[0].valid = 1;

	root_dirent[1].ino = 0;
	root_dirent[1].len = strlen(".");
	strcpy(root_dirent[1].name, ".");
	root_dirent[1].valid = 1;

	bio_write(superblock->i_start_blk, inode_root);
	bio_write(superblock->d_start_blk, root_dirent);

	free(inode_root);
	free(root_dirent);

	return 0;
}

/*
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn)
{

	// Step 1a: If disk file is not found, call mkfs
	// Step 1b: If disk file is found, just initialize in-memory data structures
	// and read superblock from disk
	if (dev_open(diskfile_path) < 0)
	{
		fprintf(stdout, "Making file system from scratch\n");
		rufs_mkfs();
		return NULL;
	}
	else
	{
		fprintf(stdout, "Reading existing file system\n");
		superblock = malloc(BLOCK_SIZE);
		inode_bitmap = malloc(BLOCK_SIZE);
		disk_bitmap = malloc(BLOCK_SIZE);

		if (!superblock || !inode_bitmap || !disk_bitmap)
		{
			return NULL;
		}

		bio_read(0, superblock);
		bio_read(superblock->i_bitmap_blk, inode_bitmap);
		bio_read(superblock->d_bitmap_blk, disk_bitmap);

		return NULL;
	}
}

static void rufs_destroy(void *userdata)
{
	// Step 1: De-allocate in-memory data structures
	if (superblock)
	{
		free(superblock);
	}
	if (inode_bitmap)
	{
		free(inode_bitmap);
	}
	if (disk_bitmap)
	{
		free(disk_bitmap);
	}
	// Step 2: Close diskfile
	dev_close();
}

static int rufs_getattr(const char *path, struct stat *stbuf)
{
	fprintf(stdout, "Inside getattr cd command\n");
	struct inode node;

	// Step 1: call get_node_by_path() to get inode from path
	if (get_node_by_path(path, 0, &node) < 0)
		return -ENOENT;

	// Step 2: fill attribute of file into stbuf from inode
	*stbuf = node.vstat;
	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi)
{
	// Step 1: Call get_node_by_path() to get inode from path
	struct inode dir_node;

	// Step 2: If not find, return -1
	return get_node_by_path(path, 0, &dir_node) < 0 ? -1 : 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi)
{
	// Step 1: Call get_node_by_path() to get inode from path
	struct inode directory_inode;
	if (get_node_by_path(path, 0, &directory_inode) < 0)
	{
		return -ENOENT;
	}

	struct dirent *found_directory = malloc(BLOCK_SIZE);
	if (!found_directory)
	{
		return -ENOMEM;
	}
	int found_directory_status = false;

	// Step 2: Get data block of current directory from inode
	for (int i = 0; i < ARRAY_SIZE(directory_inode.direct_ptr); i++)
	{
		if (!directory_inode.direct_ptr[i])
		{
			break;
		}
		if (bio_read(directory_inode.direct_ptr[i], found_directory) < 0)
		{
			break;
		}
		for (int j = 0; j < MAX_DIRENTS_IN_DISK_BLOCK; j++)
		{
			// Step 3: Read directory's data block and check each directory entry.
			// If the name matches, then copy directory entry to dirent structure
			if (found_directory[j].valid)
			{
				struct inode file_in_dir_inode;
				readi(found_directory[j].ino, &file_in_dir_inode);
				filler(buffer, found_directory[j].name, &file_in_dir_inode.vstat, 0);
			}
		}
	}
	free(found_directory);
	return 0;
}

static void init_inode(struct inode *new_file_inode, int avail_inode, int type)
{
	new_file_inode->ino = avail_inode;
	new_file_inode->valid = 1;
	new_file_inode->link = type == REG_TYPE ? 1 : 2;
	new_file_inode->direct_ptr[0] = get_avail_blkno();
	for (int i = 1; i < ARRAY_SIZE(new_file_inode->direct_ptr); i++)
		new_file_inode->direct_ptr[i] = 0;
	for (int i = 0; i < ARRAY_SIZE(new_file_inode->indirect_ptr); i++)
		new_file_inode->indirect_ptr[i] = 0;
	new_file_inode->type = type;
}

static int rufs_mkdir(const char *path, mode_t mode)
{
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	char *path_copy_one = strdup(path);
	char *path_copy_two = strdup(path);

	char *parent_dir = dirname(path_copy_one);
	char *target_dir = basename(path_copy_two);

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode parent_dir_node;
	int status = get_node_by_path(parent_dir, 0, &parent_dir_node);
	if (status < 0)
	{
		fprintf(stdout, "failed to resolve path\n");
		return -1;
	}
	// Step 3: Call get_avail_ino() to get an available inode number
	int avail_inode = get_avail_ino();
	if (avail_inode < 0)
	{
		fprintf(stdout, "failed to get open inode\n");
		return -1;
	}
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	int dir_add_status = dir_add(parent_dir_node, avail_inode, target_dir, strlen(target_dir));
	if (dir_add_status < 0)
	{
		fprintf(stdout, "failed to add directory\n");
		return -1;
	}
	// Step 5: Update inode for target directory
	struct inode new_inode;
	init_inode(&new_inode, avail_inode, DIR_TYPE);
	new_inode.size = sizeof(struct dirent) * 2;

	struct stat *new_stat = &new_inode.vstat;
	time_t create_time;

	new_stat->st_blocks = 1;
	new_stat->st_atime = time(&create_time);
	new_stat->st_mtime = time(&create_time);
	new_stat->st_mode = S_IFDIR | mode;
	new_stat->st_ino = avail_inode;
	new_stat->st_nlink = 2;
	new_stat->st_blksize = BLOCK_SIZE;
	new_stat->st_size = new_inode.size;
	new_stat->st_uid = getuid();
	new_stat->st_gid = getgid();

	// Step 6: Call writei() to write inode to disk
	struct dirent *new_dir = malloc(BLOCK_SIZE);
	if (!new_dir)
	{
		return -ENOMEM;
	}

	// .. -> parent directory
	new_dir[0].ino = parent_dir_node.ino;
	new_dir[0].len = strlen("..");
	strcpy(new_dir[0].name, "..");
	new_dir[0].valid = 1;

	// . -> current directory
	new_dir[1].ino = new_inode.ino;
	new_dir[1].len = strlen(".");
	strcpy(new_dir[1].name, ".");
	new_dir[1].valid = 1;

	writei(avail_inode, &new_inode);
	bio_write(new_inode.direct_ptr[0], new_dir);

	free(new_dir);
	free(path_copy_one);
	free(path_copy_two);

	return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char *path_copy_one = strdup(path);
	char *path_copy_two = strdup(path);

	char *parent_dir = dirname(path_copy_one);
	char *target_dir = basename(path_copy_two);

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode parent_dir_node;
	int status = get_node_by_path(parent_dir, 0, &parent_dir_node);
	if (status < 0)
	{
		fprintf(stdout, "failed to resolve path\n");
		return -1;
	}
	// Step 3: Call get_avail_ino() to get an available inode number
	int avail_inode = get_avail_ino();
	if (avail_inode < 0)
	{
		fprintf(stdout, "failed to get open inode\n");
		return -1;
	}
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	int dir_add_status = dir_add(parent_dir_node, avail_inode, target_dir, strlen(target_dir));
	if (dir_add_status < 0)
	{
		fprintf(stdout, "failed to add directory\n");
		return -1;
	}

	// Step 5: Update inode for target file
	struct inode new_inode;
	init_inode(&new_inode, avail_inode, REG_TYPE);
	new_inode.size = 0;

	struct stat *new_stat = &new_inode.vstat;
	time_t create_time;

	new_stat->st_blocks = 1;
	new_stat->st_atime = time(&create_time);
	new_stat->st_mtime = time(&create_time);
	new_stat->st_mode = S_IFREG | mode;
	new_stat->st_ino = avail_inode;
	new_stat->st_nlink = 1;
	new_stat->st_blksize = BLOCK_SIZE;
	new_stat->st_size = new_inode.size;
	new_stat->st_uid = getuid();
	new_stat->st_gid = getgid();

	// Step 6: Call writei() to write inode to disk
	writei(avail_inode, &new_inode);
	free(path_copy_one);
	free(path_copy_two);

	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi)
{
	struct inode file_node;
	return get_node_by_path(path, 0, &file_node) < 0 ? -1 : 0;
}

static int rufs_read(const char *path, char *buffer, size_t size,
					 off_t offset, struct fuse_file_info *fi)
{
}

static int rufs_write(const char *path, const char *buffer, size_t size,
					  off_t offset, struct fuse_file_info *fi)
{
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
}

static void release_data_blocks(struct inode *target)
{
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
}
static int rufs_rmdir(const char *path)
{
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
}
static int rufs_unlink(const char *path)
{
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
}
static int rufs_truncate(const char *path, off_t size)
{
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi)
{
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char *path, struct fuse_file_info *fi)
{
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2])
{
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi)
{
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static struct fuse_operations rufs_ope = {
	.init = rufs_init,
	.destroy = rufs_destroy,

	.getattr = rufs_getattr,
	.readdir = rufs_readdir,
	.opendir = rufs_opendir,
	.releasedir = rufs_releasedir,
	.mkdir = rufs_mkdir,
	.rmdir = rufs_rmdir,

	.create = rufs_create,
	.open = rufs_open,
	.read = rufs_read,
	.write = rufs_write,
	.unlink = rufs_unlink,

	.truncate = rufs_truncate,
	.flush = rufs_flush,
	.utimens = rufs_utimens,
	.release = rufs_release};

int main(int argc, char *argv[])
{
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}
