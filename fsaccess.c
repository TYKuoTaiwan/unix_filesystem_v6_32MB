// CS 5348 Operating Systems Concepts Fall 2016
// Project 2
// Tzu-Yi Kuo and Abdullah Kucuk 

// run on cs2.utdallas.edu
// compile with: 
////   g++ -o fsaccess fsaccess.cpp
// run with:
//   ./v6filesystem

// available commands:
// initfs, cpin, cpout, mkdir, rm, q

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

// unsigned short temp;
unsigned short fd, fd2, inodeCurrent;
unsigned short chainarray[256];
const unsigned short blockSize = 512;
// const unsigned short inode_alloc = 0100000;
// const unsigned short plainfile = 000000;
// const unsigned short largefile = 010000;
// const unsigned short directory = 040000;

struct inode{
	unsigned short flag;
	char size0;
	unsigned int size1;
	unsigned short addr[8]; 
};

struct superBlock{
	unsigned short isize;
	unsigned short fsize;
	unsigned short remainSize;
	unsigned short nfree;
	unsigned short ninode;
	unsigned short freeList[100];
	unsigned short inodeList[100];
}

struct dirEntry{
	unsigned short inodeNum;
	char name[14];
};

inode local, initial;
superBlock super;

void initFS(char* path, unsigned short total_blcks,unsigned short total_inodes);
void create_root();
void blockreaderchar(char *target, unsigned int blocknum);
void blockreaderint(int *target, unsigned int blocknum);
void blockwriterint(unsigned int *target, unsigned int blocknum);
void blockwriterchar(char *target, unsigned int blocknum);
void inodewriter(fs_inode inodeinstance, unsigned int inodenumber);
void freeblock(unsigned int block);
void chaindatablocks(unsigned short total_blcks);
unsigned int allocatedatablock();
unsigned short allocateinode();
void mkdirectory(char* filename, unsigned int newinode);
int directorywriter(fs_inode rootinode, dir dir);
int makeindirectblocks(int fd,int block_num);
void cpin(char* src, char* targ);
void cpout(char* src, char* targ);

int main( int argc, char *argv[] ) {

	if( argc == 2 ) {
      fd = open(argv[1], O_CREAT|O_TRUNC|O_RDWR, S_IRWXU|S_IRWXG|S_IRWXO);
	}
	else if( argc > 2 ) {
    	printf("Too many arguments supplied.\n");
    }
    else {
    	printf("Path and name of the file are expected.\n");
    }

	char input[256];
	char *parser;
	bool fsActive = false;
	// unsigned short n = 0;
	// char dirbuf[blockSize];
	// int i =0;
	// unsigned short bytes_written;
	// unsigned int number_of_blocks =0, number_of_inodes=0;

	printf("Enter command:\n");
	while (1) 
	{
		scanf(" %[^\n]s", input);
		parser = strtok(input," ");

		//initfs will initialize the file system
		if (strcmp(parser, "initfs")==0)//v
		{
			char *filepath, *num1, *num2;
			filepath = strtok(NULL, " ");
			num1 = strtok(NULL, " ");
			num2 = strtok(NULL, " ");
			if (fsActive)
			{
				printf("The filesystem already exists.\n");
			}
			else
			{
				if (num1 && num2){
					number_of_blocks = atoi(num1);
					number_of_inodes = atoi(num2);
					initFS(number_of_blocks, number_of_inodes);
					printf("The file system is initialized\n");
					fsActive = true;
				}
				else
					printf("Please check the parameters that you have entered.\n");
			}
 			parser = NULL;
		}

		//
		else if(strcmp(parser, "cpin")==0)
		{
			char *targname;
			char *srcname;
		  	if(fsActive)
		  	{
		  		srcname = strtok(NULL, " ");
		  		targname = strtok(NULL, " ");
		 		if(srcname && targname )
		 			cpin(srcname,targname);
		  		else
		  			printf("Please retry\n");
		  	}
		  	else
		  		printf("Please retry after initializing file system\n");

		   parser = NULL;
		}

		//
		else if(strcmp(parser, "cpout")==0)
		{
			char *targnameout;
			char *srcnameout;
		 	if(fsActive)
		  	{
			  	srcnameout = strtok(NULL, " ");
			  	targnameout = strtok(NULL, " ");
			 
			  	if(srcnameout && srcnameout )
			  		cpout(srcnameout,targnameout);
			  	else
			  		printf("Please retry\n");
			}
			else
				printf("Please retry after initializing file system\n");
			
			parser = NULL;
		}
		//
		else if (strcmp(parser, "mkdir")==0)//v 
		{
			char *dirname;
		 	if(fsActive)
		 	{
		 		dirname = strtok(NULL, " ");
		  		if(!dirname)
		    		printf("Directory name is needed.\n");
		  		else
		  		{
		  			unsigned short dirinum = allocInode();
		  			if(dirinum < 0)
					{	
			 			printf("Error : ran out of inodes \n");
					}
					else
					{
		  				mkdirectory(dirname,dirinum);
					}
		  		}
		 	}
		 	else
		 	{
		 		printf("Please retry after initializing file system\n");
		  		
		  	}
		   	parser = NULL;
		}

		else if(strcmp(parser, "cd")==0)//v // Changes current directory
		{
			if(fsActive)
			{
				char *dirName;
				dirName  = strtok(NULL, " ");
				if(dirName)
				{
					int inum = getInum(dirname);
					local = accessInode(inum);
					if(inum!= -1 && checkFlag(local.flags, DIR))
					{
						inodeCurrent = inum;
					}
					else
					{
						cout << "Invalid directory" << endl;
					}
				}
			}
			else
			{
				cout << "There is not a file system active right now. Please use 'initfs' or 'use' to start one." << endl;
			}
		}
//
		else if(strcmp(parser, "rm")==0)
		{
		
		}
//
		else if(strcmp(parser, "q")==0)//v
		{
		
			lseek(fd,blockSize,0);
			if(fsActive)
			{
				write(fd,&super,sizeof(super));
				close(fd);
			}
			return 0;
		}
//	
		else
		{
			printf("\nInvalid command\n ");
		}
	}
	return 0;
}

enum inodeFlag{
	ALLOC, //allocated
	PLAIN, //regular file
	DIR, //directory
	LARGE //large file
};

int shift (int blockNum, int offset = 0){
	return blockNum * blockSize + offset;
}

void chaindatablocks (unsigned short totalBlockNum) {
	unsigned short emptybuffer[512];   // buffer to fill with zeros to entire blocks. Since integer size is 4 bytes, 512 * 4 = 2048 bytes.
	unsigned short chunkNum = totalBlockNum/100;			//splitting into blocks of 100
	unsigned short remainingBlockNum = totalBlockNum%100;		//getting remaining/left over blocks
	unsigned short blockcounter;

	for (i=0; i<512; i++)
		emptybuffer[i] = 0;	
	
	//chaining for chunks of blocks 100 blocks at a time
	for (blockcounter=0; blockcounter < chunkNum; blockcounter++)
	{
		chainarray[0] = 100;
	
		for (i=0;i<100;i++)
		{
			if (blockcounter == (chunkNum - 1) && remainingBlockNum == 0 && i==0) {
				chainarray[i+1] = 0;
			}
			else if (i==0) {
				chainarray[i+1] = 2+super.isize+i+100*(blockcounter+1);
			}
			else{
				chainarray[i+1] = 2+super.isize+i+100*(blockcounter);
			}

		}	
		

		for (i=0; i<100;i++){
			if (i==0) {
				lseek(fd,shift(2+super.isize+0+100*blockcounter),SEEK_SET);
        		write(fd, &chainarray, blockSize);
			}
			else {
				lseek(fd,shift(2+super.isize+i+100*blockcounter),SEEK_SET);
        		write(fd, &emptybuffer, blockSize);
			}
		}
	}

	//chaining remaining blocks
	chainarray[0] = remainingBlockNum;
	chainarray[1] = 0;
	for (i=1;i<=remainingBlockNum;i++)
		chainarray[i+1] = 2+super.isize+i+(100*blockcounter);

	for (i=0; i<remainingBlockNum;i++){
		if (i==0) {
			lseek(fd,shift(2+super.isize+0+100*blockcounter),SEEK_SET);
        	write(fd, &chainarray, blockSize);
		}
		else {
			lseek(fd,shift(2+super.isize+i+100*blockcounter),SEEK_SET);
        	write(fd, &emptybuffer, blockSize);
		}
	}
}

void fillFreeList (int blockNum) {
	unsigned short buf;
	lseek(fd, shift(blockNum), SEEK_SET);
	read(fd, &super.nfree, sizeof(super.nfree));
	for (int i = 0; i<100; i++) {
		read(fd, &buf, sizeof(buf));
		super.freeList[i] = buf;
	}
}

void fillInodeList () {
	lseek(fd, 2 * blockSize, SEEK_SET);
	int counter = 0;
	for(int i = 0; i < (blockSize / sizeof(inode)) * super.isize; i++) // retrieve 100 free inode by traveling through each inode 
	{
		if(counter >= 100)
		{
			break;
		}
		read(fd, &local, sizeof(local));
		if(!checkFlag(local.flags, ALLOC))
		{
			super.inodeList[counter] = i + 1; // since root inode starts at 1 
			counter++; 
		}
	}
	super.ninode = counter;
}

unsigned short allocBlock () {
	super.nfree--;
	if (super.nfree > 0) {
		super.remainSize--;
		return super.freeList[super.nfree];
	}

	unsigned short newBlock = freeList[0];
	fillFreeList(newBlock);
	super.remainSize--;
	return newBlock;
}
unsigned short allocInode () {

	if (super.ninode > 0) {
		super.ninode--;
		return super.inodeList[super.nfree];
	}

	// List is empty, so...
	fillInodeList();
	if (super.ninode > 0)
	{
		return allocInode(); 
	}
	
	// No inodes left!
	return -1;
}

void setFlag (unsigned short &flags, inodeFlag iflag) {
	switch(iflag)
	{
		case ALLOC: 
			flags = flags | 0x8000;
			break;

		case PLAIN: 
			flags = flags | 0x6000;
			flags = flags & ~(0x0100); 
			break;

		case DIR: 
			flags = flags | 0x4000; // 4 or 6
			flags = flags & ~(0x2000);// has to be 4
			flags = flags & ~(0x0100);  
			break;

		case LARGE: 
			flags = flags | 0x1100;
			break;
	}
}

int checkFlag (unsigned short &flags, inodeFlag iflag) {
	switch(iflag)
	{
		case ALLOC: 
			return  (flags & 0x8000) == 0x8000;
			break;

		case PLAIN: 
			return  (flags & 0x6000) == 0x6000 && ((flags & 0x0100) != 0x0100);
			break;

		case DIR: 
			return ((flags & 0x4000) == 0x4000) && ((flags & 0x2000) != 0x2000) && ((flags & 0x0100) != 0x0100);
			break;

		case LARGE: 
			return  (flags & 0x1100) == 0x1100;
			break;
	}
}

// Write a new inode into the given directory.
void initInode (int dir, inodeFlag flag) {
	local = accessInode(dir);
	setFlag(local.flags,ALLOC);
	setFlag(local.flags,flag);
	write_inode(dir, local);
}

inode accessInode (int inum) {
	lseek(fd,2 * blockSize + (inum - 1) * sizeof(inode), SEEK_SET);
	read(fd, &local, sizeof(local));
	return local;	
}

void writeInode (int inum, inode node) {
	lseek(fd, 2 * blockSize + (inum - 1) * sizeof(inode), SEEK_SET);
	write(fd, &node, sizeof(node));
}

// Make a new entry in the given directory with the given inode number and name.
void addEntry (int dir, int inodeNum, char *filename) {
	dirEntry entry;
	entry.inodeNum = inodeNum;
	strncpy(entry.name,filename,14);

	lseek(fd, 2 * blockSize + (dir - 1) * sizeof(inode), SEEK_SET);
	read(fd, &local, sizeof(local));
	if(local.size % blockSize == 0)
	{
		local.addr[local.size / blockSize] = alloc_block();
	}
	lseek(fd, get_offset(local.addr[local.size / blockSize], local.size % blockSize), SEEK_SET);
	write(fd, &entry, sizeof(entry));

	local.size += sizeof(entry);
	lseek(fd, 2 * blockSize + (dir - 1) * sizeof(inode), SEEK_SET);
	write(fd, &local, sizeof(local));
}

int getInum(char* filename) {
	local  = accessInode(current_inode);
	dirEntry tempEntry;
	for (int i=0; i<local.size/16; i++) {

		if (i % sizeof(inode)==0){
			lseek(fd, local.addr[i/sizeof(inode)],SEEK_SET);
		}
		read(fd, &tempEntry, sizeof(tempEntry));
		if (strcmp(parser, tempEntry.name) == 0)
		{
			return tempEntry.inodeNum;
		}

	}
	return -1;
}

//function to create root directory and its corresponding inode.
***void create_root()
{
	unsigned short bytes_written;
	unsigned short newBlock = allocBlock();	

	for (int i=0;i<14;i++)
		newdir.filename[i] = 0; 
		
	newdir.filename[0] = '.';			//root directory's file name is .
	newdir.filename[1] = '\0';
	newdir.inode = 1;					// root directory's inode number is 1.

	inoderef.flags = inode_alloc | directory | 000077;   		// flag for root directory 
	inoderef.size0 = '0';
	inoderef.size1 = ISIZE;
	inoderef.addr[0] = datablock;

	for (int i=1;i<8;i++)
		inoderef.addr[i] = 0;

	writeInode();
	inodewriter(inoderef, 0); 		

	lseek(fd, shift(newBlock), 0);
	//filling 1st entry with .
	write(fd, &newdir, 16);
	
	newdir.filename[1] = '.';
	newdir.filename[2] = '\0';
	// filling with .. in next entry(16 bytes) in data block.	
	write(fd, &newdir, 16);

	inodeCurrent = 1;
}

void initFS(unsigned short total_blcks,unsigned short total_inodes )
{
	char buffer[BLOCK_SIZE];
	int bytes_written;

	

	if((total_inodes%16) == 0)
		super.isize = total_inodes/16;
	else
		super.isize = (total_inodes/16) + 1;

	super.fsize = total_blcks;

	// Set length of file
	ftruncate(fd, super.fsize * blockSize);//**********************************

	lseek(fd, 1 * blockSize, SEEK_SET);
	write(fd, &super, sizeof(super));

	// writing zeroes to all inodes in ilist
	for (int i=0; i<blockSize; i++)
		buffer[i] = 0;
	for (i=0; i < super.isize; i++)
		write(fd,buffer,blockSize);

	// calling chaining data blocks procedure
	chaindatablocks(total_blcks);	

	//filling free array to first 100 data blocks
	fillInodeList();
	fillFreeList(2+super.isize);

	// Make root directory
	create_root();
}

void mkdirectory(char* filename, unsigned short newinode)
{
	int inum = allocInode();
	initInode(inum, DIR);

	// Add to current directory
	addEntry(inodeCurrent, inum, filename);//************
	addEntry(inum, inum, ".");
	addEntry(inum, inodeCurrent, "..");
	printf("\n Directory created \n");
}

void cpin(char* src, char* targ)
{
		//open external file
		int srcfd;
		 if((srcfd = open(src, O_RDONLY)) == -1)
		{
			printf("\nerror opening file: %s \n",src);
			return;
		}

		int indirect = 0;
		int indirectfn_return =1;
		char reader[BLOCK_SIZE];
		int bytes_read;
		int extfilesize =0;
        unsigned int inumber = allocateinode();
		if(inumber < 0)
		{
		 	printf("Error : ran out of inodes \n");
		 	return;
		}
        unsigned int newblocknum;
		
		//preapare new file in V6 file system
		newdir.inode = inumber;
		memcpy(newdir.filename,targ,strlen(targ));
		//write inode for the new file
		inoderef.flags = inode_alloc | plainfile | 000777;
        inoderef.nlinks = 1; 
        inoderef.uid = '0';
        inoderef.gid = '0';
		inoderef.size0='0';
		
		int i =0;
		
		//start reading external file and perform file size calculation simultaneously	
		while(1)
		{
			if((bytes_read=read(srcfd,reader,BLOCK_SIZE)) != 0 )
			{
				newblocknum = allocatedatablock();
				blockwriterchar(reader,newblocknum);
				inoderef.addr[i] = newblocknum;
				// When bytes returned by read() system call falls below the block size of 
				//2048, reading and writing are complete. Print file size in bytes and exit
				if(bytes_read < BLOCK_SIZE)
				{
					extfilesize = i*BLOCK_SIZE + bytes_read;
					printf("Small file copied\n");
					inoderef.size1 = extfilesize;
					printf("File size = %d bytes\n",extfilesize);
					break;
				}
		
				i++;
		
			//if the counter i exceeds 27(maximum number of elements in addr[] array,
			//transfer control to new function that creates indirect blocks which
			//handles large files(file size > 56 KB).
				if(i>27)
				{
		
					indirectfn_return=makeindirectblocks(srcfd,inoderef.addr[0]);
					indirect = 1;
					break;
				}
			}
		// When bytes returned by read() system call is 0,
		// reading and writing are complete. Print file size in bytes and exit
			else
			{
				extfilesize = i*BLOCK_SIZE;
				printf("Small file copied\n");
				inoderef.size1 = extfilesize;
				printf("File size = %d bytes\n",extfilesize);
				break;
			}
		
		}
		inoderef.actime[0] = 0;
		inoderef.modtime[0] = 0;
		inoderef.modtime[1] = 0;	

		//if call is made to function that creates indirect blocks,
		//it is a large file. Set flags for large file to 1.
		if(indirect == 1)
		{
			inoderef.flags = inoderef.flags | largefile;
		}
		//write to inode and directory data block
		if(	indirectfn_return > -1)
		{
			inodewriter(inoderef,inumber);
			lseek(fd,2*BLOCK_SIZE,0);
			read(fd,&inoderef,ISIZE);
 			inoderef.nlinks++;
			directorywriter(inoderef,newdir); 
		}
		if(indirectfn_return == -1)
		{
			printf("\nExitting as file is large..");
		}
}

void cpout(char* src, char* targ)
{
		int indirect = 0;
		int found_dir = 0;
		int src_inumber;
		char reader[BLOCK_SIZE];						//reader array to read characters (contents of blocks or file contents)
		int reader1[BLOCK_SIZE];						//reader array to read integers (block numbers contained in add[] array)
		int bytes_read;
		int targfd;
		int i=0;
		int j=0;
		int addrcount=0;
		int total_blocks=0;
		int remaining_bytes =0;
		int indirect_block_chunks = 0;						//each chunk of indirect blocks contain 512 elements that point to data blocks
		int remaining_indirectblks=0;
		int indirectblk_counter=0;
		int bytes_written=0;
		
		//open or create external file(target file) for read and write
		 if((targfd = open(targ, O_RDWR | O_CREAT, 0600)) == -1)
			{
			printf("\nerror opening file: %s\n",targ);
			return;
			}
		lseek(fd,2*BLOCK_SIZE,0);
		read(fd,&inoderef,ISIZE);
		
			//find the source V6 file in the root directory
		for (addrcount=0;addrcount <= 27;addrcount++)
		{
		if(found_dir !=1)
		{
		lseek(fd,(inoderef.addr[addrcount]*BLOCK_SIZE), 0);
		for (i=0;i<128;i++)
		{		if(found_dir !=1)
				{
				read(fd, &dir1, 16);
			
				if(strcmp(dir1.filename,src) == 0)
				{
				
				src_inumber = dir1.inode;
				found_dir =1;
				}
				}
			
		}
		}
		}
		
		if(src_inumber == 0)
		{
		printf("File not found in the file system. Unable to proceed\n");
		return;
		}
		lseek(fd, (2*BLOCK_SIZE + ISIZE*src_inumber), 0);	
		read(fd, &inoderef, 128);
		
		//check if file is directory. If so display information and return.
		if(inoderef.flags & directory)
		{
		printf("The given file name is a directory. A file is required. Please retry.\n");
		return;
		}
		
		//check if file is a plainfile. If so display information and return.
		if((inoderef.flags & plainfile))
		{
		printf("The file name is not a plain file. A plain file is required. Please retry.\n");
		return;
		}
		//check if file is a large file
		if(inoderef.flags & largefile)
		{
		indirect = 1;
		}
		
		total_blocks = (int)ceil(inoderef.size1 / 2048.0);
		remaining_bytes = inoderef.size1 % BLOCK_SIZE;
		
		
		//read and write small file to external file
		if(indirect == 0)				//check if it is a small file. indirect = 0 implies the function that makes indirect blocks was not called during cpin.
		{
		printf("file size = %d \n",inoderef.size1);
		
			for(i=0 ; i < total_blocks ; i++)
			{
				blockreaderchar(reader,inoderef.addr[i]);
				//if counter reaches end of the blocks, write remaining bytes(bytes < 2048) and return.
				if( i == (total_blocks - 1))
				{
					write(targfd, reader, remaining_bytes);
					printf("Contents were transferred to external file\n");
					return;
				}
				write(targfd, reader, BLOCK_SIZE);
			}
		}
		
		
		//read and write large file to external file
		if(indirect == 1)			//check if it is a large file. indirect = 1 implies the function that makes indirect blocks was called during cpin.
		{
			total_blocks = inoderef.size1 / 2048;
			indirect_block_chunks = (int)ceil(total_blocks/512.0); 	//each chunk of indirect blocks contain 512 elements that point to data blocks
			remaining_indirectblks = total_blocks%512;
			printf("file size = %d \n",inoderef.size1);
	
			//Loop for chunks of indirect blocks
			for(i=0 ;i < indirect_block_chunks; i++)
			{
				blockreaderint(reader1,inoderef.addr[i]);				//store block numbers contained in addr[] array in integer reader array )
		
				//if counter reaches last chunk of indirect blocks, program loops the remaining and exits after writing the remaining bytes
			if(i == (indirect_block_chunks - 1))
				total_blocks = remaining_indirectblks;
			for(j=0; j < 512 && j < total_blocks; j++)
			{
				
				blockreaderchar(reader,reader1[j]);			//store block contents pointed by addr[] array in character  reader array )
				if((bytes_written = write(targfd, reader, BLOCK_SIZE)) == -1)
				{
					printf("\n Error in writing to external file\n");
					return;
				}
				
				if( j == (total_blocks - 1))
				{
					write(targfd, reader, remaining_bytes);
					printf("Contents were transferred to external file\n");
					return;
				}	
			}
		}
	}
}





