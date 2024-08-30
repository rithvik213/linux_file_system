


#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <stdlib.h>
#include <string.h>
#include "filesystem_function_kernel.h"
#include "filesystem_structs.h"

#ifndef _RD_FUNCTIONS
#define _RD_FUNCTIONS

#define RD_CREAT _IOWR(0, 11, struct pathParam)
#define RD_UNLINK _IOWR(0, 12, struct pathParam)
#define RD_OPEN _IOWR(0, 13, struct openParam)
#define RD_CLOSE _IOWR(0, 14, struct closeParam)
#define RD_READ _IOWR(0, 15, struct rwParam)
#define RD_WRITE _IOWR(0, 16, struct rwParam)
#define RD_LSEEK _IOWR(0, 17, struct lseekParam)
#define RD_MKDIR _IOWR(0, 18, struct pathParam)
#define RD_READDIR _IOWR(0, 19, struct readdirParam)

#endif

struct fileDescriptor
{
  int fd;
  int inodeNum;
  int filePosition;
};

// file descriptor functions
static void addToFDTable(struct fileDescriptor *fileDescriptor);
static void removeFromFDTable(struct fileDescriptor *fileDescriptor);
static struct fileDescriptor *FDSearch(int fd);



static int currentFD = 1;



// function to create file
static int rd_creat(char *path)
{
  int fd = open("/proc/ramdisk", O_RDONLY); // open proc/ramdisk 
  if (fd < 0)
  {
    return -1;
  }
  struct pathParam creatParams;
  creatParams.returnVal = -1;
  creatParams.path = (const char *)path;
  creatParams.pathLen = (int)strlen(path);
 
  int ret = ioctl(fd, RD_CREAT, &creatParams); // ioctl call
  close(fd);
  if (ret != 0) {
    return -1;
  }
  if (creatParams.returnVal != 0) {
    return creatParams.returnVal;
  }

  return 0;
}


// unlink file at path, free memory
static int rd_unlink(char *path)
{
  int fd = open("/proc/ramdisk", O_RDONLY); // open proc/ramdisk 
  if (fd < 0) {
    return -1;
  }
  struct pathParam unlinkParams;
  unlinkParams.returnVal = -1;
  unlinkParams.path = (const char *)path;
  unlinkParams.pathLen = (int)strlen(path);

  int ret = ioctl(fd, RD_UNLINK, &unlinkParams); // ioctl
  close(fd);
  if (ret != 0) {
    return -1;
  }
  if (unlinkParams.returnVal != 0) {
    return unlinkParams.returnVal;
  }

  return 0;
}

// open file at path
static int rd_open(char *path) {
  int fd_ioctl = open("/proc/ramdisk", O_RDONLY);

  if (fd_ioctl < 0) {
    return -1;
  }

  struct openParam openParams = {
    .returnVal = -1,
    .path = (const char *)path,
    .pathLen = (int)strlen(path)
  };

  if (ioctl(fd_ioctl, RD_OPEN, &openParams) != 0) {
    close(fd_ioctl);
    return -1;
  }

  close(fd_ioctl);

  if (openParams.returnVal != 0) {
    return -1;
  }

  int inodeNum = openParams.inodeNum;

  // create file descriptor for open file
  struct fileDescriptor *fileDescriptor = (struct fileDescriptor *)malloc(sizeof(struct fileDescriptor)); 
  fileDescriptor->inodeNum = inodeNum;
  fileDescriptor->fd = currentFD++;
  fileDescriptor->filePosition = 0;
  addToFDTable(fileDescriptor); 

  return fileDescriptor->fd;
}



// close file at fd
static int rd_close(int fd) {
  struct fileDescriptor *fileDescriptor = FDSearch(fd);

  if (NULL == fileDescriptor) {
    return -1;
  }

  int fd_ioctl = open("/proc/ramdisk", O_RDONLY);

  if (fd_ioctl < 0) {
    return -1; 
  }

  struct closeParam closeParams = {
    .returnVal = -1,
    .inodeNum = fileDescriptor->inodeNum
  };

  if (ioctl(fd_ioctl, RD_CLOSE, &closeParams) != 0) {
    close(fd_ioctl);
    return -1;
  }

  close(fd_ioctl);

  removeFromFDTable(fileDescriptor);
  free(fileDescriptor);

  return 0;
}


// read bytes of the file at fd, into the specified address
static int rd_read(int fd, char *address, int numBytes) {
  struct fileDescriptor *fileDescriptor = FDSearch(fd);

  if (NULL == fileDescriptor) {
    return -1;
  }

  int fd_ioctl = open("/proc/ramdisk", O_RDONLY);

  if (fd_ioctl < 0) {
    return -1;
  }

  struct rwParam readParams = {
    .returnVal = -1,
    .inodeNum = fileDescriptor->inodeNum,
    .filePosition = fileDescriptor->filePosition,
    .address = address,
    .numBytes = numBytes
  };

  if (ioctl(fd_ioctl, RD_READ, &readParams) != 0) {
    close(fd_ioctl);
    return -1;
  }

  close(fd_ioctl);

  if (readParams.returnVal < 0) {
    return -1;
  }

  fileDescriptor->filePosition += readParams.returnVal;

  return readParams.returnVal;
}


// write from address to file, up to the numBytes
static int rd_write(int fd, char *address, int numBytes) {
  struct fileDescriptor *fileDescriptor = FDSearch(fd);

  if (NULL == fileDescriptor) {
    return -1;
  }

  int fd_ioctl = open("/proc/ramdisk", O_RDONLY);

  if (fd_ioctl < 0) {
    return -1;
  }

  struct rwParam writeParams = {
    .returnVal = -1,
    .inodeNum = fileDescriptor->inodeNum,
    .filePosition = fileDescriptor->filePosition,
    .address = address,
    .numBytes = numBytes
  };

  if (ioctl(fd_ioctl, RD_WRITE, &writeParams) != 0) {
    close(fd_ioctl);
    return -1;
  }

  close(fd_ioctl);

  if (writeParams.returnVal < 0) {
    return -1;
  }

  fileDescriptor->filePosition += writeParams.returnVal;

  return writeParams.returnVal;
}


// set file position to offset, return new position
static int rd_lseek(int fd, int offset) {
  struct fileDescriptor *fileDescriptor = FDSearch(fd);

  if (NULL == fileDescriptor) {
    return -1;
  }

  int fd_ioctl = open("/proc/ramdisk", O_RDONLY);

  if (fd_ioctl < 0) {
    return -1;
  }

  struct lseekParam lseekParams = {
    .returnVal = -1,
    .inodeNum = fileDescriptor->inodeNum,
    .offset = offset,
    .offset_toReturn = -1
  };

  if (ioctl(fd_ioctl, RD_LSEEK, &lseekParams) != 0) {
    close(fd_ioctl);
    return -1;
  }

  close(fd_ioctl);

  if (lseekParams.returnVal != 0) {
    return -1;
  }

  fileDescriptor->filePosition = lseekParams.offset_toReturn;

  return 0;
}



// create directory
static int rd_mkdir(char *path) {
  int fd = open("/proc/ramdisk", O_RDONLY);

  if (fd < 0) {
    return -1;
  }

  struct pathParam mkdirParams = {
    .returnVal = -1,
    .path = (const char *)path,
    .pathLen = (int)strlen(path)
  };

  if (ioctl(fd, RD_MKDIR, &mkdirParams) != 0) {
    close(fd);
    return -1;
  }

  close(fd);

  if (mkdirParams.returnVal != 0) {
    return mkdirParams.returnVal;
  }

  return 0;
}


// read a single entry from directory at fd, store at address
static int rd_readdir(int fd, char *address) {
  if (NULL == address) {
    return -1;
  }

  struct fileDescriptor *fileDescriptor = FDSearch(fd);
  if (NULL == fileDescriptor) {
    return -1;
  }

  int fd_ioctl = open("/proc/ramdisk", O_RDONLY);
  if (fd_ioctl < 0) {
    return -1;
  }

  struct readdirParam readdirParams = {
    .returnVal = -1,
    .inodeNum = fileDescriptor->inodeNum,
    .filePosition = fileDescriptor->filePosition
  };

  if (ioctl(fd_ioctl, RD_READDIR, &readdirParams) != 0) {
    close(fd_ioctl);
    return -1;
  }

  close(fd_ioctl);

  if (readdirParams.returnVal < 0) {
    return readdirParams.returnVal;
  }

  if (readdirParams.returnVal > 0) {
    memcpy(address, readdirParams.address, readdirParams.dirDataLen);
  }

  fileDescriptor->filePosition = readdirParams.filePosition;

  return readdirParams.returnVal;
}


//file descriptor table
static struct fileDescriptor fileDescriptors[1023];
static struct fileDescriptor emptyDescriptor = {0};
static int fdStart = -1;
static int fdEnd = -1;


// add file descriptor to array
static void addToFDTable(struct fileDescriptor *fileDescriptor) {
  if (fdStart == -1) { // empty list
    fdStart = 0;
    fdEnd = 0;
  } else {
    fdEnd++;
  }
  fileDescriptors[fdEnd] = *fileDescriptor;
}

// find file descriptor
static struct fileDescriptor *FDSearch(int fd)
{
  for (int i = fdStart; i <= fdEnd; i++) {
    if (fileDescriptors[i].fd == fd) {
      return &fileDescriptors[i];
    }
  }

  return NULL; // not found
}

// remove file descriptor from array
static void removeFromFDTable(struct fileDescriptor *fileDescriptor) {
    if (fdStart == -1 || fdEnd == -1) { // List is empty
        return;
    }

    int foundIndex = -1;

    // find the index of the file descriptor to be removed
    for (int i = fdStart; i <= fdEnd; i++) {
        if (fileDescriptors[i].fd == fileDescriptor->fd) {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex == -1) { // return if not found
        return;
    }

    // shift elements back to replace the removed one
    for (int i = foundIndex; i < fdEnd; i++) {
        fileDescriptors[i] = fileDescriptors[i + 1];
    }

    fileDescriptors[fdEnd] = emptyDescriptor;

    fdEnd--;
}




