// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUFBUCKET 37

// FIFO
struct freebuflist {
  struct buf buffer[NBUF];
  struct spinlock lock;
  struct buf* head;
  struct buf* tail;
} freeblist;

struct bufbucket {
  struct spinlock lock;
  struct buf* head;
} bbucket[NBUFBUCKET];

static inline uint buckethash(uint dev, uint blockno) {
  return (uint) ((1L * blockno * 97 + dev) % NBUFBUCKET);
}

void
binit(void)
{
  struct buf* b;

  initlock(&freeblist.lock, "bcache_freelist");
  for (int i = 0; i < NBUFBUCKET; i++) {
    initlock(&bbucket[i].lock, "bcache_bucket");
  }
  // Create linked list of buffers
  for (b = freeblist.buffer; b < freeblist.buffer + NBUF; b++) {
    b->next = b + 1;
    b->prev = b - 1;
    initsleeplock(&b->lock, "buffer");
  }
  freeblist.buffer[0].prev = 0;
  freeblist.buffer[NBUF - 1].next = 0;
  freeblist.head = freeblist.buffer;
  freeblist.tail = &freeblist.buffer[NBUF - 1];
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf* b;

  // Is the block already cached?
  uint bbi = buckethash(dev, blockno);

  acquire(&bbucket[bbi].lock);
  for (b = bbucket[bbi].head; b != 0; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bbucket[bbi].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&freeblist.lock);
  if (freeblist.head == 0) {
    release(&freeblist.lock);
    panic("bget: no free buffers");
  }
  b = freeblist.head;
  freeblist.head = freeblist.head->next;
  if (freeblist.head) {
    freeblist.head->prev = 0;
  } else {
    freeblist.tail = 0;
  }
  release(&freeblist.lock);

  if(b->refcnt != 0)
    panic("bget: free buffer has non-zero refcnt");
  if (holdingsleep(&b->lock))
    panic("bget: free buffer is locked");


  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  // add to bucket
  b->prev = 0;
  b->next = bbucket[bbi].head;
  if (bbucket[bbi].head) {
    bbucket[bbi].head->prev = b;
  }
  bbucket[bbi].head = b;
  
  release(&bbucket[bbi].lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint bbi = buckethash(b->dev, b->blockno);
  acquire(&bbucket[bbi].lock);

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // remove from bucket
    if (b->prev) {
      b->prev->next = b->next;
    }
    else {
      bbucket[bbi].head = b->next;
    }
    if (b->next) {
      b->next->prev = b->prev;
    }
    // add to the end of free list
    acquire(&freeblist.lock);
    b->prev = freeblist.tail;
    b->next = 0;
    if (freeblist.tail) {
      freeblist.tail->next = b;
    }
    else {
      freeblist.head = b;
    }
    freeblist.tail = b;
    release(&freeblist.lock);
  }

  release(&bbucket[bbi].lock);
}

void
bpin(struct buf *b) {
  uint bbi = buckethash(b->dev, b->blockno);
  acquire(&bbucket[bbi].lock);
  b->refcnt++;
  release(&bbucket[bbi].lock);
}

void
bunpin(struct buf *b) {
  uint bbi = buckethash(b->dev, b->blockno);
  acquire(&bbucket[bbi].lock);
  b->refcnt--;
  release(&bbucket[bbi].lock);
}


