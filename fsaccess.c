// CS 5348 Operating Systems Concepts Fall 2016
// Project 2
// Tzu-Yi Kuo and Abdullah Kucuk 

// run on cs2.utdallas.edu
// compile with: 
//   g++ -o fsaccess fsaccess.c
// run with:
//   ./fsaccess <path to the disk>

// available commands:
// initfs, cpin, cpout, mkdir, q, cd

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>


const unsigned short blockSize = 512;
const unsigned int maxSize = 32*1024*1024;

typedef struct {
    unsigned short flags;
    char nlinks;
    char uid;
    char gid;
    char size0;
    unsigned short size1;
    unsigned short addr[8];
    unsigned short actime[2];
    unsigned short modtime[2];
}inode;
typedef struct{
    unsigned short isize;
    unsigned short fsize;
    unsigned short nfree;
    unsigned short free[100];
    unsigned short ninode;
    unsigned short inodeList[100];
    char flock;
    char ilock;
    char fmod;
    unsigned short time[2];
}superBlock;
enum inodeFlag{
    ALLOC, //allocated
    PLAIN, //regular file
    DIR, //directory
    LARGE //large file
};

superBlock super;
inode Inode;
int fd;
unsigned short dataBlockNum, inodeBlockNum, freelistBlockNum, inodeCurrent, totalBlockNum, inodeNum;

void fillInodeList ();
unsigned short allocInode ();
void initFreelist ();
int fillFreelist ();
unsigned short allocBlock ();
unsigned int sizing (char size0, unsigned short size1, unsigned short flags);
void setFlag (unsigned short flags, enum inodeFlag iflag);
int checkFlag (unsigned short flags, enum inodeFlag iflag);
int find_dirInode(char *directory, int dirInodeNo);
void addEntry (char *filename, unsigned short dirInodeNo, unsigned short file_inode_num);

void initFS ();
void mkdirectory (char* filename, unsigned short inum);
void createFile (char *src, char *dst);
void copyFileExt (char *srcname, char *targnam);

int main(int argc, char *argv[])
{
//	int i = 0, j = 0;
//	char buffer[50], path[50];
//	char cmd[10], total_blocks[10];
//	char initfs_cmd[50];
//	char ext_file[1000], int_file[1000], int_direct[1000];
//	char directory[14];
//	unsigned char dir_orfile[14];
//    unsigned short dirInodeNo, file_inode_num, inode_num;
//	char filename[14];
//	int error;
	

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
    char *cmd;
    int fsActive = 0;
    
	while(1)
	{
        printf("Enter command:\n");
        scanf(" %[^\n]s", input);
        cmd = strtok(input," ");
        
		if(strcmp(cmd, "initfs") == 0)
		{
			char *num1, *num2;
            num1 = strtok(NULL, " ");
            num2 = strtok(NULL, " ");
            if (fsActive)
            {
                printf("The filesystem already exists.\n");
            }
            else
            {
                if (num1 && num2){
                    totalBlockNum = atoi(num1);
                    inodeNum = atoi(num2);
                    initFS();
                    printf("The file system is initialized\n");
                    fsActive = 1;
                }
                else
                    printf("Please check the parameters that you have entered.\n");
            }
            cmd = NULL;
		}
        
		else if(strcmp(cmd, "q") == 0)
		{
			lseek(fd, blockSize, SEEK_SET);
			write(fd, &super, sizeof(super));
            close(fd);
            return 0;
		}
        
		else if(strcmp(cmd, "cpin") == 0)
		{
            char *targname;
            char *srcname;
            if(fsActive)
            {
                srcname = strtok(NULL, " ");
                targname = strtok(NULL, " ");
                if(srcname && targname )
                {
                    //cpoy in the file
                    createFile(srcname, targname);
                }
                else
                    printf("Please retry\n");
            }
            else
                printf("Please retry after initializing file system\n");
            
            cmd = NULL;
		}

		else if(strcmp(cmd, "cpout") == 0)
		{
            
            char *targname;
            char *srcname;
            if(fsActive)
            {
                srcname = strtok(NULL, " ");
                targname = strtok(NULL, " ");
                if(srcname && targname )
                {
                    //cpoy out the file
                    copyFileExt(srcname, targname);
                }
                else
                    printf("Please retry\n");
            }
            else
                printf("Please retry after initializing file system\n");
            
            cmd = NULL;
        }

		else if(strcmp(cmd,"mkdir") == 0)
		{
            if(fsActive)
            {
                char *dirname;
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
            cmd = NULL;
            
        }
        
        else if(strcmp(cmd, "cd")==0)
        {
            if(fsActive)
            {
                char *dirName;
                dirName  = strtok(NULL, " ");
                if(dirName)
                {
                    inode temp;
                    int inum = find_dirInode(dirName, inodeCurrent);
                    lseek(fd, 2 * blockSize + inum * sizeof(inode), SEEK_SET);
                    read(fd, &temp, sizeof(inode));
                    if(inum!= 0 && checkFlag(temp.flags, DIR))
                    {
                        inodeCurrent = inum;
                    }
                    else
                    {
                        printf("Invalid directory\n");
                    }
                }
            }
            else
            {
                printf("Please retry after initializing file system\n");
            }
        }
        
        else
        {
            printf("\nInvalid command\n ");
		}
	}
}

void fillInodeList () {
    lseek(fd, 2 * blockSize, SEEK_SET);
    inode local;
    int counter = 0;
    for(int i = 1; i < (blockSize / sizeof(inode)) * super.isize; i++) // retrieve 100 free inode by traveling through each inode
    {
        if(counter >= 100)
        {
            break;
        }
        read(fd, &local, sizeof(inode));
        if(!checkFlag(local.flags, ALLOC))
        {
            super.inodeList[counter] = i; // since root inode starts at 0
            counter++;
        }
    }
    super.ninode = counter;
    return;
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

void initFreelist () {
    unsigned short nfree = 100;
    unsigned short freelistBlockNoo = 0, freeDataBlockNo;
    unsigned short emptySignal = 0;
    int dataBlockNum = totalBlockNum - inodeBlockNum - 2;
    
    unsigned short freelistBlockNum = (dataBlockNum - 1) / 100 ;
    
    freelistBlockNoo = 2 + inodeBlockNum + 1; // first block is for root directory block
    
    freeDataBlockNo = 2 + inodeBlockNum + freelistBlockNum + 1; // In the data black array, there are two part. The first part is for block lists, and the second part is for empty blocks.
    
    // initialize freelist
    super.free[0] = freelistBlockNoo;
    for(int i = 1; i < 100; i++)
    {
        super.free[i] = freelistBlockNoo++;
    }
    
    // free data block list
    lseek(fd, freelistBlockNoo * blockSize, SEEK_SET);
    int remainingBlockNum = (dataBlockNum - 1 - 100);// dataBlockNum - root block - blocks already in super.free
    for(int i = 0; i < freelistBlockNum; i++)
    {
        if (remainingBlockNum / 100)
            nfree = 100 - 1;
        else
            nfree = remainingBlockNum % 100;
        
        write(fd, &nfree, 2);
        
        if (i == freelistBlockNum - 1)
        {
            write(fd, &emptySignal, 2); //write 0 in the head of the last list
        }
        else
        {
            freelistBlockNoo++;
            write(fd, &freelistBlockNoo, 2); // link the head of each list
            remainingBlockNum--;
        }
        
        for (int j = 0; j < nfree - 1; j++)
        {
            write(fd, &freeDataBlockNo, 2);
            freeDataBlockNo++;
            remainingBlockNum--;
        }
        
        lseek(fd, freelistBlockNoo * blockSize, SEEK_SET); // jump to next block
    }
}

int fillFreelist () {
    if(super.free[0] == 0)
    {
        printf("No more available data blocks \n");
        return 0;
    }
    else
    {
        unsigned short nfree;
        unsigned short freelistBlockNo = super.free[0];
        lseek(fd, freelistBlockNo * blockSize, SEEK_SET);
        read(fd, &nfree, 2);
        
        unsigned short buf;
        for(int i = 0; i < nfree; i++)
        {
            read(fd, &buf, 2);
            super.free[i] = buf;
        }
        
        super.free[nfree] = freelistBlockNo;
        super.nfree = nfree + 1;
        return 1;
    }
}

unsigned short allocBlock () {
    if(super.nfree == 1)
    {
        if(fillFreelist() != 1)
        {
            return 0;
        }
        else
        {
            super.nfree--;
            return super.free[super.nfree];
        }
    }
    else
    {
        printf("haha\n");
        super.nfree--;
        return super.free[super.nfree];
    }
}

unsigned int sizing (char size0, unsigned short size1, unsigned short flags) {
    unsigned int size = 0;
    unsigned int high = (size0 & 0xFF) << 16;
    unsigned int low = (size1 & 0xFFFF);
    unsigned int firstBit = (flags & 0x0200) << 15;
    size = high + low +firstBit;
    return size;
}

int checkFlag (unsigned short flags, enum inodeFlag iflag) {
    switch(iflag)
    {
        case ALLOC:
            return  (flags & 0x8000) == 0x8000;
            break;
            
        case PLAIN:
            return  (flags & 0x6000) == 0x6000 && ((flags & 0x0200) != 0x0200);
            break;
            
        case DIR:
            return ((flags & 0x4000) == 0x4000) && ((flags & 0x2000) != 0x2000) && ((flags & 0x0200) != 0x0200);
            break;
            
        case LARGE:
            return  (flags & 0x1200) == 0x1200;
            break;
    }
}

void setFlag (unsigned short flags, enum inodeFlag iflag) {
    switch(iflag)
    {
        case ALLOC:
            flags = flags | 0x8000;
            break;
            
        case PLAIN:
            flags = flags | 0x6000;
            flags = flags & ~(0x0200);
            break;
            
        case DIR:
            flags = flags | 0x4000; // 4 or 6
            flags = flags & ~(0x2000);// has to be 4
            flags = flags & ~(0x0200);
            break;
            
        case LARGE:
            flags = flags | 0x1200;
            break;
    }
}

int find_dirInode(char *directory, int dirInodeNo) {
    int i = 0, j = 0, k = 0, l = 0;
    
    unsigned short new_dirInodeNo, data_block_num, data_block, data_block_1;
    char *dir_name;
    inode dir_inode;
    
    lseek(fd, 2 * blockSize + dirInodeNo * 32, SEEK_SET);
    read(fd, &dir_inode, sizeof(inode));
    
    if(!(checkFlag(dir_inode.flags,DIR)))
    {
        printf("It is not a directory. \n");
        return 0;  //error
    }
    
    for(i = 0; i < 8 ; i++)
    {
        if(dir_inode.addr[i] == 0)
        {
            return 0;
        }
        
        data_block_num = dir_inode.addr[i];
        lseek(fd, blockSize * data_block_num, SEEK_SET);
        
        if(i == 0)
        {
            lseek(fd, 32, SEEK_CUR);      //first 32 bytes are for itself and parent dir
        }
        for(j = 0; j < ((blockSize/16) - 2); j++)
        {
            read(fd, &new_dirInodeNo, 2);
            if(new_dirInodeNo != 0)
            {
                read(fd, dir_name, 14);
                if(strcmp(dir_name, directory) == 0)
                {
                    return new_dirInodeNo;
                }
            }
            else
            {
                return 0;
            }
        }
    }
    return 0;
}

void addEntry (char *filename, unsigned short dirInodeNo, unsigned short file_inode_num) {
    int i = 0, j = 0, k = 0, l = 0;
    int inode_pos;
    unsigned short inode_num;
    unsigned short data_blocknum, data_blocknum_2, free_data_block;
    unsigned short inode_addr_data_block, addr_data_block;
    unsigned short data_block, data_block_1, data_block_2;
    unsigned short zero_block = 0;
    int addr_changed = 0;
    
    inode dir_inode;
    
    lseek(fd, (blockSize * 2) + ((dirInodeNo) * sizeof(inode)), SEEK_SET);//
    read(fd, &dir_inode, sizeof(inode));
    
    if(!checkFlag(dir_inode.flags,DIR))
    {
        printf("Entered name is not a directory. Command isn't executed.\n");//<=====================Stuck Here
        return;
    }
    
    for(i = 0; (dir_inode.addr[i] != 0) && (i < 7); i++){}
    
    data_blocknum = dir_inode.addr[--i];   //go to the last one entry
    
    
    if(i > 0)
    {
        j = 0;
        lseek(fd, blockSize * data_blocknum, SEEK_SET);
    }
    else
    {
        j = 2;
        lseek(fd, blockSize * data_blocknum + 32, SEEK_SET);
    }
    
    for( ; j < 32; j++)
    {
        read(fd, &inode_num, 2);
        if(inode_num == 0)
        {
            lseek(fd, -2, SEEK_CUR);
            write(fd, &file_inode_num, 2);
            write(fd, filename, 14);
            break;
        }
        else
        {
            lseek(fd, 14, SEEK_CUR);
        }
    }
    
    if(j != 31)
    {
        write(fd, &zero_block, 2);
    }
    else//j==31
    {
        if(i < 7)
        {
            dir_inode.addr[i+1] = allocBlock();
            lseek(fd, blockSize *  dir_inode.addr[i+1], SEEK_SET);
            write(fd, &zero_block, 2);
        }
        else//i=7 && j==31
        {
            printf("out of data blocks \n");
            return;
        }
    }
    
    //pushback directroy inode
    lseek(fd, (blockSize * 2) + (dirInodeNo * sizeof(inode)), SEEK_SET);
    write(fd, &dir_inode, sizeof(inode));
}

void initFS() {
    //initialize global variable
    // dataBlockNum, inodeBlockNum, freelistBlockNum;
    
    // initialize super block
    if(inodeNum % 16)
        super.isize = (inodeNum / 16) + 1;
    else
        super.isize = (inodeNum / 16);
    super.fsize = totalBlockNum;
    initFreelist();
    fillInodeList();
    
    lseek(fd, 1 * blockSize, SEEK_SET);
    write(fd, &super, sizeof(super));
    
    // Set length of file
    ftruncate(fd, super.fsize * blockSize);
    
    // writing zeroes to all inodes in ilist
    char buffer[blockSize];
    for (int i=0; i<blockSize; i++)
        buffer[i] = 0;
    lseek(fd, 2 * blockSize, SEEK_SET);
    for (int i=0; i < super.isize; i++)
        write(fd,buffer,blockSize);
    
    //Initialize the root data block in DIR format
    unsigned short inodeBlockNo = allocBlock();
    lseek(fd, ((2 + inodeBlockNo) * blockSize), SEEK_SET);
    
    inode rootInode;
    lseek(fd, (blockSize * 2), SEEK_SET);//
    read(fd, &rootInode, sizeof(inode));
    rootInode.addr[0] = inodeBlockNo;
    lseek(fd, blockSize * inodeBlockNo, SEEK_SET);
    char *c1 = ".";
    char *c2 = "..";
    unsigned short zero_block = 0;
    write(fd, &zero_block, 2);
    write(fd, c1, 14);
    write(fd, &zero_block, 2);
    write(fd, c2, 14);
    write(fd, &zero_block, 2);
    
    //create root directory inode
    setFlag(rootInode.flags,ALLOC);
    setFlag(rootInode.flags, DIR);
    rootInode.size0 = 0x00;
    rootInode.size1 = 0x0020;
    rootInode.addr[0] = inodeBlockNo;
    lseek(fd, blockSize*2, SEEK_SET);//the No. for rootInode is 0;
    inodeCurrent = 0;
    write(fd, &rootInode, sizeof(inode));
}

void mkdirectory (char* filename, unsigned short inum) {
    inode local;
    lseek(fd, 2 * blockSize + inum * sizeof(inode), SEEK_SET);
    read(fd, &local, sizeof(inode));


    setFlag(local.flags,ALLOC);
    setFlag(local.flags,DIR);
    lseek(fd, 2 * blockSize + (inum - 1) * sizeof(inode), SEEK_SET);
    write(fd, &local, sizeof(local));
    
    // Add to current directory
    addEntry(filename, inodeCurrent, inum);
    //initail a directory block
    lseek(fd, (blockSize * 2) + ((inum) * sizeof(inode)), SEEK_SET);//
    read(fd, &local, sizeof(inode));
    unsigned short data_blocknum = allocBlock();
    local.addr[0] = data_blocknum;
    lseek(fd, blockSize * data_blocknum, SEEK_SET);
    char *c1 = ".";
    char *c2 = "..";
    unsigned short zero_block = 0;
    write(fd, &inum, 2);
    write(fd, c1, 14);
    write(fd, &inodeCurrent, 2);
    write(fd, c2, 14);
    write(fd, &zero_block, 2);
    //pushback directroy inode
    lseek(fd, (blockSize * 2) + (inum * sizeof(inode)), SEEK_SET);
    write(fd, &local, sizeof(inode));
    
    printf("Directory created \n");
}

void createFile (char *src, char *dst) {
    //check the length of the file name
    if(strlen(dst) > 14)
    {
        printf("filename is greater than 14 bytes \n");
        return;
    }
    
    //check the number of file in the directory
    
    //check the external file existance
    int fp = open(src, O_RDONLY);
    if (fp<0)
    {
        printf("The file can't be opened.\n");
        return;
    }
    
    //check the input file size with 32MB
    unsigned long long cpinFileSize = lseek(fp, 0, SEEK_END)+1;
    close(fp);
    if(cpinFileSize > maxSize)
    {
        printf("File transfer of more than 32 MB is not supported in MyV6 Filesystem \n");
        return;
    }
    
    // srcname is a path+filename or just filename
    unsigned int size = 0;
    char extData[512];
    inode file_inode;
    int direct_addressing = 1;
    int indirect_addressing = 0;
    int double_indirect_addr = 0;
    int blockNoTemp;
    
    
    int i = 0, j = 0, k = 0, l = 0;
    
    
    ssize_t readSize, write_size;
    unsigned short data_block, data_block_2, data_block_3, data_block_test, data_block_2_test;
    unsigned short zero_block = 0;
    int inode_num;
    
    
    
    
    
    int extfd = open(src, O_RDONLY);
    
    while(1)
    {
        readSize = read(extfd, extData, 512);
        if(readSize == 0)
        {
            break;
        }
        else
        {
            size += readSize;
            if(size > maxSize)
            {
                printf("The copied file can't larger than 32 MB \n");
                break;
            }
            
            if(!indirect_addressing && !direct_addressing && !double_indirect_addr)
            {
                indirect_addressing = 1;
                blockNoTemp = allocBlock();
                lseek(fd, blockNoTemp * blockSize, SEEK_SET);
                for(j = 0; j < 8; j++)
                {
                    data_block = file_inode.addr[j];
                    file_inode.addr[j] = 0;
                    write(fd, &data_block, 2);
                }
                i = 0;
                k = 8;
                write(fd, &zero_block, 2);
                file_inode.addr[i] = blockNoTemp;
                file_inode.flags |= 0x1000;
            }
            
            
            blockNoTemp = allocBlock();
            if(blockNoTemp == 0)
            {
                printf("out of data blocks haha\n"); //<========stuck here
                return;
            }
            
            
            lseek(fd, blockNoTemp * blockSize, SEEK_SET);
            write_size = write(fd, extData, readSize);
            if(direct_addressing)
            {
                file_inode.addr[i++] = blockNoTemp;
                if(i > 7)
                {
                    direct_addressing = 0;
                }
            }
            if(indirect_addressing)
            {
                data_block = file_inode.addr[i];
                lseek(fd, data_block * blockSize +(k * 2), SEEK_SET);
                write(fd, &blockNoTemp, 2);
                k++;
                if(k > 255)
                {
                    data_block = allocBlock();
                    if(data_block == 0)
                    {
                        printf("Out of data blocks \n");
                        return;
                    }
                    lseek(fd, data_block * blockSize, SEEK_SET);
                    i++;
                    file_inode.addr[i] = data_block;
                    if(i == 7)
                    {
                        indirect_addressing = 0;
                        double_indirect_addr = 1;
                        data_block_2 = allocBlock();
                        lseek(fd, data_block * blockSize, SEEK_SET);
                        write(fd, &data_block_2, 2);
                        write(fd, &zero_block, 2);
                        lseek(fd, data_block_2 * blockSize,SEEK_SET);
                        write(fd, &zero_block, 2);
                        l = 0;
                        k = 0;
                        continue;
                    }
                    else
                    {
                        write(fd, &zero_block, 2);
                    }
                    k = 0;
                }
                else
                {
                    write(fd, &zero_block, 2);
                }
            }
            if(double_indirect_addr)
            {
                data_block = file_inode.addr[i];
                lseek(fd, data_block * blockSize +(k * 2), SEEK_SET);
                read(fd, &data_block_2, 2);
                lseek(fd, data_block_2 * blockSize + (l * 2), SEEK_SET);
                write(fd, &blockNoTemp, 2);
                l++;
                if(l > 255)
                {
                    k++;
                    if(k > 255)
                    {
                        printf("out of the space in the indirect address \n");
                        return;
                    }
                    data_block_3 = allocBlock();
                    if(data_block_3 == 0)
                    {
                        printf("Out of data blocks \n");
                        return;
                    }
                    lseek(fd, data_block_3 * blockSize, SEEK_SET);
                    write(fd, &zero_block, 2);
                    lseek(fd, data_block * blockSize +(k * 2), SEEK_SET);
                    write(fd, &data_block_3, 2);
                    write(fd, &zero_block, 2);
                    l = 0;
                }
                else
                {
                    write(fd, &zero_block, 2);
                }
            }
        }
    }
    
    //setup and inode for the file
    setFlag (file_inode.flags, ALLOC);
    file_inode.size1 = size & 0x0000FFFF;
    file_inode.size0 = (size >> 16) & 0x00FF;
    if(size > (16*1024*1024))
        setFlag (file_inode.flags, LARGE);
    else
        setFlag (file_inode.flags, PLAIN);
    int inum = allocInode();    // inode number recieved is considering root node as 0
    lseek(fd, 2 * blockSize + inum * sizeof(inode), SEEK_SET);
    write(fd, &file_inode, sizeof(inode));
    
    // create an enrty in the directory for the file
    addEntry(dst, inodeCurrent, inum);
    printf("The file is coped.");
    return;
}

void copyFileExt (char *srcname,char *targname) {
    int i = 0,j = 0, k = 0;
    unsigned short fileInode_num;
    inode fileInode;
    int data_block_num;
    unsigned short data_block_1, data_block_2,data_block_3;
    int ext_fd;
    
    ssize_t readSize, write_size, test_size;
    char data[512];
    
    fileInode_num = find_dirInode(srcname, inodeCurrent);
    if(fileInode_num == 0)
    {
        printf("Could not find the Inode of %s \n",srcname);
        return;
    }
    
    lseek(fd, (2 * blockSize) + (fileInode_num * sizeof(inode)), SEEK_SET);
    read(fd, &fileInode, sizeof(inode));
    
    ext_fd = open(targname, O_TRUNC | O_CREAT | O_RDWR, 0666);
    
    if(ext_fd < 0)
    {
        printf("Could not open/create external file \n");
        return;
    }
    
    unsigned int size = sizing(fileInode.size0, fileInode.size1, fileInode.flags);
    
    if(checkFlag(fileInode.flags, PLAIN))
    {
        for(i = 0; i < 8; i++)
        {
            if(fileInode.addr[i] == 0)
            {
                break;
            }
            
            
            data_block_num = fileInode.addr[i];
            lseek(fd, blockSize * data_block_num, SEEK_SET);
            if(size > 512)
            {
                readSize = read(fd, &data, blockSize);
                write_size = write(ext_fd, &data, readSize);
                size = size - write_size;
            }
            else
            {
                readSize = read(fd, &data, size);
                write_size = write(ext_fd, &data, readSize);
            }
        }
    }
    else
    {
        for(i = 0; i < 8; i++)
        {
            if(fileInode.addr[i] == 0)
            {
                break;
            }
            
            if(i < 7)
            {
                data_block_1 = fileInode.addr[i];
                for(j = 0; j < blockSize/2; j++)
                {
                    lseek(fd, (blockSize * data_block_1) + (j * 2), SEEK_SET);
                    read(fd, &data_block_2, 2);
                    if(data_block_2 == 0)
                    {
                        break;
                    }
                    lseek(fd, data_block_2 * blockSize, SEEK_SET);
                    if(size > 512)
                    {
                        readSize = read(fd, &data, blockSize);
                        write_size = write(ext_fd, &data, readSize);
                        size = size - write_size;
                    }
                    else
                    {
                        readSize = read(fd, &data, size);
                        write_size = write(ext_fd, &data, readSize);
                        size = size - write_size;
                    }
                }
            }
            else
            {
                data_block_1 = fileInode.addr[i];
                for(j = 0; j < blockSize/2; j++)
                {
                    lseek(fd, (blockSize * data_block_1) + (j * 2), SEEK_SET);
                    read(fd, &data_block_2, 2);
                    if(data_block_2 == 0)
                    {
                        break;
                    }
                    for(k = 0; k < blockSize/2; k++)
                    {
                        lseek(fd, (data_block_2 * blockSize) + (k * 2), SEEK_SET);
                        read(fd, &data_block_3, 2);
                        if(data_block_3 == 0)
                        {
                            break;
                        }
                        lseek(fd, data_block_3 * blockSize, SEEK_SET);
                        if(size > 512)
                        {
                            readSize = read(fd, &data, blockSize);
                            write_size = write(ext_fd, &data, readSize);
                            size = size - write_size;
                        }
                        else
                        {
                            readSize = read(fd, &data, size);
                            write_size = write(ext_fd, &data, readSize);
                            size = size - write_size;
                        }
                    }
                }
            }
        }
    }
    close(ext_fd);
    
    if(size == 0)
    {
        printf("copy completed. \n");
        return;
    }
    else
    {
        return;
    }
}
