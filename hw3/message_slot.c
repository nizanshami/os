// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/errno.h>
#include <linux/slab.h>


MODULE_LICENSE("GPL");

//Our custom definitions of IOCTL operations
#include "message_slot.h"

struct message_slots
{
  int minor;
  struct channel *channels;
  struct message_slots *next;
};

static struct message_slots *root;
//-----------------------message_slots function-----------------------

/* return the message_slot we the maching minor or crate new one*/
struct message_slots *search_slot(struct message_slots **root, int minor){
  struct message_slots *new_slot;
  if(!(*root)){
    new_slot = (struct message_slots*) kmalloc(sizeof(struct message_slots), GFP_KERNEL);
    if(!new_slot){
      return NULL;
    }
    new_slot->minor = minor;
    new_slot->channels = NULL;
    new_slot->next = NULL;
    *root = new_slot;
    return *root;
  }
  while(*root != NULL){
     
    if((*root)->minor == minor){
      return *root;
    }
    *root = (*root)->next;
  }
  new_slot = (struct message_slots*) kmalloc(sizeof(struct message_slots), GFP_KERNEL);
  if(!new_slot){
    return NULL;
  }
  new_slot->minor = minor;
  new_slot->channels = NULL;
  new_slot->next = NULL;
  *root = new_slot;
  return *root;   
}

//---------------------------------------------------------------
struct channel{
    unsigned int id;
    char *message;
    int length;
    struct channel *next;
};
//------------------channel functions----------------------------
struct channel *search_channel(struct channel **channels, int channel_id){
  struct channel *new_channel;
  if(!(*channels)){
    new_channel = (struct channel *)kmalloc(sizeof(struct channel), GFP_KERNEL);
    if(!new_channel){
      return NULL;
    }
    new_channel->id = channel_id;
    new_channel->message = NULL;
    new_channel->length = 0;
    new_channel->next = NULL;
    *channels = new_channel;
    return *channels;   

  }
  while(*channels != NULL){
    if((*channels)->id == channel_id){
      return *channels;
    }
    *channels = (*channels)->next;
  }
  new_channel = (struct channel *)kmalloc(sizeof(struct channel), GFP_KERNEL);
  if(!new_channel){
    return NULL;
  }
  new_channel->id = channel_id;
  new_channel->message = NULL;
  new_channel->length = 0;
  new_channel->next = NULL;
  *channels = new_channel;
  return *channels;   
   
}

//-----------------------cleanup---------------------------------
void free_channels(struct channel **channels){
  struct channel *tmp;
  while(*channels != NULL){
    tmp = *channels;
    *channels = (*channels)->next;
    if(tmp->message){
      kfree(tmp->message);
    }
    kfree(tmp);
  }
}

void delete_data(struct message_slots **ms){
  struct message_slots *tmp;
  while(*ms != NULL){
    tmp = *ms;
    *ms = (*ms)->next;
    free_channels(&tmp->channels);
    kfree(tmp);
  }
}

//---------------------------------------------------------------
//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
  int minor;
  struct message_slots *ms;
  
  
  minor = iminor(inode);
  ms = search_slot(&root, minor);
  if(!ms){
    return -EIO;
  }

  return SUCCESS;
}

//---------------------------------------------------------------
static int device_release( struct inode* inode,
                           struct file*  file)
{
  return SUCCESS;
}
//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
  int minor, i, channel_id;
  struct message_slots *ms;
  struct channel *channel;

  if(!buffer || !file || !file->private_data){
    return -EINVAL;
  }
  channel_id = (int) (uintptr_t) file->private_data;
  if(channel_id <= 0){
    return -EINVAL;
  }

  minor = iminor(file_inode(file));
  if(minor < 0){
    return -EIO;
  }
  ms = search_slot(&root, minor);
  if(!ms){
    return -EIO;
  }
  channel = search_channel(&ms->channels, channel_id);
  if(!channel){
    return -EIO;
  }
  if(channel->length == 0 || channel->message == NULL){
    return -EWOULDBLOCK;
  }
  if (channel->length > length)
  {
    return -ENOSPC;
  }
  for (i = 0; i < channel->length; i++)
  {
    put_user(channel->message[i], &buffer[i]);
  }
  
  return i;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
  int i, minor, channel_id;
  struct message_slots *ms;
  struct channel *channel;
  
  if(!file || !buffer || !file->private_data){
    return -ENAVAIL;
  }
  channel_id = (int) (uintptr_t) file->private_data;
  if(channel_id <= 0){
    return -ENAVAIL;
  }
  if(length > BUF_LEN || length == 0){
    return -EMSGSIZE;
  }

  minor = iminor(file_inode(file));
  if(minor < 0){
    return -EIO;
  }
  ms = search_slot(&root, minor);
  if(!ms){
    return -EIO;
  }
  channel = search_channel(&ms->channels, channel_id);
  if(!channel){
    return -EIO;
  }
  channel->message = kmalloc(sizeof(char)*BUF_LEN, GFP_KERNEL);
  for(i = 0;i < length && i < BUF_LEN;i++){
    get_user(channel->message[i], &buffer[i]);
  }
  channel->length = i;
  
  // return the number of input characters used
  return i;
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
  if(ioctl_command_id == MSG_SLOT_CHANNEL && ioctl_param > 0 && file){
    file->private_data = (void *) ioctl_param;
    return SUCCESS;
  }
  return -EINVAL;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops =
{
  .owner	  = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
  .release        = device_release,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
  int rc = -1;
  
  // Register driver capabilities. Obtain major num
  rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );

  // Negative values signify an error
  if( rc < 0 )
  {
    printk( KERN_ALERT "%s registraion failed for  %d\n",
                       DEVICE_FILE_NAME, MAJOR_NUM );
    return rc;
  }

  printk( "Registeration is successful. ");
  printk( "If you want to talk to the device driver,\n" );
  printk( "you have to create a device file:\n" );
  printk( "mknod /dev/%s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM );
  printk( "You can echo/cat to/from the device file.\n" );
  printk( "Dont forget to rm the device file and "
          "rmmod when you're done\n" );

  return 0;
}

//---------------------------------------------------------------
static void __exit cleanup(void)
{
  // Unregister the device
  // Should always succeed
  delete_data(&root);
  unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(cleanup);

//========================= END OF FILE =========================
