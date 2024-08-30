#ifndef _KERNEL_STRUCTS_H
#define _KERNEL_STRUCTS_H


// INDEX NODE STRUCT
struct inode {
  char type[4];
  int size;
  int location[10];
  int dirCount;
  int filesOpen;
  char padding[8];
} 

// SUPERBLOCK STRUCT 
struct super_block {
  int freeBlocks;
  int freeINodes;
  struct inode first;
} 


// DIRECTORY ENTRY STRUCT
struct directory_entry {
  char filename[14];
  short inodeNum;
}

//signify type of block ptr
enum blk_ptr_type {
  directBlkPtr = 1,
  singleIndirectBlkPtr = 2,
  doubleIndirectBlkPtr = 3
} 


// BLOCK PTR STRUCT
blk_ptr {
  int readOnly;
  enum blk_ptr_type blkPtrType;
  int dirBlkPtr;
  int singleIndirBlkPtr;
  int doubleIndirBlkPtrRow;
  int doubleIndirBlkPtrColumn;
  struct inode *indexNode;

}

// FILE POSN STRUCT
struct file_posn {
  int filePosition;
  int dataBlockOffset;
  struct blk_ptr blockPointer;
}

#endif