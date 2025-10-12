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

#define NBUFBUCKET 13

struct bufbucket {
  struct spinlock lock;
  struct buf* head;
  struct buf* freehead;
};

struct bufcache {
  struct buf buffer[NBUF];
  struct bufbucket bbucket[NBUFBUCKET];
} bcache;
static inline void setupnewbuf(struct buf* b, uint dev, uint blockno) {
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
}

static inline uint buckethash(uint dev, uint blockno) {
  return (uint) ((1L * blockno * 97 + dev) % NBUFBUCKET);
}
// assuming bucket's lock is held
static void bucketaddfront(struct bufbucket* bbucket, struct buf* b, struct buf** listhead) {
  b->prev = 0;
  b->next = *listhead;
  if (*listhead) {
    (*listhead)->prev = b;
  }
  *listhead = b;
}
static void bucketremove(struct bufbucket* bbucket, struct buf* b, struct buf** listhead) {
  if (b->prev) {
    b->prev->next = b->next;
  }
  else {
    (*listhead) = b->next;
  }
  if (b->next) {
    b->next->prev = b->prev;
  }
}

void
binit(void)
{
  struct buf* b;

  for (int i = 0; i < NBUFBUCKET; i++) {
    initlock(&bcache.bbucket[i].lock, "bcache_bucket");
  }
  // Create linked list of buffers
  for (b = bcache.buffer; b < bcache.buffer + NBUF; b++) {
    b->next = b + 1;
    b->prev = b - 1;
    initsleeplock(&b->lock, "buffer");
  }
  bcache.buffer[0].prev = 0;
  bcache.buffer[NBUF - 1].next = 0;
  // Initially, all buffers are in the first bucket
  bcache.bbucket[0].freehead = bcache.buffer;
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

  acquire(&bcache.bbucket[bbi].lock);
  for (b = bcache.bbucket[bbi].head; b != 0; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bbucket[bbi].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  /**
   * if release the lock here, another block might be added to the bucket with the same (dev, blockno)
   * causing duplicate buffers in the cache. further causing freeing a free buffer issue.
   */
  // release(&bcache.bbucket[bbi].lock);

  // Not cached.
  // Recycle an unused buffer, search in all buckets with ascending order.
  // Search its own free list first without holding additional lock.
  if ((b = bcache.bbucket[bbi].freehead) != 0) {
    bucketremove(&bcache.bbucket[bbi], b, &bcache.bbucket[bbi].freehead);
    bucketaddfront(&bcache.bbucket[bbi], b, &bcache.bbucket[bbi].head);
    setupnewbuf(b, dev, blockno);
    release(&bcache.bbucket[bbi].lock);
    acquiresleep(&b->lock);
    return b;
  }
  register int i = 1;
  for (uint idx = (bbi + 1) % NBUFBUCKET; i < NBUFBUCKET; idx = (idx + 1) % NBUFBUCKET, ++i) {
    acquire(&bcache.bbucket[idx].lock);
    b = bcache.bbucket[idx].freehead;
    if (b == 0) {
      release(&bcache.bbucket[idx].lock);
      continue;
    }
    // Remove from freelist in the current searching bucket
    bucketremove(&bcache.bbucket[idx], b, &bcache.bbucket[idx].freehead);
    release(&bcache.bbucket[idx].lock);

    // Add to the front of the main list of the target bucket
    bucketaddfront(&bcache.bbucket[bbi], b, &bcache.bbucket[bbi].head);
    setupnewbuf(b, dev, blockno);
    release(&bcache.bbucket[bbi].lock);
    acquiresleep(&b->lock);
    return b;
  }
  panic("bget: no free buffers");
  return 0;
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
  acquire(&bcache.bbucket[bbi].lock);

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // Move to the head of the bucket's free list.
    bucketremove(&bcache.bbucket[bbi], b, &bcache.bbucket[bbi].head);
    bucketaddfront(&bcache.bbucket[bbi], b, &bcache.bbucket[bbi].freehead);
  }

  release(&bcache.bbucket[bbi].lock);
}

void
bpin(struct buf *b) {
  uint bbi = buckethash(b->dev, b->blockno);
  acquire(&bcache.bbucket[bbi].lock);
  b->refcnt++;
  release(&bcache.bbucket[bbi].lock);
}

void
bunpin(struct buf *b) {
  uint bbi = buckethash(b->dev, b->blockno);
  acquire(&bcache.bbucket[bbi].lock);
  b->refcnt--;
  release(&bcache.bbucket[bbi].lock);
}


