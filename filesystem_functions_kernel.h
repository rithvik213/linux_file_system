#include "filesystem_kernel.c"

// kernel function to create file
static int rd_creat_kernel(char *path) {
  struct inode *parent_inode = getDirIndexNode(path);

  // error check: If directory does not exist
  if (parent_inode == NULL) {
    return -1;
  }

  // error check: check if the path specified by the user already exists
  const char *filename = UsingPathGetFileName(path);
  if (getDirectory(parent_inode, filename, NULL) != NULL) {
    return -1;
  }

  // error check: check if there is a free inode for the file
  int inodeNum = getAvailableNode();
  if (inodeNum == -1) {
    return -1;
  }

  // initialize the new index node
  struct inode *indexNode = getINode(inodeNum);
  memset(indexNode, 0, sizeof(struct inode));
  strcpy(indexNode->type, "reg");

  // create and update directory entry
  struct directory_entry entry;
  strcpy(entry.filename, filename);
  entry.inodeNum = inodeNum;
  if (addToParentDir(parent_inode, &entry) != 0) {
    return -1;
  }

  return 0;
}

// create directory
static int rd_mkdir_kernel(char *path)
{
  // error check if parent directory exists
  struct inode *parent_inode = getDirIndexNode(path);
  if (NULL == parent_inode) {
    return -1;
  }
  //error check if directory already exists
  const char *directory_name = UsingPathGetFileName(path);
  if (NULL != getDirectory(parent_inode, directory_name, NULL)) {
    return -1;
  }
  // error check for any available nodes
  int inodeNum = getAvailableNode();
  if (-1 == inodeNum) {
    return -1;
  }
  // initialize directory node
  struct inode *indexNode = getINode(inodeNum);
  memset(indexNode, 0, sizeof(struct inode));
  strcpy(indexNode->type, "dir");

  struct directory_entry entry;
  strcpy(entry.filename, directory_name); // update parent directory file for new entry
  entry.inodeNum = inodeNum;
  if (0 != addToParentDir(parent_inode, &entry)) {
    return -1;
  }

  return 0;
}


// open file at path
static int rd_open_kernel(char *path, int *inodeNum) {
  // error check if root directory
  if ((0 == strcmp("/", path)) || (0 == strcmp("", path))) {
    *inodeNum = 0;
    struct super_block *superblock = (superblock *)ramdisk;
    superblock->first.filesOpen++;
    *inodeNum = 0;
    return 0;
  }

  // error check: if parent directory exists
  struct inode *parent_inode = getDirIndexNode(path);
  if (NULL == parent_inode) {
    return -1;
  }

  // error check if path exists
  const char *filename = UsingPathGetFileName(path);
  struct directory_entry *entry = getDirectory(parent_inode, filename, NULL);
  if (NULL == entry) {
    return -1;
  }

  // assign node number & update vars
  *inodeNum = entry->inodeNum;
  struct inode *indexNode = getINode(entry->inodeNum);
  indexNode->filesOpen++;

  return 0;
}


// close file at fd
static int rd_close_kernel(int inodeNum) {
  struct inode *indexNode = NULL; // set to null, update vars
  indexNode = getINode(inodeNum);
  indexNode->filesOpen--;

  return 0;
}

// read bytes of the file at fd, into the specified address
static int rd_read_kernel(int inodeNum, int pos, char *address, int numBytes)
{
  // error check, if node is a directory
  struct inode *indexNode = getINode(inodeNum);
  if (0 != strcmp("reg", indexNode->type)) { return -1; }
  struct file_posn filePosition;
  
  if (indexNode->size-pos < numBytes) { // check if byte num is greater than file size
    num bytes = indexNode->size-pos;
  }
  initFilePosn(&filePosition, indexNode, pos, 1);
  char *availablePosn = address;
  int readDataRemainderLen = numBytes;

  int readDataLen = 0;
  int currReadDataLen = 0;
  while (readDataRemainderLen > 0) { // read data by the block
    int blockDataRemainderLen = BLOCK_SIZE - filePosition.dataBlockOffset;
    if (blockDataRemainderLen < readDataRemainderLen) {
      currReadDataLen = blockDataRemainderLen;
    } else {
      currReadDataLen = readDataRemainderLen;
    }
    char *src = getMemAddress(&filePosition);
    if (NULL == src) {
      break;
    }

    copy_to_user(availablePosn, src, currReadDataLen); // copy from ramdisk to user
    readDataLen = readDataLen + currReadDataLen;
    availablePosn = availablePosn + currReadDataLen;
    readDataRemainderLen = readDataRemainderLen - currReadDataLen;
    filePosnAdjust(&filePosition, currReadDataLen);
  }

  return readDataLen;
}

// write from address to file, up to the numBytes
static int rd_write_kernel(int inodeNum, int pos, char *address, int numBytes)
{

  //error check if node is a directory
  struct inode *indexNode = getINode(inodeNum);
  if (0 != strcmp("reg", indexNode->type)) {
    return -1;
  }

  if (MAX_FILE_SIZE - pos < numBytes) { // calculations of partition
    numBytes = MAX_FILE_SIZE - pos;
  }
  struct file_posn filePosition;
  if (numBytes > 0) {
    initFilePosn(&filePosition, indexNode, pos, 0); // initialize file position
  }
  char *src = address;
  int writeDataRemainderLen = numBytes;
  int dataWrittenLen = 0;
  int currWriteDataRemain = 0;

  while (writeDataRemainderLen > 0) { // read data by block
    int blkSpaceRemaining = BLOCK_SIZE - filePosition.dataBlockOffset;
    if (blkSpaceRemaining < writeDataRemainderLen) {
      currWriteDataRemain = blkSpaceRemaining;
    } else {
      currWriteDataRemain = writeDataRemainderLen;
    }
    char *availablePosn = getMemAddress(&filePosition);
    if (NULL == availablePosn) {
      // no space left to write
      break;
    }
    copy_from_user(availablePosn, src, currWriteDataRemain); // copy from user to kernel
    dataWrittenLen = dataWrittenLen + currWriteDataRemain;
    src = src + currWriteDataRemain;
    writeDataRemainderLen = writeDataRemainderLen - currWriteDataRemain;
    if (writeDataRemainderLen > 0) {
      filePosnAdjust(&filePosition, currWriteDataRemain);
    }
  }
  indexNode->size = (pos + dataWrittenLen > indexNode->size) ? (pos + dataWrittenLen) : indexNode->size;

  return dataWrittenLen;
}

// set file position to offset, return new position
static int rd_lseek_kernel(int inodeNum, int offset, int *offset_toReturn) {
  // error check: if directory
  struct inode *indexNode = getINode(inodeNum);
  if (0 != strcmp("reg", indexNode->type)) {
    return -1;
  }

  if (offset < 0) {
    *offset_toReturn = 0; // offset below zero, return beginning
  } else if (offset > indexNode->size) {
    *offset_toReturn = indexNode->size; // offset greater than end of file, return end
  } else {
    *offset_toReturn = offset;
  }

  return 0;
}



// unlink file at path, free memory
static int rd_unlink_kernel(char *path) {
  // error check: if root directory, return -1
  if ((0 == strcmp("", path)) || (0 == strcmp("/", path))) {
    return -1;
  }

  // Error check: If parent directory exists
  struct inode *parent_inode = getDirIndexNode(path);
  if (NULL == parent_inode) {
    return -1;
  }

  // error check: if path exists
  const char *filename = UsingPathGetFileName(path);
  struct directory_entry *entry = getDirectory(parent_inode, filename, NULL);
  if (NULL == entry) {
    return -1;
  }

  // error check: if directory has contents, return -1
  struct inode *indexNode = getINode(entry->inodeNum);
  if ((0 == strcmp("dir", indexNode->type)) && (indexNode->dirCount > 0)) {
    return -1;
  }

  // error check: if file is open, return -1
  if (indexNode->filesOpen > 0) {
    return -1;
  }

  // free inode memory, return to unused, remove directory entry
  freeINodeMem(indexNode);
  memset(indexNode, 0, sizeof(struct inode));
  struct super_block *superblock = (superblock *)ramdisk;
  superblock->freeINodes++;
  memset(entry, 0, sizeof(struct directory_entry));
  parent_inode->dirCount--;

  // free parent if it has no directory entries
  if (0 == parent_inode->dirCount) {
    freeINodeMem(parent_inode);
  }

  return 0;
}

// read a single entry from directory at fd, store at address
static int rd_readdir_kernel(int inodeNum, char *address, int *pos)
{
  // error check if regular file
  struct inode *indexNode = getINode(inodeNum);
  if (0 != strcmp("dir", indexNode->type)) {
    return -1;
  }
  // initialize needed struct
  struct file_posn filePosition;
  initFilePosn(&filePosition, indexNode, *pos, 1);

  while (filePosition.filePosition < indexNode->size) { // iterate through directory to specified posn
    struct directory_entry *entry = (struct directory_entry *)getMemAddress(&filePosition);
    if (NULL == entry) {
      break;
    }

    filePosnAdjust(&filePosition, sizeof(struct directory_entry)); // increases by 16

    if (0 != strcmp(entry->filename, "")) {
      memcpy(address, entry, sizeof(struct directory_entry));
      *pos = filePosition.filePosition;
      return 1;
    }
  }
  
  // increment posn, end of file
  *pos = filePosition.filePosition;

  return 0;
}





