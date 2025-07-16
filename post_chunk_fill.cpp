
struct ChunkPostFill {
    VoxelModel *model; //NOTE: If there is no model, there must be voxelData.
    u32 *voxelData; //NOTE: Only if there is no model. This is for terrain voxels that aren't coming from a model.
    float3 localVoxelStart;
    int chunkX;
    int chunkY;
    int chunkZ;

    ChunkPostFill *next;
};

ChunkPostFill initChunkPostFill(VoxelModel *model, u32 *voxelData, float3 localVoxelStart, int chunkX, int chunkY, int chunkZ) {
    ChunkPostFill result = {};

    result.model = model;
    result.voxelData = voxelData;
    result.localVoxelStart = localVoxelStart;
    result.chunkX = chunkX;
    result.chunkY = chunkY;
    result.chunkZ = chunkZ;

    return result;
}

struct BuildingInfoForChunk {
    int x;
    int y;
    int z;

    ChunkPostFill *list;
};

#define CHUNK_POST_FILL_HASH_SIZE 1024
struct ChunkPostFill_ThreadSafe {
    MutexLock mutex;

    int holeCount;
    int holes[1024];
    ChunkPostFill *chunkPostFillInfoHash[CHUNK_POST_FILL_HASH_SIZE]; //NOTE: Hash map linked
    ChunkPostFill *chunkPostFillInfoFreeList; //NOTE: Linked list
};

uint32_t getHashForChunkPostFill(int x, int y, int z) {
    int values[3] = {x, y, z};
    uint32_t hash = get_crc32((char *)values, arrayCount(values)*sizeof(int));
    hash = hash & (CHUNK_POST_FILL_HASH_SIZE - 1);
    assert(hash < CHUNK_POST_FILL_HASH_SIZE);
    assert(hash >= 0);
    return hash;
}

void initChunkPostFill_ThreadSafe(ChunkPostFill_ThreadSafe *t) {
    t->mutex = createMutex();
    t->chunkPostFillInfo = 0;
    t->holeCount = 0;
    t->chunkPostFillInfoFreeList = 0;
}

void addChunkPostToList(ChunkPostFill_ThreadSafe *parent, ChunkPostFill fillInfo) {
    beginMutex(&parent->mutex);
    ChunkPostFill *child = 0;

    if(parent->chunkPostFillInfoFreeList) {
        child = parent->chunkPostFillInfoFreeList;
        parent->chunkPostFillInfoFreeList = child->next;
    } else {
        child = (ChunkPostFill *)easyPlatform_allocateMemory(sizeof(ChunkPostFill));
    }
    
    easyPlatform_copyMemory(child, &fillInfo, sizeof(ChunkPostFill));

    u32 hash = getHashForChunkPostFill(child->x, child->y, child->z);

    child->next = parent->chunkPostFillInfoHash[hash];
    parent->chunkPostFillInfoHash[hash] = child;

    int holeIndex = -1;
    for(int i = 0; i < parent->holeCount && holeIndex < 0; ++i) {
        if(parent->holes[i] == hash) {
            holeIndex = i;
        }
    }
    
    if(holeIndex < 0) {
        assert(parent->holeCount < arrayCount(parent->holes));
        if(parent->holeCount < arrayCount(parent->holes)){
            parent->holes[parent->holeCount++] = hash;
        }
    }
   
    endMutex(&parent->mutex);
}
