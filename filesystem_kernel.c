
#define NULL 0

#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include <linux/string.h>

#include "filesystem_kernel.h"

static unsigned char *ramdisk;


#define RD_MEM_CAP (2 * 1024 * 1024)
#define MAX_INODES 1024
#define INODE_ARRAY_BLK_COUNT 256
#define BITMAP_BLK_COUNT 4
#define BLOCK_SIZE 256	
#define POINTER_SIZE 4	
#define PTR_PER_BLOCK  (BLOCK_SIZE / POINTER_SIZE)

#define TOTAL_DIRECT_BLK_PTRS 8
#define TOTAL_SINGLE_INDIR_BLK_PTRS 1
#define TOTAL_DOUBLE_INDIR_BLK_PTRS 1


#define MAX_BLOCK_COUNT_IN_FILE   (TOTAL_DIRECT_BLK_PTRS+ PTR_PER_BLOCK + PTR_PER_BLOCK * PTR_PER_BLOCK)
#define MAX_FILE_SIZE   (MAX_BLOCK_COUNT_IN_FILE * BLOCK_SIZE)


void ramdiskInitOperations() {

    superblock *superblock = NULL;
    indexNode *inodeArray = NULL;
    unsigned char *block_bitmap = NULL;

    //use vmalloc() to allocate 2MB for our ramdisk
    ramdisk = (unsigned char *)vmalloc(sizeof(unsigned char) * RD_MEM_CAP);

    //pointer to beginning of ramdisk
    superblock = (superblock *)ramdisk;
    setRamdisk(superblock, 0, BLOCK_SIZE);
    superblock->freeBlocks = RD_MEM_CAP / BLOCK_SIZE;
    superblock->freeINodes = MAX_INODES; 

    strcpy(superblock->first.type, "dir");

    //initialize index node array
    inodeArray = getINode(1);
    setRamdisk(inodeArray, 0, sizeof(unsigned char) * (BLOCK_SIZE * INODE_ARRAY_BLK_COUNT));

    //initialize block bitmap
    block_bitmap = (ramdisk + BLOCK_SIZE * (1 + INODE_ARRAY_BLK_COUNT));
    int i = 0;

    //set all blocks to free status
    for(int a = 0; a < BITMAP_BLK_COUNT; a++) {

      for(int b = 0; b < BLOCK_SIZE; b++) {
        block_bitmap[i++] = 0x0FF;
      }
    }

    for (int i = 0; i < (1 + INODE_ARRAY_BLK_COUNT + BITMAP_BLK_COUNT); i++){
      allocateOneBlock();
    } 
}

//uses bitmap to allocate one empty block
int allocateOneBlock() {
  int index = 0;
  int block_ptr = 0;
  unsigned char check = 0;
  superblock *superblock = NULL;
  unsigned char *block_bitmap = NULL;

  superblock = (superblock *)ramdisk;
  block_bitmap = (ramdisk + BLOCK_SIZE * (1 + INODE_ARRAY_BLK_COUNT));

  for(int i = 0; i < BITMAP_BLK_COUNT; i++) {

    for(int j = 0; j < BLOCK_SIZE; j++) {
      check = 1;

      for(int g = 0; g < 8; g++) {

        if ((block_bitmap[index] & mask) != 0) {
          block_bitmap[index] = block_bitmap[index] & (~mask);
          superblock->freeBlocks--;
          return blockPointer;
        }

        blockPointer++;
        mask = mask << 1;
      }
      index++;
    }
  }
  return -1;
}



//remove from memory
void uninitialize()
{
    vfree(ramdisk);
    ramdisk = NULL;
}


// return address of inode at inodeNum
struct inode *getINode(int inodeNum)
{
  if (inodeNum <= 0) { // root inode
    struct super_block *superblock = (superblock *)ramdisk;
    return &superblock->first;
  }
  
  struct inode *indexNode_array = (struct inode *)(ramdisk + BLOCK_SIZE); // calculate address of inode
  return indexNode_array + (inodeNum - 1);
}

// return mem address of bitmap
unsigned char *getBitmap()
{
  return (ramdisk + BLOCK_SIZE * (1 + INODE_ARRAY_BLK_COUNT));
}

// return address of block at block_ptr
char *getBlockAddress(int blockPointer)
{
  return (ramdisk + (BLOCK_SIZE * blockPointer));
}

// find dir index node for specified pathname
struct inode* getDirIndexNode(const char *pathname)
{
  struct inode *current_node = &(superblock *)ramdisk->first;

  // Skip leading '/'
  if (pathname[0] == '/')
  {
    pathname++;
  }

  const char *segment_start = pathname;
  const char *segment_end;

  for (;; segment_start = segment_end + 1)
  {
    segment_end = getNextDir(segment_start);

    // End of path reached
    if (segment_end == NULL)
    {
      break;
    }

    struct directory_entry *current_entry = getDirectory(current_node, segment_start, segment_end);

    // Child directory entry not found
    if (current_entry == NULL)
    {
      return NULL;
    }

    current_node = getINode(current_entry->inodeNum);

    // Not a directory
    if (strcmp(current_node->type, "dir") != 0)
    {
      return NULL;
    }
  }

  return current_node;
}


//using a defined absolute path, get the next directory referenced
static const char* getNextDir(const char* path) {
    const char* current = path;

    while ('/' == current[0]) {
        current++;
    }

    // find the next '/'
    while ('\0' != current[0] && '/' != current[0]) {
        current++;
    }

    // return the next directory or NULL if the end of the path is reached
    return ('\0' != current[0]) ? current : NULL;
}



// find child directory entry by its name
struct directory_entry *getDirectory(struct inode *indexNode, const char *fnameStart, const char *fnameEnd)
{

  struct file_posn filePosition;
  initFilePosn(&filePosition, indexNode, 0, 1);

  while (filePosition.filePosition < indexNode->size) { // iterate thru directory file to the end
    struct directory_entry *entry = (struct directory_entry *)getMemAddress(&filePosition);
    if (NULL == entry) {
      break;
    }

    filePosnAdjust(&filePosition, sizeof(struct directory_entry)); // add fild posn by size of directory entry

    if (NULL == fnameEnd) { // if directory found with same name, return
      if (0 == strcmp(fnameStart, entry->filename)) {
        return entry;
      }
    } else {
      if (0 == strncmp(entry->filename, fnameStart, fnameEnd)) {
        return entry;
      }
    }
  }

  return NULL;
}

//used a defined absolute path, get a specific filename
const char* UsingPathGetFileName(const char* pathname)
{
    // find the last occurrence of '/'
    const char* lastSlash = strrchr(pathname, '/');

    // set fnameStart accordingly
    const char* fnameStart = (lastSlash != NULL) ? (lastSlash + 1) : pathname;

    return fnameStart;
}

void freeINodeMem(struct inode *indexNode)
{
  int blockVal = 0;
  int *indirectLoc = NULL;
  struct blk_ptr blockPointer;

  initBlockPtr(&blockPointer, indexNode, 0, 1);

  // Free direct and indirect blocks
  for (int i = 0; i < MAX_BLOCK_COUNT_IN_FILE; i++)
  {
    blockVal = getBlkPtr(&blockPointer);
    if (blockVal <= 0)
    {
      break;
    }
    freeBlock(blockVal);
    blockPtrIncrease(&blockPointer);
  }

  // Free single indirect block
  if (indexNode->location[TOTAL_SINGLE_INDIR_BLK_PTRS] > 0)
  {
    freeBlock(indexNode->location[TOTAL_SINGLE_INDIR_BLK_PTRS]);
  }

  // Free double indirect blocks
  if (indexNode->location[TOTAL_DOUBLE_INDIR_BLK_PTRS] > 0)
  {
    indirectLoc = (int *)getBlockAddress(indexNode->location[TOTAL_DOUBLE_INDIR_BLK_PTRS]);
    for (i = 0; i < PTR_PER_BLOCK; i++)
    {
      if (indirectLoc[i] <= 0)
      {
        break;
      }
      freeBlock(indirectLoc[i]);
    }
    freeBlock(indexNode->location[TOTAL_DOUBLE_INDIR_BLK_PTRS]);
  }

  // Reset locations and size
  for (i = 0; i < 10; i++)
  {
    indexNode->location[i] = 0;
  }

  indexNode->size = 0;
  indexNode->dirCount = 0;
}


// find available inode, return -1 if none
int getAvailableNode() {
    struct inode *indexNode_array = NULL;

    struct super_block *superblock = (superblock *)ramdisk;
    if (superblock->freeINodes <= 0) return -1; // check for free inodes

    indexNode_array = getINode(1);

    for (int i = 0; i < MAX_INODES; i++) {
        // empty string == available node
        if (indexNode_array[i].type[0] == '\0') {
            superblock->freeINodes--;
            return (i + 1);
        }
    }

    return -1; // no node found
}

// find empty child directory entry under parent
struct directory_entry *findEmptyDirEntry(struct inode *indexNode)
{
  struct directory_entry *entry = NULL;
  struct file_posn filePosition;

  initFilePosn(&filePosition, indexNode, 0, 1);

  while (filePosition.filePosition < indexNode->size) // iterate thru directory file to the end
  {
    entry = (struct directory_entry *)getMemAddress(&filePosition);
    if (NULL == entry)
    {
      break;
    }

    filePosnAdjust(&filePosition, sizeof(struct directory_entry)); // add fild posn by size of directory entry

    if (0 == strcmp(entry->filename, "")) // return if empty directory found
    {
      return entry;
    }
  }

  return NULL;
}

//use file position object to get the respective memory address corresponding to it
char *getMemAddress(struct file_posn *filePosition)
{
  //gets block pointer value from file position block pointer ref
  int blockPtrValue = getBlockPtrValue(&filePosn->blockPointer);
  if (blockPtrValue <= 0)
  {
    return NULL;
  }
  return getBlockAddress(blockPtrValue) + filePosn->dataBlockOffset;
}


//allocate free block and clear mem to zero, return -1 if none
int clearAllocateBlock() {
    int blockPointer = allocateOneBlock();
    if (blockPointer > 0) {
        // clear the block's mem to zero
        memset(getBlockAddress(blockPointer), 0, BLOCK_SIZE);
    }

    return blockPointer;
}

// sets used block to free, frees to the list
void freeBlock(int blockPointer) {
    int index = blockPointer / 8;
    int k = blockPointer % 8;
    int mask = 1 << k;

    struct super_block *superblock = (superblock *)ramdisk;
    unsigned char *block_bitmap = getBitmap();

    // if bitmap mask bit of the corresponding block is 0, set it to 1
    if ((block_bitmap[index] & mask) == 0) {
        block_bitmap[index] |= mask;
        superblock->freeBlocks++;
    }
}


// add directory entry to specified parent directory
int addToParentDir(struct inode *indexNode, struct directory_entry *entry)
{
  // if empty directory in dir file inode, use it for the inode & return
  if ((indexNode->dirCount * sizeof(struct directory_entry)) < ((unsigned int)indexNode->size))
  {
    memcpy(findEmptyDirEntry(indexNode), entry, sizeof(struct directory_entry));
    indexNode->dirCount++;
    return 0;
  }

  if ((indexNode->size + sizeof(struct directory_entry)) > MAX_FILE_SIZE) { // parent directory size exceeds max file size
    return -1;
  }
  struct file_posn filePosition;
  initFilePosn(&filePosition, indexNode, indexNode->size, 0);
  char *availablePosn = getMemAddress(&filePosition);
  if (NULL == availablePosn) { // check if available space
    return -1;
  }
  // add dir to end of parent directory file
  memcpy(availablePosn, entry, sizeof(struct directory_entry));
  indexNode->size = indexNode->size + sizeof(struct directory_entry);
  indexNode->dirCount++;

  return 0;
}



//use file position object to get the respective memory address corresponding to it
char *getMemAddress(struct file_posn *filePosition)
{
  //gets block pointer value from file position block pointer ref
  int blockPtrValue = getBlockPtrValue(&filePosn->blockPointer);
  if (blockPtrValue <= 0)
  {
    return NULL;
  }
  return getBlockAddress(blockPtrValue) + filePosn->dataBlockOffset;
}

// initialize file posn data struct
void initFilePosn(struct file_posn *filePosition, struct inode *indexNode, int pos, int readOnly)
{
  int block_number = pos / BLOCK_SIZE;
  filePosition->filePosition = pos;

  //use blick number and pointer information to init block pointer
  initBlockPtr(&filePosition->blockPointer,
    indexNode,
    block_number,
    readOnly);

  //uses the position variable to offset the block data structure
  filePosition->dataBlockOffset = pos % BLOCK_SIZE;
}

//goes through and updates offset into file position and also into the block offset
void filePosnAdjust(struct file_posn *filePosition, int offset)
{
   // Updates file position and offset into data block
  filePosn->filePosition = filePosn->filePosition + offset;
  filePosn->dataBlockOffset = filePosn->dataBlockOffset + offset;

  // Checks if the offset exceeds the block size, requiring an increase in block pointer
  if (filePosn->dataBlockOffset >= BLOCK_SIZE)
  {
    increaseBlockPointer(&filePosn->blockPointer);
    filePosn->dataBlockOffset = filePosn->dataBlockOffset - BLOCK_SIZE;
  }
}

//primary function for initializing each of our block pointer data structures
void initBlockPtr(struct blk_ptr*blockPointer, struct inode *indexNode, int block_number, int readOnly) {

  //clear the block pointer structure
  memset(blockPointer, 0, sizeof(blockPointer_t));
  blockPointer->readOnly = readOnly;
  blockPointer->indexNode = indexNode;

  //determine block pointer type based on the block numbers
  if (block_number < dirBlkPtrCoUNT)
  {
    blockPointer->blkPtrType = directBlkPtr;
    blockPointer->dirBlkPtr = block_number;
  }
  else if (block_number < (directBlkPtrS+ PTR_PER_BLOCK))
  {
    blockPointer->blkPtrType = singleIndirectBlkPtr;
    blockPointer->singleIndirBlkPtr = block_number - dirBlkPtrCoUNT;
  }
  //need to use double indirect block pointers here
  else
  {
    blockPointer->blkPtrType = doubleIndirectBlkPtr;
    blockPointer->doubleIndirBlkPtrRow = (block_number - (directBlkPtrS+ PTR_PER_BLOCK)) / PTR_PER_BLOCK;
    blockPointer->doubleIndirBlkPtrColumn = (block_number - (directBlkPtrS+ PTR_PER_BLOCK)) % PTR_PER_BLOCK;
  }
}

//helper method whenever a specific block pointer might need to be increased because of the offset
void blockPtrIncrease(struct blk_ptr*blockPointer)
{
  
  //check to determine whether to increase direct block pointer
  if (directBlkPtr == blockPointer->blkPtrType)
  {
    blockPointer->dirBlkPtr++;
    
    //if we are full with direct pointer, we need to start using single indirect
    if (directBlkPtrS== blockPointer->dirBlkPtr)
    {
      blockPointer->blkPtrType = singleIndirectBlkPtr;
      blockPointer->singleIndirBlkPtr = 0;
    }
  }
  //check once again to determine whether or not to increase single indirect block pointer
  else if (singleIndirectBlkPtr == blockPointer->blkPtrType)
  {
    blockPointer->singleIndirBlkPtr++;

    //if single indirect full, need to move to double indirect
    if (PTR_PER_BLOCK == blockPointer->singleIndirBlkPtr)
    {
      blockPointer->blkPtrType = doubleIndirectBlkPtr;
      blockPointer->doubleIndirBlkPtrRow = 0;
      blockPointer->doubleIndirBlkPtrColumn = 0;
    }
  }
  
  else if (doubleIndirectBlkPtr == blockPointer->blkPtrType)
  {
    blockPointer->doubleIndirBlkPtrColumn++;
  
    if (PTR_PER_BLOCK == blockPointer->doubleIndirBlkPtrColumn)
    {
      blockPointer->doubleIndirBlkPtrRow++;
      blockPointer->doubleIndirBlkPtrColumn = 0;
    }
  }
}

//helps us with allocating the necessary required memory in blocks for our pointers to store data
int getBlkPtr(struct blk_ptr*blockPointer)
{
  int blockPointer_index = 0;

  int *location = blockPointer->indexNode->location;
  if (singleIndirectBlkPtr == blockPointer->blkPtrType)
  {
    
    //allocate required memory if type is a single indirect block pointer
    if (0 == location[TOTAL_SINGLE_INDIR_BLK_PTRS])
    {
      //dont proceed further if someone is reading it
      if (blockPointer->readOnly)
      {
        return -1;
      }
      else
      {
        //fail if no more memory as well
        location[TOTAL_SINGLE_INDIR_BLK_PTRS] = clearAllocateBlock();
        if (location[TOTAL_SINGLE_INDIR_BLK_PTRS] <= 0)
        {
          return -1;
        }
      }
    }
    location = (int *)getBlockAddress(location[TOTAL_SINGLE_INDIR_BLK_PTRS]);
  }
  else if (doubleIndirectBlkPtr == blockPointer->blkPtrType)
  {

    //same logic as above but for double indirect block pointers
    if (0 == location[TOTAL_DOUBLE_INDIR_BLK_PTRS])
    {
      //dont proceed further if someone reading it
      if (blockPointer->readOnly)
      {
        return -1;
      }
      else
      {
      
        //if dont have enough space, fail
        location[TOTAL_DOUBLE_INDIR_BLK_PTRS] = clearAllocateBlock();
        if (location[TOTAL_DOUBLE_INDIR_BLK_PTRS] <= 0)
        {
          return -1;
        }
      }
    }
    location = (int *)getBlockAddress(location[TOTAL_DOUBLE_INDIR_BLK_PTRS]);
    if (0 == location[blockPointer->doubleIndirBlkPtrRow])
    {
      if (blockPointer->readOnly)
      {
        return -1;
      }
      else
      {
        location[blockPointer->doubleIndirBlkPtrRow] = clearAllocateBlock();
        if (location[blockPointer->doubleIndirBlkPtrRow] <= 0)
        {
          return -1;
        }
      }
    }
    location = (int *)getBlockAddress(location[blockPointer->doubleIndirBlkPtrRow]);
  }

 // Check the type of block pointer
if (directBlkPtr == blockPointer->blockPointerType)
{
  // If it's a direct block pointer, get the corresponding block index
  blockPointer_index = blockPointer->directBlockPointer;
}
else if (singleIndirectBlkPtr == blockPointer->blockPointerType)
{
  // If it's a single-indirect block pointer, get the corresponding block index
  blockPointer_index = blockPointer->singleIndirectBlockPointer;
}
else if (doubleIndirectBlkPtr == blockPointer->blockPointerType)
{
  // If it's a double-indirect block pointer, get the corresponding column index
  blockPointer_index = blockPointer->doubleIndirBlkPtrColumn;
}

// Check if the block at the determined index is not allocated
if (0 == location[blockPointer_index])
{
  // If it's read-only, return -1 as it's not allowed to allocate for reading
  if (blockPointer->readOnly)
  {
    return -1;
  }
  else
  {
    // Allocate one block for writing data
    location[blockPointer_index] = allocateOneBlock();
    
    // If allocation fails, return -1
    if (location[blockPointer_index] <= 0)
    {
      return -1;
    }
  }
}

// Return the block pointer value of the corresponding block for reading or writing data
return location[blockPointer_index];
}




