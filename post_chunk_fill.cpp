
struct ChunkPostFill {
    VoxelModel *model; //NOTE: If there is no model, there must be voxelData.
    u32 *voxelData; //NOTE: Only if there is no model. This is for terrain voxels that aren't coming from a model.
    float3 localVoxelStart;
    int x;
    int y;
    int z;

    ChunkPostFill *next;
};

ChunkPostFill initChunkPostFill(VoxelModel *model, u32 *voxelData, float3 localVoxelStart, int chunkX, int chunkY, int chunkZ) {
    ChunkPostFill result = {};

    result.model = model;
    result.voxelData = voxelData;
    result.localVoxelStart = localVoxelStart;
    result.x = chunkX;
    result.y = chunkY;
    result.z = chunkZ;

    return result;
}

struct BuildingInfoForChunk {
    int x;
    int y;
    int z;

    ChunkPostFill *list;
};
typedef struct FillChunkData FillChunkData;
struct PoolChunkGeneration {
    AtomicInt value; //NOTE: Updated by chunks multi-threaded
    int targetValue;

    int chunksInPoolCount;
    FillChunkData *chunksInPool[1];

    PoolChunkGeneration *next;
};

#define CHUNK_POST_FILL_HASH_SIZE 1024
struct ChunkPostFill_ThreadSafe {

    int holeCount;
    int holes[1024];
    ChunkPostFill *chunkPostFillInfoHash[CHUNK_POST_FILL_HASH_SIZE]; //NOTE: Hash map linked
    ChunkPostFill *chunkPostFillInfoFreeList; //NOTE: Linked list

    //NOTE: All the chunks generated on the same frame are put into the same pool. 
    //      The chunks are not considered finsihed generating until all the chunks it their pool
    //      have finished generating their terrain, aswell as written any post building fills into 
    //      their chunk. Then it is considered finished generating.  
    PoolChunkGeneration *generationPools; //NOTE: Linked list
    PoolChunkGeneration *generationPoolFreeList; //NOTE: Linked list
    PoolChunkGeneration *currentPool;  //NOTE: The pool that is currently active for the frame. If null, it should be allocated 
};

uint32_t getHashForChunkPostFill(int x, int y, int z) {
    int values[3] = {x, y, z};
    uint32_t hash = get_crc32((char *)values, arrayCount(values)*sizeof(int));
    hash = hash & (CHUNK_POST_FILL_HASH_SIZE - 1);
    assert(hash < CHUNK_POST_FILL_HASH_SIZE);
    assert(hash >= 0);
    return hash;
}

void addChunkPostToList(ChunkPostFill_ThreadSafe *parent, ChunkPostFill fillInfo) {
    
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
   
    
}
