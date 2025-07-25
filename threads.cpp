
#define MemoryBarrier()  __sync_synchronize()
#define ReadWriteBarrier() { SDL_CompilerBarrier(); }

typedef void ThreadWorkFuncType(void *Data);

struct ThreadWork
{
    ThreadWorkFuncType *FunctionPtr;
    void *Data;
    bool Finished;
};

struct ThreadWorkQueue {
    ThreadWork WorkQueue[4096];
    SDL_atomic_t IndexToTakeFrom;
    SDL_atomic_t IndexToAddTo;
};

struct ThreadsInfo {
    SDL_sem *Semaphore;
    ThreadWorkQueue queue_;
    ThreadWorkQueue perFrameQueue_;
};

typedef struct {
    SDL_atomic_t mutex;
} MutexSpinLock;

void beginSpinMutex(MutexSpinLock *lock) {
    // Spin until we acquire the lock
    while (!SDL_AtomicCAS(&lock->mutex, 0, 1)) {
        // yield to prevent tight spinning
        SDL_Delay(1);
    }
}

void endSpinMutex(MutexSpinLock *lock) {
    // Just release the lock
    assert(SDL_AtomicGet(&lock->mutex) == 1);
    SDL_AtomicSet(&lock->mutex, 0);
}

typedef struct {
    SDL_mutex *mutex;
} MutexLock;

MutexLock createMutex() {
    MutexLock result = {};
    result.mutex = SDL_CreateMutex();
    return result;
}

void beginMutex(MutexLock *lock) {
   SDL_LockMutex(lock->mutex);
}

void endMutex(MutexLock *lock) {
   SDL_UnlockMutex(lock->mutex);
}

struct AtomicInt {
    SDL_atomic_t value;
};

int setAtomicInt(AtomicInt *atomic, int newValue) {
    return SDL_AtomicSet(&atomic->value, newValue);
}

int getAtomicInt(AtomicInt *atomic) {
    return SDL_AtomicGet(&atomic->value);
}

int addAtomicInt(AtomicInt *atomic, int addend) {
    return SDL_AtomicAdd(&atomic->value, addend);
}


//TODO(ollie): Make safe for threads other than the main thread to add stuff
void pushWorkOntoQueue(ThreadsInfo *Info, ThreadWorkFuncType *WorkFunction, void *Data, ThreadWorkQueue *queue = 0) { //NOT THREAD SAFE. OTHER THREADS CAN'T ADD TO QUEUE

    if(!queue) {
        queue = &Info->queue_;
    }

    for(;;)
    {
        int OnePastTheHead = (queue->IndexToAddTo.value + 1) % arrayCount(queue->WorkQueue);
        if(queue->WorkQueue[queue->IndexToAddTo.value].Finished && OnePastTheHead != queue->IndexToTakeFrom.value)
        {
            ThreadWork *Work = queue->WorkQueue + queue->IndexToAddTo.value; 
            Work->FunctionPtr = WorkFunction;
            Work->Data = Data;
            Work->Finished = false;
            
            MemoryBarrier();
            ReadWriteBarrier();
            
            ++queue->IndexToAddTo.value %= arrayCount(queue->WorkQueue);
            
            MemoryBarrier();
            ReadWriteBarrier();
            
            SDL_SemPost(Info->Semaphore);
            break;
        }
        else
        {   
            // NOTE(ollie): Queue is full
            assert(false);
        }
    }
}

ThreadWork *GetWorkOffQueueSingle(ThreadsInfo *Info, ThreadWorkQueue *queue, ThreadWork **WorkRetrieved)
{
    *WorkRetrieved = 0;

    int OldValue = queue->IndexToTakeFrom.value;
    if(OldValue != queue->IndexToAddTo.value)
    {
        uint32_t NewValue = (OldValue + 1) % arrayCount(queue->WorkQueue);
        
        if(SDL_AtomicCAS(&queue->IndexToTakeFrom, OldValue, NewValue) == SDL_TRUE)
        {
            *WorkRetrieved = queue->WorkQueue + OldValue;
            assert(*WorkRetrieved);
        }
    }    
    return *WorkRetrieved;
}

ThreadWork *GetWorkOffQueue(ThreadsInfo *Info, ThreadWork **WorkRetrieved)
{
    *WorkRetrieved = 0;

    if(GetWorkOffQueueSingle(Info, &Info->perFrameQueue_, WorkRetrieved)) {

    } else {
        GetWorkOffQueueSingle(Info, &Info->queue_, WorkRetrieved);
    }
   
    return *WorkRetrieved;
}


void doThreadWork(ThreadsInfo *Info)
{
    ThreadWork *Work;
    while(GetWorkOffQueue(Info, &Work))
    {
        Work->FunctionPtr(Work->Data);
        assert(!Work->Finished);
        
        MemoryBarrier();
        ReadWriteBarrier();
        
        Work->Finished = true;
    }
}

int platformThreadEntryPoint(void *Info_) {
    ThreadsInfo *Info = (ThreadsInfo *)Info_;

    for(;;) {
        doThreadWork(Info);
        SDL_SemWait(Info->Semaphore);
    }
    
}

bool isWorkFinished(ThreadsInfo *Info, ThreadWorkQueue *queue)
{
    bool Result = true;
    for(uint32_t WorkIndex = 0;
        WorkIndex < arrayCount(queue->WorkQueue) && Result;
        ++WorkIndex)
    {
        Result &= queue->WorkQueue[WorkIndex].Finished;
        if(!Result) { break; }
    }

    return Result;
}

void waitForAllWorkToFinish(ThreadsInfo *Info)
{
    while(!isWorkFinished(Info, &Info->queue_) && !isWorkFinished(Info, &Info->perFrameQueue_))
    {
        doThreadWork(Info);        
    }
}

void waitForPerFrameWorkToFinish(ThreadsInfo *Info)
{
    while(!isWorkFinished(Info, &Info->perFrameQueue_))
    {
        doThreadWork(Info);        
    }
}

void initThreadQueue(ThreadsInfo *threadsInfo) {
    //NOTE: Clear data to zero
    memset(threadsInfo, 0, sizeof(ThreadsInfo));

    int numberOfProcessors = SDL_GetCPUCount();
            
    int numberOfUnusedProcessors = (numberOfProcessors - 1); //NOTE(oliver): minus one to account for the one we are on
    
    threadsInfo->Semaphore = SDL_CreateSemaphore(0);
    
    for(int WorkIndex = 0;
        WorkIndex < arrayCount(threadsInfo->queue_.WorkQueue);
        ++WorkIndex)
    {
        threadsInfo->queue_.WorkQueue[WorkIndex].Finished = true;
    }

    for(int WorkIndex = 0;
        WorkIndex < arrayCount(threadsInfo->perFrameQueue_.WorkQueue);
        ++WorkIndex)
    {
        threadsInfo->perFrameQueue_.WorkQueue[WorkIndex].Finished = true;
    }
    
    SDL_Thread *Threads[12];
    int threadCount = 0;

    int coreCount = MathMin(numberOfUnusedProcessors, arrayCount(Threads));

    for(int CoreIndex = 0;
        CoreIndex < coreCount;
        ++CoreIndex)
    {
        assert(threadCount < arrayCount(Threads));
        Threads[threadCount++] = SDL_CreateThread(platformThreadEntryPoint, "", threadsInfo);
    }
            
}