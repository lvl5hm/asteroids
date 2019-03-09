
#define WORKER_FN(name) void *name(void *data)
typedef WORKER_FN(WorkerFn);

struct WorkQueueEntry
{
  void *data;
  WorkerFn *workerFn;
};

struct WorkQueue
{
  WorkQueueEntry entries[128];
  u32 volatile writeCursor;
  u32 volatile readCursor;
  
  u32 volatile addedCount;
  u32 volatile completedCount;
  
  HANDLE semaphore;
};

void AddEntry(WorkQueue *queue, WorkerFn *workerFn, void *data)
{
  u32 originalWriteCursor = queue->writeCursor;
  u32 newWriteCursor = (originalWriteCursor + 1) % ArrayCount(queue->entries);
  WorkQueueEntry *entry = queue->entries + queue->writeCursor;
  entry->workerFn = workerFn;
  entry->data = data;
  
  queue->addedCount++;
  queue->writeCursor = newWriteCursor;
}

b32 DoTopEntry(WorkQueue *queue)
{
  u32 originalReadCursor = queue->readCursor;
  if (originalReadCursor == queue->writeCursor)
  {
    return false;
  }
  
  u32 newReadCursor = (originalReadCursor + 1) % ArrayCount(queue->entries);
  u32 entryIndex = InterlockedCompareExchange(&queue->readCursor,
                                              newReadCursor,
                                              originalReadCursor);
  if (entryIndex == originalReadCursor)
  {
    WorkQueueEntry *entry = queue->entries + entryIndex;
    entry->workerFn(entry->data);
    InterlockedIncrement(&queue->completedCount);
  }
  
  return true;
}

void CompleteAllEntries(WorkQueue *queue)
{
  while (queue->completedCount != queue->addedCount)
  {
    DoTopEntry(queue);
  }
  
  queue->addedCount = 0;
  queue->completedCount = 0;
}

struct ThreadInfo
{
  WorkQueue *queue;
};

DWORD WINAPI ThreadProc(void *lpParameter)
{
  ThreadInfo *info = (ThreadInfo *)lpParameter;
  
  while (true)
  {
    b32 didTopEntry = DoTopEntry(info->queue);
    if (!didTopEntry)
    {
      WaitForSingleObjectEx(info->queue->semaphore, INFINITE, false);
    }
  }
  
  return 0;
}
