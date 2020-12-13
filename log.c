#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  // int n;
  int block[LOGSIZE];
  char bitmap[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;

  char commit_bitmap[LOGSIZE];
  struct buf *logbuf[LOGSIZE];
};
struct log log;

static void recover_from_log(void);
static void commit();

int ckpt_started = 0;
int ckpt_running = 0;
int ckpt_runcount = 0;
struct sleeplock ckptlock;

void
initlog(int dev)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  struct superblock sb;
  initlock(&log.lock, "log");
  readsb(dev, &sb);
  log.start = sb.logstart;
  log.size = sb.nlog;
  log.dev = dev;
  recover_from_log();

  initsleeplock(&ckptlock, "checkpoint lock");
}

// Copy committed blocks from log to their home location
static void
install_trans(int from_ckpt)
{
  int tail;

  while (ckpt_runcount > 0)
  {
    for (tail = 0; tail < LOGSIZE; tail++)
    {
      if (!log.commit_bitmap[tail])
        continue;
      cprintf("==================commiting STA log t=%d \n", tail);
      struct buf *lbuf;
      if (from_ckpt)
        lbuf = log.logbuf[tail];
      else
      {
        lbuf = bread(log.dev, log.start + tail + 1); // read log block
      }
      cprintf("commit@%d: ", tail);
      struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
      memmove(dbuf->data, lbuf->data, BSIZE);                // copy block to dst
      bwrite(dbuf);                                          // write dst to disk
      cprintf("commit@%d: ", tail);
      brelse(lbuf);
      cprintf("commit@%d: ", tail);
      brelse(dbuf);
      acquire(&log.lock);
      log.lh.bitmap[tail] = 0;
      log.commit_bitmap[tail] = 0;
      release(&log.lock);
      cprintf("====================commiting FIN log t=%d d=%s\n", tail, lbuf->data);
    }
    ckpt_runcount -= 1;
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  // log.lh.n = lh->n;
  for (i = 0; i < LOGSIZE; i++) {
    log.lh.bitmap[i] = lh->bitmap[i];
    if (lh->bitmap[i])
      log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  // hb->n = log.lh.n;
  for (i = 0; i < LOGSIZE; i++) {
    hb->bitmap[i] = log.lh.bitmap[i];
    if (log.lh.bitmap[i])
      hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  ckpt_runcount += 1;
  install_trans(0); // if committed, copy from log to disk
  // log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);
    }
    //  else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
    //   // this op might exhaust log space; wait for commit.
    //   sleep(&log, &log.lock);
    // } 
    else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if(log.committing)
    panic("log.committing");
  if(log.outstanding == 0){
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < LOGSIZE; tail++) {
    if (!log.lh.bitmap[tail])
      continue;
    cprintf("==================writing STA t=%d\n", tail);
    cprintf("write@%d: ", tail);
    struct buf *to = bread(log.dev, log.start+tail+1); // log block
    log.lh.bitmap[tail] = 1;
    log.logbuf[tail] = to;
    cprintf("write@%d: ", tail);
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // write the log
    cprintf("write@%d: ", tail);
    brelse(from);
    // brelse(to);
    cprintf("==================writing FIN log t=%d d=%s\n", tail, from->data);
  }
}

static void
commit()
{
  int go = 0;
  int i;
  for (i = 0; i < LOGSIZE; i++)
  {
    if (log.lh.bitmap[i])
    {
      go = 1;
      break;
    }
  }

  if (go) {
    cprintf("going commit\n");
    write_log();     // Write modified blocks from cache to log
    write_head();    // Write header to disk -- the real commit

    acquire(&log.lock);
    int i;
    for (i = 0; i < LOGSIZE; i++)
    {
      if (log.lh.bitmap[i])
        log.commit_bitmap[i] = 1;
    }
    release(&log.lock);

    ckpt_runcount += 1;
    if (!ckpt_started)
    {
      install_trans(1); // Now install writes to home locations
      // log.lh.n = 0;
      write_head(); // Erase the transaction from the log
    }
    else
    {
      releasesleep(&ckptlock);
    }
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache with B_DIRTY.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  // if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
  //   panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  acquire(&log.lock);
  for (i = 0; i < LOGSIZE; i++) {
    if (log.lh.bitmap[i] && log.lh.block[i] == b->blockno)   // log absorbtion
      break;
  }
  if (i == LOGSIZE)
  {
    for (i = 0; i < LOGSIZE; i++)
    {
      if (!log.lh.bitmap[i])
        break;
    }
  }
  log.lh.bitmap[i] = 1;
  log.lh.block[i] = b->blockno;
  // if (i == log.lh.n)
  //   log.lh.n++;
  b->flags |= B_DIRTY; // prevent eviction
  release(&log.lock);
}

void start_ckpt(void)
{
  cprintf("running start_ckpt in log.c\n");
  ckpt_started = 1;
  acquiresleep(&ckptlock);
  for (;;)
  {
    acquiresleep(&ckptlock);
    ckpt_running = 1;
    cprintf("go checkpoint\n");
    install_trans(1); // Now install writes to home locations
    // log.lh.n -= 0;
    write_head();    // Erase the transaction from the log
    cprintf("fin checkpoint\n");
    ckpt_running = 0;
  }
}