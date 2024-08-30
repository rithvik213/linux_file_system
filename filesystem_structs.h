#ifndef _FILESYSTEM_STRUCTS_H
#define _FILESYSTEM_STRUCTS_H

// parameter for rd_creat, rd_mkdir, rd_unlink
struct pathParam {
  int pathLen;
  const char *path;
  int returnVal;
};

//parameter for read/write
struct rwParam {
  int inodeNum;
  char *address;
  int filePosition;
  int numBytes;
  int returnVal;
};

// parameter for rd_open
struct openParam {
  int pathLen;
  const char *path;
  int inodeNum;
  int returnVal;
};

//parameter for rd_close
struct closeParam {
  int inodeNum;
  int returnVal;
};

// parameter for lseek
struct lseekParam {
  int returnVal;
  int inodeNum;
  int offset;
  int offset_toReturn;
};

// parameter for readdir
struct readdirParam {
  int inodeNum;
  char address[16];
  int filePosition;
  int dirDataLen;
  int returnVal;
};


#endif