#include <stdio.h>
#include "ext2_fs.h"
#include "read_ext2.h"
#include <sys/types.h>
#include <dirent.h>

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("expected usage: ./runscan inputfile outputfile\n");
		exit(0);
	}

	DIR *dir = opendir(argv[2]);
	if (dir == NULL)
	{
		printf("error: could not open directory %s\n", argv[2]);
		exit(0);
	}

	int fd;

	fd = open(argv[1], O_RDONLY); /* open disk image */

	ext2_read_init(fd);

	struct ext2_super_block super;
	struct ext2_group_desc group;

	// example read first the super-block and group-descriptor
	read_super_block(fd, 0, &super);
	read_group_desc(fd, 0, &group);

	printf("There are %u inodes in an inode table block and %u blocks in the idnode table\n", inodes_per_block, itable_blocks);
	// iterate the first inode block
	off_t start_inode_table = locate_inode_table(0, &group);

	// for each inode block (160 blocks) in block group 
	for (unsigned int j = 0; j < itable_blocks; j++)
	{
		// for each inode (8 inodes) in block 
		// get first data block of inode and check if is jpg
		for (unsigned int k = 0; k < inodes_per_block; k++) {
			struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
			// uint location = start_inode_table + (j * inodes_per_block * sizeof(struct ext2_inode)) + (k * sizeof(struct ext2_inode));
			int inode_no = (j * inodes_per_block) + k;
			read_inode(fd, 0, start_inode_table, inode_no, inode);

			// printf("Unused : %u\n", !inode->i_block[0]);

			if (S_ISREG(inode->i_mode))
			{
				// build buffer
				char buffer[1024];

				lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
				read(fd, buffer, 1024);

				// check if jpg
				int is_jpg = 0;
				if (buffer[0] == (char)0xff &&
					buffer[1] == (char)0xd8 &&
					buffer[2] == (char)0xff &&
					(buffer[3] == (char)0xe0 ||
					 buffer[3] == (char)0xe1 ||
					 buffer[3] == (char)0xe8))
				{
					is_jpg = 1;
				}
				printf("is jpg %u\n", is_jpg);
				printf("in ode %u\n", inode_no);
			}
			free(inode);
		}
	}
	
	

	// for (unsigned int i = 0; i < inodes_per_block; i++)
	// {
	// 	printf("inode %u: \n", i);
	// 	struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
	// 	read_inode(fd, 0, start_inode_table, i, inode);
	// 	/* the maximum index of the i_block array should be computed from i_blocks / ((1024<<s_log_block_size)/512)
	// 	 * or once simplified, i_blocks/(2<<s_log_block_size)
	// 	 * https://www.nongnu.org/ext2-doc/ext2.html#i-blocks
	// 	 */
	// 	unsigned int i_blocks = inode->i_blocks / (2 << super.s_log_block_size);
	// 	printf("number of blocks %u\n", i_blocks);
	// 	printf("is directory %u\n", S_ISDIR(inode->i_mode));
	// 	printf("is regular file %u\n", S_ISREG(inode->i_mode));
	// 	if (S_ISREG(inode->i_mode))
	// 	{
			// build buffer
			// char buffer[1024];

			// check if jpg
			// int is_jpg = 0;
			// if (buffer[0] == (char)0xff &&
			// 	buffer[1] == (char)0xd8 &&
			// 	buffer[2] == (char)0xff &&
			// 	(buffer[3] == (char)0xe0 ||
			// 	 buffer[3] == (char)0xe1 ||
			// 	 buffer[3] == (char)0xe8))
			// {
			// 	is_jpg = 1;
			// }
			// printf("is jpg %u\n", is_jpg);
		// }
		// printf("Unused : %u\n", !inode->i_block[0]);
		// print i_block numbers
		// for (unsigned int i = 0; i < EXT2_N_BLOCKS; i++)
		// {
		// 	if (i < EXT2_NDIR_BLOCKS) /* direct blocks */
		// 		printf("Block %2u : %u\n", i, inode->i_block[i]);
		// 	else if (i == EXT2_IND_BLOCK) /* single indirect block */
		// 		printf("Single   : %u\n", inode->i_block[i]);
		// 	else if (i == EXT2_DIND_BLOCK) /* double indirect block */
		// 		printf("Double   : %u\n", inode->i_block[i]);
		// 	else if (i == EXT2_TIND_BLOCK) /* triple indirect block */
		// 		printf("Triple   : %u\n", inode->i_block[i]);
		// }

		// free(inode);
	// }

	close(fd);
}
