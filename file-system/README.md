# CS416 Project 4: File System
Rohan Deshpande (ryd4) Jinyue (Eric) Liu (jl2661).

# Documentation

## Data Structures
```c
struct superblock {
	uint32_t	magic_num;			/* magic number */
	uint16_t	max_inum;			/* maximum inode number */
	uint16_t	max_dnum;			/* maximum data block number */
	uint32_t	i_bitmap_blk;		/* start block of inode bitmap */
	uint32_t	d_bitmap_blk;		/* start block of data block bitmap */
	uint32_t	i_start_blk;		/* start block of inode region */
	uint32_t	d_start_blk;		/* start block of data block region */
};

struct inode {
	uint16_t	ino;				/* inode number */
	uint16_t	valid;				/* validity of the inode */
	uint32_t	size;				/* size of the file */
	uint32_t	type;				/* type of the file */
	uint32_t	link;				/* link count */
	int			direct_ptr[16];		/* direct pointer to data block */
	int			indirect_ptr[8];	/* indirect pointer to data block */
	struct stat	vstat;				/* inode stat */
};

struct dirent {
	uint16_t ino;					/* inode number of the directory entry */
	uint16_t valid;					/* validity of the directory entry */
	char name[208];					/* name of the directory entry */
	uint16_t len;					/* length of name */
};
```

```c
bitmap_t inode_bitmap; /* Each bit in bitmap represents if a inode is free or not*/

bitmap_t disk_bitmap; /* Each bit in bitmap represents if a data block on disk is free or not*/
```
## Functions

```c
int get_avail_ino()
```
Gets the first free bit from the inode bitmap.

1. Read the Inode bitmap from disk.
2. Traverse through the bitmap to find the unset bit. Once found,
we set the bit.
3. Write the Inode bitmap back to disk.

-----
```c
int get_avail_ino()
```
Gets the free bit from the disk bitmap.

1. Read the Disk bitmap from disk.
2. Traverse through the bitmap to find the unset bit. Once found,
we set the bit.
3. Write the Disk bitmap back to disk.

-----
```c
int readi(uint16_t ino, struct inode *inode)
```
Read the data to ```inode``` from disk located at ```ino``` index.

1. Get the inode's on-disk block number
2. Get offset of the inode in the inode on-disk block
3. Read the block from disk and then copy into inode structure
-----
```c
int writei(uint16_t ino, struct inode *inode)
```
Write the data to ```inode``` from disk located at ```ino``` index.

1. Get the inode's on-disk block number
2. Get offset of the inode in the inode on-disk block
3. Write the block to disk.
-----
```c
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent)
```
1. Call readi() to get the inode using ino (inode number of current directory).
2. Get data block of current directory from inode.
3. Read directory's data block and check each directory entry. If the name matches, then copy directory entry to dirent structure.
-----
```c
int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len)
```
1. Call dir_find first to check for duplicate file.
2. Find an empty dirent in the data block of directory inode
3. Wite a new directory entry with the given inode number and name
in the current directory’s data blocks.
-----
```c
static int get_node_by_path(const char *path, uint16_t ino, struct inode *inode)
```
1. Implemented it iteratively.
2. Split the path based on "/" to get individual directories.
3. Call ```dir_find``` to find inode for each directory.
4. Finally read the inode data into the input inode pointer.
-----
```c
int rufs_mkfs()
```
1. Creates the file system and setup the data structures mentioned in the Data Structure section of this document.
Also creates the DISKFILE to persist the file system.
----
```c
static void *rufs_init(struct fuse_conn_info *conn)
```
1. If there is no DISKFILE then create it using ```rufs_mkfs``` or else just read the 
data structures from DISKFILE.
-----
```c
static void rufs_destroy(void *userdata)
```
1. Free the core data structures of the application and close the DISKFILE file descriptor.
-----
```c
static int rufs_getattr(const char *path, struct stat *stbuf)
```
1. This is the cd command too.
2. Get's the path's inode object first. Fron the inode object, we read the vstat field into the stbuf.
-----
```c
static int rufs_opendir(const char *path, struct fuse_file_info *fi)
```
1. This is the cd command.
2. Tries to open the directory using get_node_by_path. If not exist then return -1.
-----
```c
static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi)
```
1. This is the ls command.
2. Read the inode and see if this path is valid, read all directory entries
of the current directory into the input buffer. 
-----
```c
static int rufs_mkdir(const char *path, mode_t mode)
```
This is the mkdir command. Creates a directory.
1. Use dirname() and basename() to separate parent directory path and target directory name
2. Call get_node_by_path() to get inode of parent directory
3. Call get_avail_ino() to get an available inode number
4. Call dir_add() to add directory entry of target directory to parent directory
5. Update inode for target directory
6. Call writei() to write inode to disk
-----
```c
static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
```
This is the touch command. Create a regular file.
1. Use dirname() and basename() to separate parent directory path and target directory name
2. Call get_node_by_path() to get inode of parent directory
3. Call get_avail_ino() to get an available inode number
4. Call dir_add() to add directory entry of target directory to parent directory
5. Update inode for target directory
6. Call writei() to write inode to disk
-----
```c
static int rufs_open(const char *path, struct fuse_file_info *fi)
```
1. Tries to open the file given by path else returns -1

## Problems

Due to time constraint, we could not implement the read and write functions. However,
we were able to implement the rest of the functions successfully.

## Compile

1. Run ```make``` to generate rufs binary.
2. cd into benchmark directory and run ```make```
3. Finally run the tests in the benchmark