#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h> /* error codes */
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/tty.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");


#include "filesystem_structs.h"
#include "filesystem_functions_kernel.h"


#ifndef _RD_FUNCTIONS
#define _RD_FUNCTIONS

#define RD_CREAT _IOWR(0, 11, struct pathParam)
#define RD_MKDIR _IOWR(0, 18, struct pathParam)
#define RD_OPEN _IOWR(0, 13, struct openParam)
#define RD_CLOSE _IOWR(0, 14, struct closeParam)
#define RD_READ _IOWR(0, 15, struct rwParam)
#define RD_WRITE _IOWR(0, 16, struct rwParam)
#define RD_LSEEK _IOWR(0, 17, struct lseekParam)
#define RD_UNLINK _IOWR(0, 12, struct pathParam)
#define RD_READDIR _IOWR(0, 19, struct readdirParam)

#endif


static struct file_operations pseudo_dev_proc_operations;
static struct proc_dir_entry *proc_entry;
static int rd_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
void ramdiskInitOperations(void);

static int __init initialization_routine(void) {
  pseudo_dev_proc_operations.ioctl = rd_ioctl;

  /* Start create proc entry */
  proc_entry = create_proc_entry("ramdisk", 0444, NULL);
  if(!proc_entry)
  {
    printk("<1> Error creating /proc entry.\n");
    return 1;
  }

  proc_entry->proc_fops = &pseudo_dev_proc_operations;
  ramdiskInitOperations();

  return 0;
}

/* 'printk' version that prints to active tty. */
void my_printk(char *string)
{
  struct tty_struct *my_tty;

  my_tty = current->signal->tty;

  if (my_tty != NULL) {
    (*my_tty->driver->ops->write)(my_tty, string, strlen(string));
    (*my_tty->driver->ops->write)(my_tty, "\015\012", 2);
  }
} 

void uninitialize(void);

// cleanup from primer
static void __exit cleanup_routine(void) {

  uninitialize();
  remove_proc_entry("ramdisk", NULL);

  return;
}


// based on ioctl call from primer
static int rd_ioctl(struct inode *inode, struct file *file,
				unsigned int cmd, unsigned long arg)
{

  struct pathParam creatParams;
  struct pathParam mkdirParams;
  struct openParam openParams;
  struct closeParam closeParams;
  struct rwParam readParams;
  struct rwParam writeParams;
  struct lseekParam lseekParams;
  struct pathParam unlinkParams;
  struct readdirParam readdirParams;
  char *path = NULL;

  switch (cmd)
  {
  case RD_CREAT:
    copy_from_user(&creatParams, (struct pathParam *)arg, sizeof(struct pathParam));
    path = strdup(&creatParams.path);
    int retCreat = rd_creat_kernel(path);
    creatParams.returnVal = retCreat;
    copy_to_user((int *)arg, &creatParams.returnVal, sizeof(int));
    kfree(path);
    break;

  case RD_MKDIR:
    copy_from_user(&mkdirParams, (struct pathParam *)arg, sizeof(struct pathParam));
    path = strdup(&mkdirParams.path);
    int retMkdir = rd_mkdir_kernel(path);
    mkdirParams.returnVal = retMkdir;
    copy_to_user((int *)arg, &mkdirParams.returnVal, sizeof(int));
    kfree(path);
    break;

  case RD_OPEN:
    copy_from_user(&openParams, (struct openParam *)arg, sizeof(struct openParam));
    path = strdup(&openParams.path);
    int retOpen =  = rd_open_kernel(path, &openParams.inodeNum);
    openParams.returnVal = retOpen;
    copy_to_user((struct openParam *)arg, &openParams, sizeof(struct openParam));
    kfree(path);
    break;

  case RD_CLOSE:
    copy_from_user(&closeParams, (struct closeParam *)arg, sizeof(struct closeParam));
    int retClose = rd_close_kernel(closeParams.inodeNum);
    closeParams.returnVal = retClose;
    copy_to_user((int *)arg, &closeParams.returnVal, sizeof(int));
    break;

  case RD_READ:
    copy_from_user(&readParams, (struct rwParam *)arg, sizeof(struct rwParam));
    int retRead =  = rd_read_kernel(readParams.inodeNum, readParams.filePosition, readParams.address, readParams.numBytes);
    readParams.returnVal = retRead;
    copy_to_user((struct rwParam *)arg, &readParams, sizeof(struct rwParam));
    break;

  case RD_WRITE:
    copy_from_user(&writeParams, (struct rwParam *)arg, sizeof(struct rwParam));
    int retWrite = rd_write_kernel(writeParams.inodeNum, writeParams.filePosition, writeParams.address, writeParams.numBytes);
    writeParams.returnVal = retWrite;
    copy_to_user((struct rwParam *)arg, &writeParams, sizeof(struct rwParam));
    break;

  case RD_LSEEK:
    copy_from_user(&lseekParams, (struct lseekParam *)arg, sizeof(struct lseekParam));
    int offset_toReturn = 0;
    int retLseek = rd_lseek_kernel(lseekParams.inodeNum, lseekParams.offset, &offset_toReturn);
    lseekParams.returnVal = retLseek;
    if (0 == lseekParams.returnVal) {
      lseekParams.offset_toReturn = offset_toReturn;
    }
    copy_to_user((struct lseekParam *)arg, &lseekParams, sizeof(struct lseekParam));
    break;

  case RD_UNLINK:
    copy_from_user(&unlinkParams, (struct pathParam *)arg, sizeof(struct pathParam));
    path = strdup(&unlinkParams.path);
    int retUnlink = rd_unlink_kernel(path);
    unlinkParams.returnVal = retUnlink;
    copy_to_user((int *)arg, &unlinkParams.returnVal, sizeof(int));
    kfree(path);
    break;

  case RD_READDIR:
    copy_from_user(&readdirParams, (struct readdirParam *)arg, sizeof(struct readdirParam));
    int retReaddir = rd_readdir_kernel(readdirParams.inodeNum, readdirParams.address, &readdirParams.filePosition);
    readdirParams.returnVal = retReaddir;
    if (readdirParams.returnVal > 0) {
      readdirParams.dirDataLen = ((int)sizeof(struct directory_entry));
    }
    copy_to_user((struct readdirParam *)arg, &readdirParams, sizeof(struct readdirParam));
    break;

  default:
    return -EINVAL;
    break;
  }
  
  return 0;
}


module_init(initialization_routine); 
module_exit(cleanup_routine); 



