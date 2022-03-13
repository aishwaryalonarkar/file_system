#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"
#include "fs_util.h"
#include "disk.h"

char inodeMap[MAX_INODE / 8];
char blockMap[MAX_BLOCK / 8];
Inode inode[MAX_INODE];
SuperBlock superBlock;
Dentry curDir;
int curDirBlock;

int fs_mount(char *name)
{
	int numInodeBlock = (sizeof(Inode) * MAX_INODE) / BLOCK_SIZE;
	int i, index, inode_index = 0;

	// load superblock, inodeMap, blockMap and inodes into the memory
	printf("%d", disk_mount(name));
	if (disk_mount(name) == 1)
	{
		disk_read(0, (char *)&superBlock);
		if (superBlock.magicNumber != MAGIC_NUMBER)
		{
			printf("Invalid disk!\n");
			exit(0);
		}
		disk_read(1, inodeMap);
		disk_read(2, blockMap);
		for (i = 0; i < numInodeBlock; i++)
		{
			index = i + 3;
			disk_read(index, (char *)(inode + inode_index));
			inode_index += (BLOCK_SIZE / sizeof(Inode));
		}
		// root directory
		curDirBlock = inode[0].directBlock[0];
		// printf("%d Cur Dir Block = \n",curDirBlock);
		disk_read(curDirBlock, (char *)&curDir);
	}
	else
	{
		// Init file system superblock, inodeMap and blockMap
		superBlock.magicNumber = MAGIC_NUMBER;
		superBlock.freeBlockCount = MAX_BLOCK - (1 + 1 + 1 + numInodeBlock);
		superBlock.freeInodeCount = MAX_INODE;

		//Init inodeMap
		for (i = 0; i < MAX_INODE / 8; i++)
		{
			set_bit(inodeMap, i, 0);
		}
		//Init blockMap
		for (i = 0; i < MAX_BLOCK / 8; i++)
		{
			if (i < (1 + 1 + 1 + numInodeBlock))
				set_bit(blockMap, i, 1);
			else
				set_bit(blockMap, i, 0);
		}
		//Init root dir
		int rootInode = get_free_inode();
		curDirBlock = get_free_block();

		inode[rootInode].type = directory;
		inode[rootInode].owner = 0;
		inode[rootInode].group = 0;
		gettimeofday(&(inode[rootInode].created), NULL);
		gettimeofday(&(inode[rootInode].lastAccess), NULL);
		inode[rootInode].size = 1;
		inode[rootInode].blockCount = 1;
		inode[rootInode].directBlock[0] = curDirBlock;

		curDir.numEntry = 1;
		strncpy(curDir.dentry[0].name, ".", 1);
		curDir.dentry[0].name[1] = '\0';
		curDir.dentry[0].inode = rootInode;
		// printf("%d Cur Dir Block = \n",curDirBlock);

		disk_write(curDirBlock, (char *)&curDir);
	}
	return 0;
}

int fs_umount(char *name)
{
	int numInodeBlock = (sizeof(Inode) * MAX_INODE) / BLOCK_SIZE;
	int i, index, inode_index = 0;
	disk_write(0, (char *)&superBlock);
	disk_write(1, inodeMap);
	disk_write(2, blockMap);
	for (i = 0; i < numInodeBlock; i++)
	{
		index = i + 3;
		disk_write(index, (char *)(inode + inode_index));
		inode_index += (BLOCK_SIZE / sizeof(Inode));
	}
	// current directory
	disk_write(curDirBlock, (char *)&curDir);

	disk_umount(name);
}

int search_cur_dir(char *name)
{
	// return inode. If not exist, return -1
	int i;

	for (i = 0; i < curDir.numEntry; i++)
	{
		// printf(" cur dentry %d \n",curDir.dentry[i].inode);
		if (command(name, curDir.dentry[i].name))
			return curDir.dentry[i].inode;
	}
	return -1;
}

int file_create(char *name, int size)
{
	int i;

	if (size > SMALL_FILE)
	{
		printf("Do not support files larger than %d bytes.\n", SMALL_FILE);
		return -1;
	}

	if (size < 0)
	{
		printf("File create failed: cannot have negative size\n");
		return -1;
	}

	int inodeNum = search_cur_dir(name);
	// printf("")
	if (inodeNum >= 0)
	{
		printf("File create failed:  %s exist.\n", name);
		return -1;
	}

	if (curDir.numEntry + 1 > MAX_DIR_ENTRY)
	{
		printf("File create failed: directory is full!\n");
		return -1;
	}

	int numBlock = size / BLOCK_SIZE;
	if (size % BLOCK_SIZE > 0)
		numBlock++;

	if (numBlock > superBlock.freeBlockCount)
	{
		printf("File create failed: data block is full!\n");
		return -1;
	}

	if (superBlock.freeInodeCount < 1)
	{
		printf("File create failed: inode is full!\n");
		return -1;
	}

	char *tmp = (char *)malloc(sizeof(int) * size + 1);

	rand_string(tmp, size);
	printf("New File: %s\n", tmp);

	// get inode and fill it
	inodeNum = get_free_inode();
	if (inodeNum < 0)
	{
		printf("File_create error: not enough inode.\n");
		return -1;
	}

	inode[inodeNum].type = file;
	inode[inodeNum].owner = 1; // pre-defined
	inode[inodeNum].group = 2; // pre-defined
	gettimeofday(&(inode[inodeNum].created), NULL);
	gettimeofday(&(inode[inodeNum].lastAccess), NULL);
	inode[inodeNum].size = size;
	inode[inodeNum].blockCount = numBlock;
	inode[inodeNum].link_count = 1;

	// add a new file into the current directory entry
	strncpy(curDir.dentry[curDir.numEntry].name, name, strlen(name));
	// printf("Cur Dir = %s \n", curDir.dentry[curDir.numEntry].name);
	curDir.dentry[curDir.numEntry].name[strlen(name)] = '\0';
	curDir.dentry[curDir.numEntry].inode = inodeNum;
	// printf("curdir %s, name %s\n", curDir.dentry[curDir.numEntry].name, name);
	curDir.numEntry++;

	// get data blocks
	for (i = 0; i < numBlock; i++)
	{
		int block = get_free_block();
		if (block == -1)
		{
			printf("File_create error: get_free_block failed\n");
			return -1;
		}
		//set direct block
		inode[inodeNum].directBlock[i] = block;

		disk_write(block, tmp + (i * BLOCK_SIZE));
	}

	//update last access of current directory
	gettimeofday(&(inode[curDir.dentry[0].inode].lastAccess), NULL);

	printf("file created: %s, inode %d, size %d\n", name, inodeNum, size);

	free(tmp);
	return 0;
}

int file_cat(char *name)
{
	int inodeNum, i, size;
	char str_buffer[512];
	char *str;

	//get inode
	inodeNum = search_cur_dir(name);
	size = inode[inodeNum].size;

	//check if valid input
	if (inodeNum < 0)
	{
		printf("cat error: file not found\n");
		return -1;
	}
	if (inode[inodeNum].type == directory)
	{
		printf("cat error: cannot read directory\n");
		return -1;
	}

	//allocate str
	str = (char *)malloc(sizeof(char) * (size + 1));
	str[size] = '\0';

	for (i = 0; i < inode[inodeNum].blockCount; i++)
	{
		int block;
		block = inode[inodeNum].directBlock[i];

		disk_read(block, str_buffer);

		if (size >= BLOCK_SIZE)
		{
			memcpy(str + i * BLOCK_SIZE, str_buffer, BLOCK_SIZE);
			size -= BLOCK_SIZE;
					// printf("str=%s\n---------ooo000000000000-----\n",str);

		}
		else
		{
			memcpy(str + i * BLOCK_SIZE, str_buffer, size);

		}

	}
	printf("%s\n", str);

	//update lastAccess
	gettimeofday(&(inode[inodeNum].lastAccess), NULL);

	free(str);

	//return success
	return 0;
}

int file_read(char *name, int offset, int size)
{
	int inodeNum, i;
	char str_buffer[512]={0};
	char *str;
	char *str_offset;

	//get inode
	inodeNum = search_cur_dir(name);
	int file_size = inode[inodeNum].size;

	//check if valid input
	if(inodeNum<0)
	{
		printf("File read error: File Not Found\n");
		return -1;
	}
	if (size > file_size)
	{
		printf("File read error: Input size Invalid\n");
		return -1;
	}
	if (offset >= file_size)
	{
		printf("File read error: Invalid Offset\n");
		return -1;
	}
	if (size + offset > file_size)
	{
		printf("File read error: Size and Offset range invalid\n");
		return -1;
	}
	if (size <= 0)
	{
		printf("File read error: Invalid Size\n");
		return -1;
	}
	if (inodeNum < 0)
	{
		printf("File read error: file not found\n");
		return -1;
	}
	if (inode[inodeNum].type == directory)
	{
		printf("File read error: cannot read directory\n");
		return -1;
	}

	//allocate str
	str = (char *)malloc(sizeof(char) * (file_size+ 1));
	str[file_size] = '\0';
	str_offset = (char *)malloc(sizeof(char) * (file_size + 1));
	str_offset[file_size] = '\0';
	memset(str_offset,0,file_size+1);
	memset(str,0,file_size+1);

	int offset_starts_here=0;
	if(offset>BLOCK_SIZE)
	{
		for (i = 0; i < inode[inodeNum].blockCount; i++)
		{
			if(offset<BLOCK_SIZE*(i+1))
			{
				offset_starts_here=i;
				offset=BLOCK_SIZE*(i+1)-offset;
				offset=BLOCK_SIZE-offset;
				break;
			}
		}
	}
	int counter = 0;

	
	for (i = offset_starts_here; i < inode[inodeNum].blockCount; i++)
	{
		int block;
		block = inode[inodeNum].directBlock[i];

		disk_read(block, str_buffer);
		// printf("offset_starts_here=%d offset=%d,size=%d,offset+size=%d,i=%d\n",offset_starts_here,offset,size,offset+size,i);
		
		if(size+offset<=BLOCK_SIZE && i==offset_starts_here)
		{
			// disk_read(block, str_buffer-offset);
			// memcpy(str + i * BLOCK_SIZE, str_buffer, size);
			// printf("1\n");
			// printf("1, offset_starts_here=%d offset=%d,size=%d,offset+size=%d\n",offset_starts_here,offset,size,offset+size);
			// disk_read(block, str_buffer);
			// memcpy(str + i * BLOCK_SIZE, str_buffer+offset, size);
			memcpy(str, str_buffer+offset, size);
			break;
		}
		else
		{
			if(i==offset_starts_here)
			{
				// printf("2\n");
				// printf("1, offset_starts_here=%d offset=%d,size=%d,offset+size=%d\n",offset_starts_here,offset,size,offset+size);
				// disk_read(block, str_buffer);
				// memcpy(str + i * BLOCK_SIZE, str_buffer+offset, size);
				memcpy(str_offset, str_buffer+offset, BLOCK_SIZE-offset);
				// printf("str_offset=%s\n--------------\n",str_offset);
				size=size-(BLOCK_SIZE-offset);
				// printf("size left=%d\n--------------\n",size);
				// printf("len=%lu\n",strlen(str_offset));

			}
			if(i>offset_starts_here)
			{
				disk_read(block,str_buffer);
				if (size >= BLOCK_SIZE)
				{
					// printf("3\n");
					// printf("2, offset=%d,size=%d,offset+size=%d\n",offset,size,offset+size);
					//memcpy(str + i * BLOCK_SIZE, str_buffer, BLOCK_SIZE);
					memcpy(str+counter*BLOCK_SIZE, str_buffer, BLOCK_SIZE);
					// memmove(str + counter*BLOCK_SIZE, str_buffer, BLOCK_SIZE);
					size -= BLOCK_SIZE;	
					counter ++;
					// printf("str=%s\n---------ooo000000000000-----\n",str);

				}
				
				else
				{
					// printf("4\n");
					// printf("3, offset=%d,size=%d,offset+size=%d\n",offset,size,offset+size);
					//memcpy(str + i * BLOCK_SIZE, str_buffer, size);
					memcpy(str+counter*BLOCK_SIZE, str_buffer, size);
					// memmove(str + counter*BLOCK_SIZE, str_buffer, size);
					// printf("str=%s\n---------ooo000000000000-----\n",str);
				}
			}
		}

	}
	strcat(str_offset,str);
	printf("%s\n", str_offset);
	printf("len=%lu\n",strlen(str_offset));
	//update lastAccess
	gettimeofday(&(inode[inodeNum].lastAccess), NULL);

	free(str);
	free(str_offset);
	//return success
	return 0;
}

int file_read1(char *name, int offset, int size)
{
	int inodeNum, i;
	char str_buffer[512]={0};
	char *str;
	char *str_offset;

	//get inode
	inodeNum = search_cur_dir(name);
	int file_size = inode[inodeNum].size;

	//check if valid input
	if(size>file_size)
	{
		printf("File read error: Input Size out of range\n");
		return -1;
	}
	if(offset+size>file_size)
	{
		printf("File read error: Input Offset and Size range out of range\n");
		return -1;
	}
	if (inodeNum < 0)
	{
		printf("File read error: file not found\n");
		return -1;
	}
	if (inode[inodeNum].type == directory)
	{
		printf("File read error: Is a Directory\n");
		return -1;
	}

	//allocate str
	str = (char *)malloc(sizeof(char) * (file_size+ 1));
	str[file_size] = '\0';
	str_offset = (char *)malloc(sizeof(char) * (file_size + 1));
	str_offset[file_size] = '\0';
	memset(str_offset,0,file_size+1);
	memset(str,0,file_size+1);

	int offset_starts_here=0;
	if(offset>BLOCK_SIZE)
	{
		for (i = 0; i < inode[inodeNum].blockCount; i++)
		{
			if(offset<BLOCK_SIZE*(i+1))
			{
				offset_starts_here=i;
				offset=BLOCK_SIZE*(i+1)-offset;
				offset=BLOCK_SIZE-offset;
				break;
			}
		}
	}
	int counter = 0;

	
	for (i = offset_starts_here; i < inode[inodeNum].blockCount; i++)
	{
		int block;
		block = inode[inodeNum].directBlock[i];

		disk_read(block, str_buffer);
		// printf("offset_starts_here=%d offset=%d,size=%d,offset+size=%d,i=%d\n",offset_starts_here,offset,size,offset+size,i);
		
		if(size+offset<=BLOCK_SIZE && i==offset_starts_here)
		{
			// disk_read(block, str_buffer-offset);
			// memcpy(str + i * BLOCK_SIZE, str_buffer, size);
			// printf("1\n");
			// printf("1, offset_starts_here=%d offset=%d,size=%d,offset+size=%d\n",offset_starts_here,offset,size,offset+size);
			// disk_read(block, str_buffer);
			// memcpy(str + i * BLOCK_SIZE, str_buffer+offset, size);
			memcpy(str, str_buffer+offset, size);
			break;
		}
		else
		{
			if(i==offset_starts_here)
			{
				// printf("2\n");
				// printf("1, offset_starts_here=%d offset=%d,size=%d,offset+size=%d\n",offset_starts_here,offset,size,offset+size);
				// disk_read(block, str_buffer);
				// memcpy(str + i * BLOCK_SIZE, str_buffer+offset, size);
				memcpy(str_offset, str_buffer+offset, BLOCK_SIZE-offset);
				// printf("str_offset=%s\n--------------\n",str_offset);
				size=size-(BLOCK_SIZE-offset);
				// printf("size left=%d\n--------------\n",size);
				// printf("len=%lu\n",strlen(str_offset));

			}
			if(i>offset_starts_here)
			{
				disk_read(block,str_buffer);
				if (size >= BLOCK_SIZE)
				{
					// printf("3\n");
					// printf("2, offset=%d,size=%d,offset+size=%d\n",offset,size,offset+size);
					//memcpy(str + i * BLOCK_SIZE, str_buffer, BLOCK_SIZE);
					memcpy(str+counter*BLOCK_SIZE, str_buffer, BLOCK_SIZE);
					// memmove(str + counter*BLOCK_SIZE, str_buffer, BLOCK_SIZE);
					size -= BLOCK_SIZE;	
					counter ++;
					// printf("str=%s\n---------ooo000000000000-----\n",str);

				}
				
				else
				{
					// printf("4\n");
					// printf("3, offset=%d,size=%d,offset+size=%d\n",offset,size,offset+size);
					//memcpy(str + i * BLOCK_SIZE, str_buffer, size);
					memcpy(str+counter*BLOCK_SIZE, str_buffer, size);
					// memmove(str + counter*BLOCK_SIZE, str_buffer, size);
					// printf("str=%s\n---------ooo000000000000-----\n",str);
				}
			}
		}

	}
	strcat(str_offset,str);
	printf("%s\n", str_offset);
	printf("len=%lu\n",strlen(str_offset));
	//update lastAccess
	gettimeofday(&(inode[inodeNum].lastAccess), NULL);

	free(str);
	free(str_offset);
	//return success
	return 0;
}

int file_stat(char *name)
{
	char timebuf[28];
	int inodeNum = search_cur_dir(name);
	if (inodeNum < 0)
	{
		printf("file cat error: file is not exist.\n");
		return -1;
	}

	printf("Inode\t\t= %d\n", inodeNum);
	if (inode[inodeNum].type == file)
		printf("type\t\t= File\n");
	else
		printf("type\t\t= Directory\n");
	printf("owner\t\t= %d\n", inode[inodeNum].owner);
	printf("group\t\t= %d\n", inode[inodeNum].group);
	printf("size\t\t= %d\n", inode[inodeNum].size);
	printf("link_count\t= %d\n", inode[inodeNum].link_count);
	printf("num of block\t= %d\n", inode[inodeNum].blockCount);
	format_timeval(&(inode[inodeNum].created), timebuf, 28);
	printf("Created time\t= %s\n", timebuf);
	format_timeval(&(inode[inodeNum].lastAccess), timebuf, 28);
	printf("Last acc. time\t= %s\n", timebuf);
}

int file_remove(char *name)
{
	int inodeNumSrc = search_cur_dir(name);
	int i;
	if (inode[inodeNumSrc].type == directory)
	{
		printf("\n File remove Error: Is a Directory\n");
		return -1;
	}
	if (inodeNumSrc < 0)
	{
		printf("\n File remove Error : Source File Not found\n");
		return -1;
	}
	int pos = 0;
	int block_Count = inode[inodeNumSrc].blockCount;
	int isLinked = 0;
	for (i = 0; i < curDir.numEntry; i++)
	{
		int comp = strcmp(curDir.dentry[i].name, name);
		if (comp == 0)
		{
			pos = i;
			if (inodeNumSrc == curDir.dentry[i].inode)
			{
				if (inode[inodeNumSrc].link_count > 1)
				{
					inode[inodeNumSrc].link_count--;
					isLinked = 1;
				}
			}
			break;
		}
	}
	int numBlock = inode[inodeNumSrc].blockCount;
	for ( i = 0; i < numBlock; i++)
	{
		inode[inodeNumSrc].directBlock[i] = 0;
		set_bit(blockMap, i, 0);
	}
	if (isLinked == 0)
	{
		superBlock.freeInodeCount++;
		set_bit(inodeMap, curDir.dentry[pos].inode, 0);
		// superBlock.freeBlockCount++;
		superBlock.freeBlockCount = superBlock.freeBlockCount + block_Count;
	}
	curDir.dentry[pos] = curDir.dentry[curDir.numEntry - 1];
	memset(&curDir.dentry[curDir.numEntry - 1], 0, 0);

	curDir.numEntry--;
	return 0;
}

int file_remove1(char *name)
{
	int inodeNumSrc = search_cur_dir(name);
	int i;
	if (inode[inodeNumSrc].type == directory)
	{
		printf("\n File remove Error: Is a Directory\n");
		return -1;
	}
	if (inodeNumSrc < 0)
	{
		printf("\n File remove Error : Source File Not found\n");
		return -1;
	}
	int pos = 0;
	int block_Count = inode[inodeNumSrc].blockCount;
	int isLinked = 0;
	for (i = 0; i < curDir.numEntry; i++)
	{
		int comp = strcmp(curDir.dentry[i].name, name);
		if (comp == 0)
		{
			pos = i;
			if (inodeNumSrc == curDir.dentry[i].inode)
			{
				if (inode[inodeNumSrc].link_count > 1)
				{
					inode[inodeNumSrc].link_count--;
					isLinked = 1;
				}
			}
			break;
		}
	}
	int numBlock = inode[inodeNumSrc].blockCount;
	for ( i = 0; i < numBlock; i++)
	{
		inode[inodeNumSrc].directBlock[i] = 0;
		set_bit(blockMap, i, 0);
	}
	if (isLinked == 0)
	{
		superBlock.freeInodeCount++;
		set_bit(inodeMap, curDir.dentry[pos].inode, 0);
		superBlock.freeBlockCount = superBlock.freeBlockCount + block_Count;
	}
	curDir.dentry[pos] = curDir.dentry[curDir.numEntry - 1];
	memset(&curDir.dentry[curDir.numEntry - 1], 0, 0);
	curDir.numEntry--;
	return 0;
}

int dir_make(char *name)
{
	int inodeNum = search_cur_dir(name);
	int block;
	if (inodeNum >= 0)
	{
		printf(" Directory make Error :  %s exist.\n", name);
		return -1;
	}

	if (curDir.numEntry + 1 > MAX_DIR_ENTRY)
	{
		printf("Directory make Error : directory is full!\n");
		return -1;
	}
	if (superBlock.freeInodeCount < 1)
	{
		printf("Directory make Error : inode is full!\n");
		return -1;
	}
	inodeNum = get_free_inode();
	block = get_free_block();

	if (block < 0)
	{
		printf("Directory make Error : data block is full!\n");
		return -1;
	}
	if (inodeNum < 0)
	{
		printf("Directory make Error : not enough inode.\n");
		return -1;
	}

	inode[inodeNum].type = directory;
	inode[inodeNum].owner = 0; // pre-defined
	inode[inodeNum].group = 0; // pre-defined
	gettimeofday(&(inode[inodeNum].created), NULL);
	gettimeofday(&(inode[inodeNum].lastAccess), NULL);
	inode[inodeNum].size = 1;
	inode[inodeNum].blockCount = 1;
	inode[inodeNum].directBlock[0] = block;

	// inode[inodeNum].link_count = 1;

	// add a new dir into the current directory entry
	strncpy(curDir.dentry[curDir.numEntry].name, name, strlen(name));
	curDir.dentry[curDir.numEntry].name[strlen(name)] = '\0';
	curDir.dentry[curDir.numEntry].inode = inodeNum;
	// curDir.numEntry=curDir.numEntry+2;
	curDir.numEntry++;
	disk_write(curDirBlock, (char *)&curDir);
	int prevDirInode=curDir.dentry[0].inode;
	int c=curDirBlock;



	block = inode[inodeNum].directBlock[0];
	disk_write(curDirBlock, (char *)&curDir);
	curDirBlock = block;
	disk_read(block, (char *)&curDir);
	if(curDir.numEntry==0)
	{
		curDir.numEntry=curDir.numEntry+2;

		strncpy(curDir.dentry[0].name, ".", 1);
		curDir.dentry[0].name[1] = '\0';
		curDir.dentry[0].inode = inodeNum;
		// // curDir.numEntry = 1;
		strncpy(curDir.dentry[1].name, "..", 2);
		curDir.dentry[1].name[2] = '\0';
		curDir.dentry[1].inode = prevDirInode;
		disk_write(curDirBlock, (char *)&curDir);

		block = c;
		disk_write(curDirBlock, (char *)&curDir);
		curDirBlock = block;
		disk_read(block, (char *)&curDir);
	}
	return 0;
}

int deleteEmptyDir(int subDirectoryBlock,char name[],int inodeNumSrc)
{
			int pos,i;
			// int block = subDirectoryBlock;
			// disk_write(curDirBlock, (char *)&curDir);
			// curDirBlock = block;
			// disk_read(block, (char *)&curDir);

			int blockInode=curDir.dentry[1].inode;
			int block = inode[blockInode].directBlock[0];
			disk_write(curDirBlock, (char *)&curDir);
			curDirBlock = block;
			disk_read(block, (char *)&curDir);

			for ( i = 0; i < curDir.numEntry; i++)
			{
				int comp = strcmp(curDir.dentry[i].name, name);
				if (comp == 0)
				{
					pos = i;
					break;
				}
			}
			int numBlock = inode[inodeNumSrc].blockCount;

			for ( i = 0; i < numBlock; i++)
			{

				inode[inodeNumSrc].directBlock[i] = 0;
				set_bit(blockMap, i, 0);
			}

			superBlock.freeBlockCount++;
			superBlock.freeInodeCount++;
			
			// set_bit(inodeMap, curDir.dentry[pos].inode, 0);
			set_bit(inodeMap, inodeNumSrc, 0);

			curDir.dentry[pos] = curDir.dentry[curDir.numEntry - 1];

			memset(&curDir.dentry[curDir.numEntry - 1], 0, sizeof(DirectoryEntry));

			curDir.numEntry--;
}

int nav_dir(int parentDirectory,char fname[], int iteration, int dirInode, int is_file_deleted)
{
	int i,subInode,block;
	iteration++;
	for(i=0;i<curDir.numEntry;i++)
	{
		subInode=curDir.dentry[i].inode;
		// printf("name=%s,type=%d\n",curDir.dentry[i].name,inode[subInode].type);
		if(inode[subInode].type==file)
		{
			// printf("Deleting name=%s\n",curDir.dentry[i].name);
			file_remove1(curDir.dentry[i].name);
			i--;
		}
	}
	// printf("==========");
	for(i=2;i<curDir.numEntry;i++)
	{
		subInode=curDir.dentry[i].inode;
		// printf("name=%s,type=%d\n",curDir.dentry[i].name,inode[subInode].type);
		if(inode[subInode].type==directory)
		{
			block=inode[subInode].directBlock[0];
			disk_write(curDirBlock, (char *)&curDir);
			curDirBlock = block;
			disk_read(block, (char *)&curDir);
			// printf("curDir Num=%d\n",curDir.numEntry);
			if(curDir.numEntry==2)
			{
				deleteEmptyDir(0,curDir.dentry[i].name,subInode);
				i--;
			}
			else if(curDir.numEntry>2)
			{
				// printf("New Dent\n");
				nav_dir(parentDirectory,fname,iteration,dirInode,is_file_deleted);
				i--;
			}
		}
	}
	for(i=0;i<iteration;i++)
	{
			int blockInode=curDir.dentry[1].inode;
			block = inode[blockInode].directBlock[0];
			if(block>parentDirectory)
			{
				disk_write(curDirBlock, (char *)&curDir);
				curDirBlock = block;
				disk_read(block, (char *)&curDir);
				nav_dir(parentDirectory,fname,iteration,dirInode,is_file_deleted);
				iteration=iteration-2;
			}
	}
	// printf("Reached Last\n");
}

int dir_remove(char *name)
{
	// dir_remove_rec(name);
	// return 0;
	int inodeNumSrc = search_cur_dir(name);
	// int blockMap_to_del=0;
	int i;
	if (inode[inodeNumSrc].type == file)
	{
		printf("\n Directory remove Error :  Is a File\n");
		return -1;
	}
	if (inodeNumSrc < 0)
	{
		printf("\n Directory remove Error : Dirrectory Not found\n");
		return -1;
	}
	int pos = 0;
	int parentDirectoryBlock=curDirBlock;
	
	int block = inode[inodeNumSrc].directBlock[0];
	disk_write(curDirBlock, (char *)&curDir);
	curDirBlock = block;
	disk_read(block, (char *)&curDir);
	// blockMap_to_del=curDirBlock;
	// int subDirectoryInodes[500];
	// int subDirectoryBlocks[500];
	// int subDirectoryCount=0;
	// int navigated=0;
	int is_file_deleted=0;
	int h_directoryblock=curDirBlock;
	if(curDir.numEntry>2)
	{
		int iteration=0;
		nav_dir(parentDirectoryBlock,name,iteration,inodeNumSrc,is_file_deleted);
		// printf("Reached Here ========\n");
			block=h_directoryblock;
			disk_write(curDirBlock, (char *)&curDir);
			curDirBlock = block;
			disk_read(block, (char *)&curDir);
			// printf("CurDir=%d\n",curDirBlock);
			if(curDir.numEntry==2 && is_file_deleted==0)
			{
				// printf("Perfect, name=%s\n",name);
				is_file_deleted=1;
				deleteEmptyDir(parentDirectoryBlock,name,inodeNumSrc);
				return 0;
			}

		// printf("GREATER");
		// navigateDirectories(parentDirectoryBlock,curDirBlock,name,blockMap_to_del,name,subDirectoryInodes,subDirectoryBlocks,subDirectoryCount,navigated);
		// return 0;
	}
	else
	{
		// printf("Empty \n");
		deleteEmptyDir(parentDirectoryBlock,name,inodeNumSrc);
		return 0;
	}
}

int dir_change(char *name)
{
	int pos,i;
	int totalBlockCount=0;
	int inodeNumSrc = search_cur_dir(name);
	int lastInode=0;
	int inodeNum;

	if( strcmp(".",name)==0)
	{
		return 0;
	}

	if(strcmp("/",name)==0)
	{
		int block = inode[0].directBlock[0];
		disk_write(curDirBlock, (char *)&curDir);
		curDirBlock = block;
		disk_read(block, (char *)&curDir);
		printf(" block=%d, curBlock=%d\n",block,inode[0].directBlock[0]);
		return 0;
	}
	
	if( strcmp("..",name)==0)
	{
		int blockInode=curDir.dentry[1].inode;
		int block = inode[blockInode].directBlock[0];
		disk_write(curDirBlock, (char *)&curDir);
		curDirBlock = block;
		disk_read(block, (char *)&curDir);
	}

	if(inodeNumSrc<0)
	{
		printf("No such directory..\n");
		return -1;
	}

	if (inode[inodeNumSrc].type == file)
	{
		printf("\n Change Directory Error : Is a File\n");
		return -1;
	}

	if (inodeNumSrc<0)
	{
		printf("\n Change Directory Error : Directory Not Fuond\n");
		return -1;
	}
	

	int block = inode[inodeNumSrc].directBlock[0];
	disk_write(curDirBlock, (char *)&curDir);
	curDirBlock = block;
	disk_read(block, (char *)&curDir);

	// int prevDirInode=curDir.dentry[0].inode;
	// disk_write(curDirBlock, (char *)&curDir);

	// int block = inode[inodeNumSrc].directBlock[0];
	// disk_write(curDirBlock, (char *)&curDir);
	// curDirBlock = block;
	// disk_read(block, (char *)&curDir);

	// ============================================
	// int prevDirInode=curDir.dentry[0].inode;
	// int curdir=curDirBlock;
	// int newdir=inode[inodeNum].directBlock[0];


	// block = newdir;
	// disk_write(curDirBlock, (char *)&curDir);
	// curDirBlock = block;
	// disk_read(block, (char *)&curDir);
	// blockMap_to_del=curDirBlock;

	// curDir.numEntry = 2;
	// strncpy(curDir.dentry[0].name, ".", 1);
	// curDir.dentry[0].name[1] = '\0';
	// curDir.dentry[0].inode = inodeNum;
	// curDir.numEntry++;


	// strncpy(curDir.dentry[1].name, "..", 2);
	// curDir.dentry[1].name[2] = '\0';
	// curDir.dentry[1].inode = prevDirInode;
	// curDir.numEntry++;

	// disk_write(curDirBlock, (char *)&curDir);
	// // disk_write(curDirBlock, (char *)&curDir);
	// curDirBlock = block;
	// disk_read(block, (char *)&curDir);


	// block = curdir;
	// disk_write(curDirBlock, (char *)&curDir);
	// curDirBlock = block;
	// disk_read(block, (char *)&curDir);
	//=====================================================================
	return 0;
}

int ls_temp()
{
	int i;
	for (i = 0; i < curDir.numEntry; i++)
	{
		int n = curDir.dentry[i].inode;
		if (inode[n].type == file)
			printf("type: file, ");
		else
			printf("type: dir, ");
		printf("name \"%s\", inode %d, size %d byte, link=%d ,owner=%d, curDirBlock=%d, nument=%d\n", curDir.dentry[i].name, curDir.dentry[i].inode, inode[n].size, inode[n].link_count, inode[n].owner,curDirBlock,curDir.numEntry);
	}

	return 0;
}

int ls()
{
		int i;
		for(i = 0; i < curDir.numEntry; i++)
		{
				int n = curDir.dentry[i].inode;
				if(inode[n].type == file) printf("type: file, ");
				else printf("type: dir, ");
				printf("name \"%s\", inode %d, size %d byte\n", curDir.dentry[i].name, curDir.dentry[i].inode, inode[n].size);
		}
		return 0;
}

int fs_stat()
{
	printf("File System Status: \n");
	printf("# of free blocks: %d (%d bytes), # of free inodes: %d\n", superBlock.freeBlockCount, superBlock.freeBlockCount * 512, superBlock.freeInodeCount);
}

int hard_link(char *src, char *dest)
{
	int inodeNumSrc = search_cur_dir(src);
	// printf("%d", inodeNumSrc);
	if (inodeNumSrc < 0)
	{
		printf("\n Link Error : Source File Not found\n");
		return 0;
	}

	if (inode[inodeNumSrc].type == directory)
	{
		printf("\n Link Error : Is a Directory\n");
		return 0;
	}

	int inodeNumDest = search_cur_dir(dest);
	// printf("%d", inodeNumDest);
	if (inodeNumDest > 0)
	{
		printf("\n Link Error : File Already Present\n");
		return 0;
	}

	inode[inodeNumSrc].link_count++;
	// add a new file into the current directory entry
	strncpy(curDir.dentry[curDir.numEntry].name, dest, strlen(dest));
	curDir.dentry[curDir.numEntry].name[strlen(dest)] = '\0';
	curDir.dentry[curDir.numEntry].inode = inodeNumSrc;
	// printf("curdir %s, name %s\n", curDir.dentry[curDir.numEntry].name, dest);
	curDir.numEntry++;
	// printf("-- Error: ln ilss not implemented.\n");
	return 0;
}

int execute_command(char *comm, char *arg1, char *arg2, char *arg3, char *arg4, int numArg)
{

	printf("\n");
	if (command(comm, "df"))
	{
		return fs_stat();

		// file command start
	}
	else if (command(comm, "create"))
	{
		if (numArg < 2)
		{
			printf("error: create <filename> <size>\n");
			return -1;
		}
		return file_create(arg1, atoi(arg2)); // (filename, size)
	}
	else if (command(comm, "stat"))
	{
		if (numArg < 1)
		{
			printf("error: stat <filename>\n");
			return -1;
		}
		return file_stat(arg1); //(filename)
	}
	else if (command(comm, "cat"))
	{
		if (numArg < 1)
		{
			printf("error: cat <filename>\n");
			return -1;
		}
		return file_cat(arg1); // file_cat(filename)
	}
	else if (command(comm, "read"))
	{
		if (numArg < 3)
		{
			printf("error: read <filename> <offset> <size>\n");
			return -1;
		}
		return file_read(arg1, atoi(arg2), atoi(arg3)); // file_read(filename, offset, size);
	}
	else if (command(comm, "rm"))
	{
		if (numArg < 1)
		{
			printf("error: rm <filename>\n");
			return -1;
		}
		return file_remove(arg1); //(filename)
	}
	else if (command(comm, "ln"))
	{
		return hard_link(arg1, arg2); // hard link. arg1: src file or dir, arg2: destination file or dir

		// directory command start
	}
	else if (command(comm, "ls"))
	{
		return ls();
	}
	else if (command(comm, "mkdir"))
	{
		if (numArg < 1)
		{
			printf("error: mkdir <dirname>\n");
			return -1;
		}
		return dir_make(arg1); // (dirname)
	}
	else if (command(comm, "rmdir"))
	{
		if (numArg < 1)
		{
			printf("error: rmdir <dirname>\n");
			return -1;
		}
		return dir_remove(arg1); // (dirname)
	}
	else if (command(comm, "cd"))
	{
		if (numArg < 1)
		{
			printf("error: cd <dirname>\n");
			return -1;
		}
		return dir_change(arg1); // (dirname)
	}
	else
	{
		fprintf(stderr, "%s: command not found.\n", comm);
		return -1;
	}
	return 0;
}
