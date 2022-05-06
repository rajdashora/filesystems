#include <stdio.h>
#include "ext2_fs.h"
#include "read_ext2.h"
#include <sys/types.h>
#include <dirent.h>
#include <math.h>
#include <string.h>

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
		mkdir(argv[2], S_IRWXU);
	}
	else
	{
		printf("Directory already exists.\n");
		exit(0);
	}

	int fd;
	struct LLNode
	{
		uint inode_idx;
		struct LLNode *next;
	};
	struct LLNode *head = NULL;
	struct LLNode *curr = NULL;
	struct LLNode *tail = NULL;
	fd = open(argv[1], O_RDONLY); /* open disk image */

	ext2_read_init(fd);

	struct ext2_super_block super;
	struct ext2_group_desc group;
	for (uint g = 0; g < num_groups; g++)
	{

		// example read first the super-block and group-descriptor
		read_super_block(fd, g, &super);
		read_group_desc(fd, g, &group);

		printf("There are %u inodes in an inode table block and %u blocks in the idnode table\n", inodes_per_block, itable_blocks);
		// iterate the first inode block
		off_t start_inode_table = locate_inode_table(g, &group);
		// for each inode block (160 blocks) in block group
		for (unsigned int j = 0; j < itable_blocks; j++)
		{
			// for each inode (8 inodes) in block
			// get first data block of inode and check if is jpg
			for (unsigned int k = 0; k < inodes_per_block; k++)
			{
				struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
				// uint location = start_inode_table + (j * inodes_per_block * sizeof(struct ext2_inode)) + (k * sizeof(struct ext2_inode));
				int inode_idx = (j * inodes_per_block) + k;
				read_inode(fd, g, start_inode_table, inode_idx, inode);

				if (S_ISREG(inode->i_mode))
				{
					// build buffer
					char buffer[1024];

					lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
					read(fd, buffer, sizeof(buffer));

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
					// printf("is jpg %u\n", is_jpg);
					// printf("inode index %u\n", inode_idx);
					char pathname[500];
					sprintf(pathname, "%s/file-%d.jpg", argv[2], inode_idx);
					// unsigned int last_i_block = ceil(file_size/block_size);
					if (is_jpg == 1)
					{

						int ff = open(pathname, O_WRONLY | O_TRUNC | O_CREAT, 0666);
						uint curr_block_idx = 0;
						int file_size = inode->i_size;
						uint final_data_block_idx = floor(file_size / 1024);
						int done = 0;
						// printf("fsize: %d final data block idx: %d \n", file_size, final_data_block_idx);

						if (head == NULL)
						{
							head = (struct LLNode *)malloc(sizeof(struct LLNode));
							curr = head;
							tail = head;
						}
						else
						{
							curr->next = (struct LLNode *)malloc(sizeof(struct LLNode));
							curr = curr->next;
							tail = curr;
						}
						curr->inode_idx = inode_idx;

						for (unsigned int i = 0; i < EXT2_N_BLOCKS; i++)
						{

							/* single indirect block */
							if (i == EXT2_IND_BLOCK)
							{ // get block number 2000
								uint single_buffer[256];
								lseek(fd, BLOCK_OFFSET(inode->i_block[i]), SEEK_SET);
								read(fd, single_buffer, 1024);

								for (unsigned int l = 0; l < 256; l++)
								{
									lseek(fd, BLOCK_OFFSET(single_buffer[l]), SEEK_SET);
									read(fd, buffer, sizeof(buffer));
									if (curr_block_idx == final_data_block_idx)
									{
										size_t v = (size_t)(file_size - (final_data_block_idx * block_size));
										write(ff, buffer, v);
										curr_block_idx++;
										done = 1;
										break;
									}
									else
									{
										write(ff, buffer, sizeof(buffer));
										curr_block_idx++;
									}
								}
								if (done)
									break;
							}
							/* double indirect block */
							else if (i == EXT2_DIND_BLOCK)
							{
								uint double_buffer[256];
								lseek(fd, BLOCK_OFFSET(inode->i_block[i]), SEEK_SET);
								read(fd, double_buffer, 1024);

								for (unsigned int l = 0; l < 256; l++)
								{
									uint single_buffer[256];
									lseek(fd, BLOCK_OFFSET(double_buffer[l]), SEEK_SET);
									read(fd, single_buffer, 1024);

									for (unsigned int m = 0; m < 256; m++)
									{
										lseek(fd, BLOCK_OFFSET(single_buffer[m]), SEEK_SET);
										read(fd, buffer, sizeof(buffer));
										if (curr_block_idx == final_data_block_idx)
										{
											size_t v = (size_t)(file_size - (final_data_block_idx * block_size));
											write(ff, buffer, v);
											curr_block_idx++;
											done = 1;
											break;
										}
										else
										{
											write(ff, buffer, sizeof(buffer));
											curr_block_idx++;
										}
									}
									if (done)
										break;
								}
								if (done)
									break;
							}
							else
							{
								lseek(fd, BLOCK_OFFSET(inode->i_block[i]), SEEK_SET);
								read(fd, buffer, sizeof(buffer));
								if (curr_block_idx == final_data_block_idx)
								{
									size_t v = (size_t)(file_size - (final_data_block_idx * block_size));
									write(ff, buffer, v);
									curr_block_idx++;
									break;
								}
								else
								{
									write(ff, buffer, sizeof(buffer));
									curr_block_idx++;
								}
							}
						}
					}
				}
				free(inode);
			}
		}
		curr = head; // reset curr back to head

		// scan through again, this time just want directories
		for (unsigned int j = 0; j < itable_blocks; j++)
		{
			for (unsigned int k = 0; k < inodes_per_block; k++)
			{
				struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
				int inode_idx = (j * inodes_per_block) + k;
				read_inode(fd, g, start_inode_table, inode_idx, inode);
				// if is directory
				if (S_ISDIR(inode->i_mode))
				{
					char buffer[1024];
					lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
					read(fd, buffer, sizeof(buffer));
					// printf("inode %d is a dir\n", j * inodes_per_block + k);
					uint ptr = 0;
					while (1)
					{
						// printf("ptr: %d\n", ptr);
						// int found = 0;
						struct ext2_dir_entry_2 *dentry = (struct ext2_dir_entry_2 *)&(buffer[ptr]);
						int name_len = dentry->name_len & 0xFF; // convert 2 bytes to 4 bytes properly
						char name[EXT2_NAME_LEN];
						strncpy(name, dentry->name, name_len);
						// printf("og len: %d\n", name_len);
						name[name_len] = '\0';
						if (name_len % 4 != 0)
							name_len += 4 - (name_len % 4);
						// printf("new len: %d\n", name_len);

						// navigate through list
						while (1)
						{
							// if data found
							if (dentry->inode == curr->inode_idx)
							{
								char command_name[500];
								char command_name2[500];
								sprintf(command_name, "cp %s/file-%d.jpg \"%s/%s\"", argv[2], curr->inode_idx, argv[2], name);
								uint c2 = 0;
								for (unsigned int c = 0; c < sizeof(command_name); c++)
								{
									if (command_name[c] =='$'){
										command_name2[c2] = '\\';
										c2++;
									}
									command_name2[c2] = command_name[c];
									c2++;
								}
								system(command_name2);
								break;
							}

							if (curr == tail)
								break;
							curr = curr->next;
						}
						curr = head;
						ptr += name_len + 8;
						if (ptr >= block_size)
							break;
					}
				}
			}
		}
	}
	close(fd);
}
