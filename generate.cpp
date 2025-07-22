//NOTE: These functions are used Multi threaded

struct FillChunkData {
    GameState *gameState;
    Chunk *chunk;
    PoolChunkGeneration *generationPool;

    int postFillInfoCount;
    ChunkPostFill postFillInfo[32];
    
};

struct GenerateMeshData {
    GameState *gameState;
    ChunkVertexToCreate *info;
};

struct GenerateMeshDataWithList {
    MultiThreadedMeshList *meshList;
    ChunkVertexToCreate *info;
};

enum WorldBuildingType {
    TALL_BUILDING,
};

struct BuildingInfo {
    WorldBuildingType type;
    float3 originLocalP;
};

int worldPToVoxelLocalP(int chunkX, float worldx) {
    int value = round((worldx - (CHUNK_SIZE_IN_METERS*chunkX)) * VOXELS_PER_METER); 
    assert(value >= 0 && value < CHUNK_DIM);
    return value;
}

void getAOMask_multiThreaded(void *data_);

void addBlock(GameState *gameState, float3 worldP, BlockType type) {
    int chunkX = roundChunkCoord(worldP.x / (float)CHUNK_DIM);
    int chunkY = roundChunkCoord(worldP.y / (float)CHUNK_DIM);
    int chunkZ = roundChunkCoord(worldP.z / (float)CHUNK_DIM);

    Chunk *c = getChunkNoGenerate(gameState, chunkX, chunkY, chunkZ);
   
    if(c) {
        if(!c->blocks) {
            c->blocks = (Block *)easyPlatform_allocateMemory(BLOCKS_PER_CHUNK*sizeof(Block), EASY_PLATFORM_MEMORY_ZERO);
        }
        int localX = worldP.x - (CHUNK_DIM*chunkX); 
        int localY = worldP.y - (CHUNK_DIM*chunkY); 
        int localZ = worldP.z - (CHUNK_DIM*chunkZ); 

        int blockIndex = getBlockIndex(localX, localY, localZ);
        assert(blockIndex < BLOCKS_PER_CHUNK);
        if(blockIndex < BLOCKS_PER_CHUNK) {
            c->blocks[blockIndex] = spawnBlock(localX, localY, localZ, type);
        } else {
            assert(false);
        }
    } 
}

#include "./perlin_noise_values.cpp"

void generateTree_multiThread(GameState *gameState, Chunk *chunk, float3 worldP) {
    int treeHeight = (int)(3*((float)rand() / (float)RAND_MAX)) + 3;

    for(int i = 0; i < treeHeight; ++i) {
        float3 p = plus_float3(worldP, make_float3(0, i, 0));
        //NOTE: Add block
        addBlock(gameState, p, BLOCK_TREE_WOOD);
    }

    int z = 0;
    int x = 0;

    float3 p = plus_float3(worldP, make_float3(x, (treeHeight + 1), z));
    addBlock(gameState, p, BLOCK_TREE_LEAVES);

    float3 offsets[] = {make_float3(1, treeHeight, 0), make_float3(1, treeHeight, 1), 
                        make_float3(-1, treeHeight, -1), make_float3(1, treeHeight, -1), 
                        make_float3(-1, treeHeight, 1), make_float3(0, treeHeight, 1),
                        make_float3(-1, treeHeight, 0), make_float3(0, treeHeight, -1), 
                        };

    for(int j = 0; j < 2; ++j) {
        for(int i = 0; i < arrayCount(offsets); ++i) {
            float3 o = offsets[i];
            o.y -= j;
            float3 p = plus_float3(worldP, o);
            addBlock(gameState, p, BLOCK_TREE_LEAVES);
        }
    }
    treeHeight -=1;

    float3 offsets2[] = {make_float3(2, treeHeight, 0), make_float3(2, treeHeight, 1), make_float3(2, treeHeight, 2), 
                        make_float3(1, treeHeight, 2), make_float3(0, treeHeight, 2), 

                        make_float3(-1, treeHeight, 2), make_float3(-2, treeHeight, 2), make_float3(-2, treeHeight, 1), 
                        make_float3(-2, treeHeight, 0), make_float3(-2, treeHeight, -1), 

                        make_float3(-2, treeHeight, -2), make_float3(-1, treeHeight, -2), 
                        make_float3(0, treeHeight, -2), make_float3(1, treeHeight, -2),  make_float3(2, treeHeight, -2), make_float3(2, treeHeight, -1), 
                        };

    for(int i = 0; i < arrayCount(offsets2); ++i) {
        float3 p = plus_float3(worldP, offsets2[i]);
        addBlock(gameState, p, BLOCK_TREE_LEAVES);
    }

}

BlockType worldGeneration_shouldBlockExist(float worldX, float worldY, float worldZ) {
    float terrainHeight = getTerrainHeight(worldX, worldZ);

    bool underWater = worldY < WATER_ELEVATION;
    BlockType type = BLOCK_NONE;

    if(worldY < terrainHeight) {
        type = BLOCK_GRASS;
        bool isTop = false;

        if(underWater) {
            type = BLOCK_SOIL;
        } else if(worldY < (terrainHeight - 1)) {
            type = BLOCK_SOIL;
        } else {
            isTop = true;
        }

        //TODO: Handle Tree building based on perlin noise
        // if(worldY > waterElevation && isTop && isTreeLocation(worldX, worldZ)) {
            // generateTree_multiThread(gameState, chunk, make_float3(worldX, worldY + 1, worldZ));
        // } 
        
    } else if(worldY < WATER_ELEVATION) {
        type = BLOCK_WATER;
    }

    return type;
}

void checkInitBlocksForChunk(Chunk *chunk) {
    if(!chunk->blocks) {
        chunk->blocks = (Block *)easyPlatform_allocateMemory(BLOCKS_PER_CHUNK*sizeof(Block), EASY_PLATFORM_MEMORY_ZERO);
    }
}

void addBuilding(FillChunkData *data, GameState *gameState, VoxelModel *model, float3 originLocalP, Chunk *chunk) {

    for(int i = 0; i < model->totalVoxelCount; ++i) {
        u32 voxelData = model->voxelData[i];
        int x1 = (int)(voxelData & 0xFF);
        int y1 = (int)((voxelData >> 8) & 0xFF);
        int z1 = (int)((voxelData >> 16) & 0xFF);
        int colorPalleteId = (int)((voxelData >> 24) & 0xFF);

        float3 localVoxelP = originLocalP;

        localVoxelP.x += x1;
        localVoxelP.y += y1;
        localVoxelP.z += z1;

        if(localVoxelP.x < CHUNK_DIM && localVoxelP.y < CHUNK_DIM && localVoxelP.z < CHUNK_DIM) {

            int blockIndex = getBlockIndex(localVoxelP.x, localVoxelP.y, localVoxelP.z);
            assert(blockIndex < BLOCKS_PER_CHUNK);
            if(blockIndex < BLOCKS_PER_CHUNK) {
                checkInitBlocksForChunk(chunk);
                chunk->blocks[blockIndex] = spawnBlock(localVoxelP.x, localVoxelP.y, localVoxelP.z, BLOCK_BUILDING, colorPalleteId, 1);
            }
        } 
    }
    
    assert(data->postFillInfoCount < arrayCount(data->postFillInfo));
    if(data->postFillInfoCount < arrayCount(data->postFillInfo)) {
        // data->postFillInfo[data->postFillInfoCount++] = initChunkPostFill(model, 0, float3 localVoxelStart, int chunkX, int chunkY, int chunkZ);
    }
}

void fillChunk_multiThread(void *data_) {
    FillChunkData *data = (FillChunkData *)data_;

    GameState *gameState = data->gameState;
    Chunk *chunk = data->chunk;
    PoolChunkGeneration *generationPool = data->generationPool;

    int buildingCount = 0;
    BuildingInfo buildings[MAX_BUILDING_COUNT_PER_CHUNK];
    

    for(int z = 0; z < CHUNK_DIM; ++z) {
        for(int x = 0; x < CHUNK_DIM; ++x) {
            float worldX = (x + chunk->x*CHUNK_DIM)*VOXEL_SIZE_IN_METERS;
            float worldZ = (z + chunk->z*CHUNK_DIM)*VOXEL_SIZE_IN_METERS;

            float waterElevation = WATER_ELEVATION;

            float terrainHeight = getTerrainHeight(worldX, worldZ);

            for(int y = 0; y < CHUNK_DIM; ++y) {
                float worldY = (y + chunk->y*CHUNK_DIM)*VOXEL_SIZE_IN_METERS;

                bool underWater = false;//worldY < waterElevation;
                bool isTop = false;

                if(worldY < terrainHeight) {
                    checkInitBlocksForChunk(chunk);
                    
                    BlockType type = BLOCK_GRASS;
                    

                    if(underWater) {
                        //NOTE: Vary underwater terrain (grass can't grow underwater)
                        float value = SimplexNoise_fractal_3d(8, worldX, worldY, worldZ, 0.1f);
                        type = BLOCK_SOIL;
                        if(value < 0.5f) {
                            type = BLOCK_STONE;
                        }
                    } else if(worldY < (terrainHeight - 1)) {
                        if(worldY >= ((terrainHeight - 1) - SUB_SOIL_DEPTH)) {
                            type = BLOCK_SOIL;
                        } else {
                            if(isIronLocation(worldX, worldY, worldZ)) {
                                type = BLOCK_IRON;
                            } else if(isCoalLocation(worldX, worldY, worldZ)) {
                                type = BLOCK_COAL;
                            } else {
                                type = BLOCK_STONE;
                            }
                            
                        }
                    } else {
                        isTop = true;
                    }

                    int blockIndex = getBlockIndex(x, y, z);
                    assert(blockIndex < BLOCKS_PER_CHUNK);
                    if(blockIndex < BLOCKS_PER_CHUNK) {
                        chunk->blocks[blockIndex] = spawnBlock(x, y, z, type);
                    }
                    
                } else if(worldY < waterElevation) {
                    // if(!chunk->blocks) {
                    //     chunk->blocks = (Block *)easyPlatform_allocateMemory(BLOCKS_PER_CHUNK*sizeof(Block), EASY_PLATFORM_MEMORY_ZERO);
                    // }
                    // //NOTE: Is water so add water
                    // int blockIndex = getBlockIndex(x, y, z);
                    // assert(blockIndex < BLOCKS_PER_CHUNK);
                    // if(blockIndex < BLOCKS_PER_CHUNK) {
                    //     chunk->blocks[blockIndex] = spawnBlock(x, y, z, BLOCK_WATER);
                    // }
                }

                 //NOTE: Now add any buildings that should be here
                if(isTallBuildingLocation(worldX, worldZ, &gameState->buildingModels[0]) && isTop) {
                    // assert(buildingCount < arrayCount(buildings));
                    if(buildingCount < arrayCount(buildings)) {
                        BuildingInfo *b = buildings + buildingCount++;
                        b->type = TALL_BUILDING;
                        b->originLocalP = make_float3(x, y, z);
                    }
                }
            }
        }
    }

    // //NOTE: Now add all buildings
    // for(int i = 0; i < buildingCount; ++i) {
    //     BuildingInfo b = buildings[i];
    //     if(b.type == TALL_BUILDING) {
    //         addBuilding(data, gameState, &gameState->buildingModel, b.originLocalP, chunk);
    //     }
    // }

    MemoryBarrier();
    ReadWriteBarrier();


    assert(chunk->generateState & CHUNK_GENERATING);
    //NOTE: Not generated yet, we can now notify this chunk is finished terrain generation, and can
    //      move to post fill with building data.
    // addAtomicInt(&generationPool->value, 1);

    chunk->generateState = CHUNK_GENERATED | CHUNK_MESH_DIRTY;

    free(data_);
    data_ = 0;
}


void fillChunk(GameState *gameState, Chunk *chunk) {
    assert(false);
    if(!gameState->chunkPostFillInfo.currentPool ||
        gameState->chunkPostFillInfo.currentPool->chunksInPoolCount < arrayCount(gameState->chunkPostFillInfo.currentPool->chunksInPool)) {
        
        assert(chunk->generateState & CHUNK_NOT_GENERATED);
        chunk->generateState = CHUNK_GENERATING;

        MemoryBarrier();
        ReadWriteBarrier();

        FillChunkData *data = (FillChunkData *)malloc(sizeof(FillChunkData));

        data->gameState = gameState;
        data->chunk = chunk;

        PoolChunkGeneration *generationPool = gameState->chunkPostFillInfo.currentPool;

        if(!generationPool) {
            if(gameState->chunkPostFillInfo.generationPoolFreeList) {
                generationPool = gameState->chunkPostFillInfo.generationPoolFreeList;
                gameState->chunkPostFillInfo.generationPoolFreeList = generationPool->next;
            } else {
                generationPool = pushStruct(&globalPerFrameArena, PoolChunkGeneration);
            }
            easyMemory_zeroSize(generationPool, sizeof(PoolChunkGeneration));
            setAtomicInt(&generationPool->value, 0);
            gameState->chunkPostFillInfo.currentPool = generationPool;
            generationPool->next = gameState->chunkPostFillInfo.generationPools;
            gameState->chunkPostFillInfo.generationPools = generationPool;
        }
        assert(gameState->chunkPostFillInfo.currentPool == generationPool);
        assert(generationPool);
        generationPool->targetValue++;
        data->generationPool = generationPool;

        assert(gameState->chunkPostFillInfo.currentPool->chunksInPoolCount < arrayCount(gameState->chunkPostFillInfo.currentPool->chunksInPool));
        gameState->chunkPostFillInfo.currentPool->chunksInPool[gameState->chunkPostFillInfo.currentPool->chunksInPoolCount++] = data;

        //NOTE: Multi-threaded version
        pushWorkOntoQueue(&gameState->threadsInfo, fillChunk_multiThread, data);
    }
}

void fillChunkWithPostBuildingData_multiThread(void *data_) {
    FillChunkData *data = (FillChunkData *)data_;

    GameState *gameState = data->gameState;
    Chunk *chunk = data->chunk;
    PoolChunkGeneration *generationPool = data->generationPool;



    MemoryBarrier();
    ReadWriteBarrier();

    //NOTE: Actually considered finished generating so can now notify other systems to this is the case
    chunk->generateState = CHUNK_GENERATED | CHUNK_MESH_DIRTY;

    free(data_);
    data_ = 0;
}

void fillChunkWithPostBuildingData(GameState *gameState, Chunk *chunk) {
    assert(chunk->generateState & CHUNK_GENERATING);

    MemoryBarrier();
    ReadWriteBarrier();

    FillChunkData *data = (FillChunkData *)malloc(sizeof(FillChunkData));

    data->gameState = gameState;
    data->chunk = chunk;

    PoolChunkGeneration *generationPool = 0;
    data->generationPool = generationPool;
    data->generationPool = generationPool;
    

    //NOTE: Multi-threaded version
    pushWorkOntoQueue(&gameState->threadsInfo, fillChunk_multiThread, data);
}

