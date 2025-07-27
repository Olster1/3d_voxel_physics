
void generateChunkMesh_multiThread(void *data_) {
    GenerateMeshData *data = (GenerateMeshData *)data_;

    GameState *gameState = data->gameState;
    ChunkVertexToCreate *info = data->info;
    Chunk *c = (Chunk *)info->chunk;

    // assert(c->generateState & CHUNK_MESH_BUILDING);
    if(c->generateState & CHUNK_MESH_BUILDING) {
    
        int blockCount = (c->blocks) ? BLOCKS_PER_CHUNK : 0;
        for(int i = 0; i < blockCount; ++i) {
            Block *b = &c->blocks[i];
            
            if(b->exists) {
                float3 worldP = make_float3(c->x*CHUNK_SIZE_IN_METERS + b->x*VOXEL_SIZE_IN_METERS, c->y*CHUNK_SIZE_IN_METERS + b->y*VOXEL_SIZE_IN_METERS, c->z*CHUNK_SIZE_IN_METERS + b->z*VOXEL_SIZE_IN_METERS);
                BlockType t = (BlockType)b->type;
                b->aoMask = 0;

                 if(t == BLOCK_WATER) {
                    if(!blockExistsReadOnly(gameState, worldP.x, worldP.y + VOXEL_SIZE_IN_METERS, worldP.z, (BlockFlags)0xFFFFFFFF)) 
                    {
                        //NOTE: Draw the water
                        int vertexCount = getArrayLength(info->alphaTriangleData);
                        for(int i = 0; i < 4; ++i) {
                            int indexIntoCubeData = i;
                            const VertexForChunk v = global_cubeDataForChunk[indexIntoCubeData];

                            float3 finalP = plus_float3(worldP, scale_float3(VOXEL_SIZE_IN_METERS, v.pos));
                            int palleteId = 0;
                            int colorId = 0;
                            VoxelVertex vForChunk = initVoxelVertex(finalP, v.normal, colorId, palleteId);

                            pushArrayItem(&info->alphaTriangleData, vForChunk, VoxelVertex);
                        }
                        
                        pushQuadIndicies(&info->alphaIndicesData, vertexCount);
                    }
                } else {
                    //NOTE: Run Greedy mesh algorithm
                    for(int k = 0; k < arrayCount(gameState->cardinalOffsets); k++) {
                        float3 p = plus_float3(worldP, gameState->cardinalOffsets[k]);
                        if(!blockExistsReadOnly(gameState, p.x, p.y, p.z, BLOCK_FLAGS_AO)) {
                            //NOTE: Face is exposed so add it to the mesh
                            int vertexCount = getArrayLength(info->triangleData);
                            //NOTE: 4 vertices for a cube face
                            for(int i = 0; i < 4; ++i) {
                                int indexIntoCubeData = i + k*4;
                                const VertexForChunk v = global_cubeDataForChunk[indexIntoCubeData];

                                float3 finalP = plus_float3(worldP, scale_float3(VOXEL_SIZE_IN_METERS, v.pos));
                                VoxelVertex vForChunk = initVoxelVertex(finalP, v.normal, (int)b->colorId, (int)b->palleteId);

                                pushArrayItem(&info->triangleData, vForChunk, VoxelVertex);
                            }
                            pushQuadIndicies(&info->indicesData, vertexCount);
                        }
                    }
                }
            }
        }
    }
    
    MemoryBarrier();
    ReadWriteBarrier();

    info->ready = true;

    free(data_);
    data_ = 0;
    
}

void processMeshData(ChunkVertexToCreate *info) {
    Chunk *c = (Chunk *)info->chunk;
    if((c->generateState & CHUNK_MESH_BUILDING) && c->meshGenerationAt <= info->generation) {

        {
            int indexCount = getArrayLength(info->indicesData);
            int vertexCount = getArrayLength(info->triangleData);
            if(indexCount > 0 && vertexCount > 0) {
                if(c->modelBuffer.handle) {
                    //TODO: Change to just do sub-buffer data and not delete the vao
                    deleteVao(c->modelBuffer.handle);
                    c->modelBuffer.indexCount = 0;
                }
                c->modelBuffer = generateVertexBuffer(info->triangleData, vertexCount, info->indicesData, indexCount, ATTRIB_INSTANCE_TYPE_VOXEL_CHUNK);
            } 
        }
        {
            int indexCount = getArrayLength(info->alphaIndicesData);
            int vertexCount = getArrayLength(info->alphaTriangleData);
            if(indexCount > 0 && vertexCount > 0) {
                if(c->alphaModelBuffer.handle) {
                    //TODO: Change to just do sub-buffer data and not delete the vao
                    deleteVao(c->alphaModelBuffer.handle);
                    c->alphaModelBuffer.indexCount = 0;
                }
                c->alphaModelBuffer = generateVertexBuffer(info->alphaTriangleData, vertexCount, info->alphaIndicesData, indexCount, ATTRIB_INSTANCE_TYPE_VOXEL_CHUNK);
            } 
        }

        assert(c->generateState & CHUNK_MESH_BUILDING);
        c->generateState &= ~CHUNK_MESH_BUILDING;
        assert(!(c->generateState & CHUNK_MESH_BUILDING));
        
        c->meshGenerationAt = info->generation;
    }

    freeResizeArray(info->triangleData);
    freeResizeArray(info->indicesData);
    freeResizeArray(info->alphaTriangleData);
    freeResizeArray(info->alphaIndicesData);
} 

void pushCreateMeshToThreads(GameState *gameState, Chunk *chunk) {
    assert(chunk->generateState & CHUNK_MESH_DIRTY);
    chunk->generateState &= ~CHUNK_MESH_DIRTY;
    chunk->generateState |= CHUNK_MESH_BUILDING;
    assert(chunk->generateState & CHUNK_MESH_BUILDING);
    assert(!(chunk->generateState & CHUNK_MESH_DIRTY));
    
    MemoryBarrier();
    ReadWriteBarrier();

    GenerateMeshData *data = (GenerateMeshData *)malloc(sizeof(GenerateMeshData));

    data->gameState = gameState;

    ChunkVertexToCreate *info = 0;

    if(gameState->meshGenerator.meshesToCreateFreeList) {
        info = gameState->meshGenerator.meshesToCreateFreeList;

        //NOTE: Take off free list
        gameState->meshGenerator.meshesToCreateFreeList = info->next;

        easyMemory_zeroSize(info, sizeof(ChunkVertexToCreate));
    } else {
        info = pushStruct(&globalLongTermArena, ChunkVertexToCreate);
        //TODO: Change this to a fixed sized array with each threads own memory arena
    }
    assert(info);

    info->triangleData = initResizeArray(VoxelVertex);
    info->indicesData = initResizeArray(u32);
    info->alphaTriangleData = initResizeArray(VoxelVertex);
    info->alphaIndicesData = initResizeArray(u32);

    //NOTE: Add to the list
    info->next = gameState->meshGenerator.meshesToCreate;
    gameState->meshGenerator.meshesToCreate = info;
    info->ready = false;
    info->chunk = chunk;
    info->voxelEntity = 0;
    info->generation = chunk->meshGeneration++;

    data->info = info;

    //NOTE: Multi-threaded
    pushWorkOntoQueue(&gameState->threadsInfo, generateChunkMesh_multiThread, data);

}