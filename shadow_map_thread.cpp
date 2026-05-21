struct ShadowMapThreadInfo {
    GameState *gameState;
    ThreadConditionVariable variables;
};


void updateShadowMap(GameState *gameState, u8 *destBuffer) {
    easyPlatform_clearMemory(destBuffer, sizeof(u8)*SHADOW_MAP_WIDTH*SHADOW_MAP_HEIGHT*SHADOW_MAP_DEPTH);
    
    float half_width  = (VOXEL_SIZE_IN_METERS * SHADOW_MAP_WIDTH)  / 2.0f;
    float half_height = (VOXEL_SIZE_IN_METERS * SHADOW_MAP_HEIGHT) / 2.0f;
    float half_depth  = (VOXEL_SIZE_IN_METERS * SHADOW_MAP_DEPTH)  / 2.0f;

    float3 shadowBoxMinCorner = make_float3(-half_width, -half_height, -half_depth);

    for(int entityIndex = 0; entityIndex < gameState->voxelEntityCount; ++entityIndex) {
        VoxelEntity *e = &gameState->voxelEntities[entityIndex];
        float16 T = sqt_to_float16(e->T.rotation, make_float3(1, 1, 1), e->T.pos);

        int voxelIndex = 0;
        for (int k = 0; k < e->depth; k++) {
            for (int j = 0; j < e->pitch; j++) {
                for (int i = 0; i < e->stride; i++) {
                    u8 flags = e->data[voxelIndex++];

                    if (flags & VOXEL_OCCUPIED) {
                        float3 p = make_float3(i, j, k);
                        float3 modelP = getVoxelPositionInModelSpaceFromCenter(e, p);
                        float4 worldP = float16_transform(T, make_float4(modelP.x, modelP.y, modelP.z, 1));

                        if (isfinite(worldP.x) && isfinite(worldP.y) && isfinite(worldP.z)) {
                            int x = (int)((worldP.x - shadowBoxMinCorner.x) * VOXELS_PER_METER);
                            int y = (int)((worldP.y - shadowBoxMinCorner.y) * VOXELS_PER_METER);
                            int z = (int)((worldP.z - shadowBoxMinCorner.z) * VOXELS_PER_METER);

                            if (x >= 0 && x < SHADOW_MAP_WIDTH &&
                                y >= 0 && y < SHADOW_MAP_HEIGHT &&
                                z >= 0 && z < SHADOW_MAP_DEPTH) {
                                int index = ((z * SHADOW_MAP_HEIGHT * SHADOW_MAP_WIDTH) + (y * SHADOW_MAP_WIDTH) + x);
                                destBuffer[index] = 1; 
                            }
                        }
                    }
                }
            }
        }
    }
}

int shadowMapThreadEntryPoint(void *Info_) {
    ShadowMapThreadInfo *Info = (ShadowMapThreadInfo *)Info_;
    GameState *gameState = Info->gameState;

    for(;;) {
        // 2. Safely read from the backbuffer completely outside of the mutex lock.
        // The main thread will never touch this memory block while we are here.
        updateShadowMap(gameState, gameState->shadowMapWorkerBackbuffer);

        // 3. Handshake and Swap
        SDL_LockMutex(Info->variables.mutex);
        
        // Swap pointers! What was our backbuffer is now the new GPU target,
        // and what was the old GPU target becomes our stale backbuffer to overwrite next frame.
        u8 *temp = gameState->shadowMapGPUReady;
        gameState->shadowMapGPUReady = gameState->shadowMapWorkerBackbuffer;
        gameState->shadowMapWorkerBackbuffer = temp;

        gameState->shouldUploadShadowMapToGPU = true;
        
        // NOTE: We completely remove the SDL_CondWait here! 
        // The thread no longer needs to wait for the main thread to finish uploading.
        // It can instantly unlock and start computing the next frame's geometry.
        
        SDL_UnlockMutex(Info->variables.mutex);
    }

    return 0;
}


void initShadowMapThread(GameState *gameState) {
    size_t mapSize = sizeof(u8) * SHADOW_MAP_WIDTH * SHADOW_MAP_HEIGHT * SHADOW_MAP_DEPTH;
    
    // Allocate two separate blocks of memory
    gameState->shadowMapGPUReady = (u8 *)easyPlatform_allocateMemory(mapSize);
    gameState->shadowMapWorkerBackbuffer = (u8 *)easyPlatform_allocateMemory(mapSize);
    
    gameState->shouldUploadShadowMapToGPU = false;

    ShadowMapThreadInfo *info = pushStruct(&globalLongTermArena, ShadowMapThreadInfo);
    info->gameState = gameState;
    info->variables.condition = SDL_CreateCond();
    info->variables.mutex = SDL_CreateMutex();
    gameState->shadowMapThreadInfo = info->variables;


    //NOTE: Shadow map thread
    SDL_CreateThread(shadowMapThreadEntryPoint, "", info);
}

//NOTE: On the main thread, main thread has to be in charge of uploading to the GPU
void mainThread_signifyRebuild(GameState *gameState) {
    SDL_LockMutex(gameState->shadowMapThreadInfo.mutex);
    
    if(gameState->shouldUploadShadowMapToGPU) {
        // 4. Send the completely stable GPUReady buffer over to OpenGL/DirectX
        update3dTextureData(gameState->renderer->shadowMapVoxelHandle, gameState->shadowMapGPUReady);
        gameState->shouldUploadShadowMapToGPU = false;
    }

    SDL_UnlockMutex(gameState->shadowMapThreadInfo.mutex);
}

void initShadowThreadBuffers(GameState *gameState) {
    size_t mapSizeInBytes = SHADOW_MAP_WIDTH * SHADOW_MAP_HEIGHT * SHADOW_MAP_DEPTH;
   
}

void initShadowMapState(GameState *gameState) {
    size_t mapSize = sizeof(u8) * SHADOW_MAP_WIDTH * SHADOW_MAP_HEIGHT * SHADOW_MAP_DEPTH;
    gameState->shadowMapGPUReady = (u8 *)easyPlatform_allocateMemory(mapSize);
     int maxThreads = gameState->threadsInfo.threadCount;
    // Allocate a persistent pool of memory that never gets wiped
    gameState->persistentThreadBuffers = (u8 *)pushSize(&globalLongTermArena, mapSize * maxThreads);
}

struct ShadowMapData {
    VoxelEntity *entities;
    u8 *destBuffer;
    int startIndex;
    int endIndex;
    float3 shadowBoxMinCorner;
    int dirtyMinIndex;
    int dirtyMaxIndex;
};

void writeShadowMapEntitiesNotSimd(void *data_) {
    ShadowMapData *data = (ShadowMapData *)data_;
    
    const int strideY = SHADOW_MAP_WIDTH;
    const int strideZ = SHADOW_MAP_HEIGHT * SHADOW_MAP_WIDTH;

    int localDirtyMin = data->dirtyMinIndex;
    int localDirtyMax = data->dirtyMaxIndex;

    for (int entityIndex = data->startIndex; entityIndex < data->endIndex; ++entityIndex) {
        VoxelEntity *e = &data->entities[entityIndex];

        float16 T = sqt_to_float16(e->T.rotation, make_float3(1, 1, 1), e->T.pos);

        // 1. Compute where the local voxel grid's origin (0,0,0) lands in world space.
        float3 modelP0 = getVoxelPositionInModelSpaceFromCenter(e, make_float3(0, 0, 0));
        float4 worldP0_4 = float16_transform(T, make_float4(modelP0.x, modelP0.y, modelP0.z, 1.0f));
        float3 worldP0 = make_float3(worldP0_4.x, worldP0_4.y, worldP0_4.z);

        // 2. Compute where stepping 1 unit along each local axis lands in world space.
        float3 modelPX = getVoxelPositionInModelSpaceFromCenter(e, make_float3(1, 0, 0));
        float4 worldPX_4 = float16_transform(T, make_float4(modelPX.x, modelPX.y, modelPX.z, 1.0f));
        float3 worldPX = make_float3(worldPX_4.x, worldPX_4.y, worldPX_4.z);

        float3 modelPY = getVoxelPositionInModelSpaceFromCenter(e, make_float3(0, 1, 0));
        float4 worldPY_4 = float16_transform(T, make_float4(modelPY.x, modelPY.y, modelPY.z, 1.0f));
        float3 worldPY = make_float3(worldPY_4.x, worldPY_4.y, worldPY_4.z);

        float3 modelPZ = getVoxelPositionInModelSpaceFromCenter(e, make_float3(0, 0, 1));
        float4 worldPZ_4 = float16_transform(T, make_float4(modelPZ.x, modelPZ.y, modelPZ.z, 1.0f));
        float3 worldPZ = make_float3(worldPZ_4.x, worldPZ_4.y, worldPZ_4.z);

        // 3. Calculate the exact delta step vectors in world space
        float3 deltaX = minus_float3(worldPX, worldP0);
        float3 deltaY = minus_float3(worldPY, worldP0);
        float3 deltaZ = minus_float3(worldPZ, worldP0);

        // 4. Scale our delta vectors by your shadow map voxel density resolution
        deltaX = make_float3(deltaX.x * VOXELS_PER_METER, deltaX.y * VOXELS_PER_METER, deltaX.z * VOXELS_PER_METER);
        deltaY = make_float3(deltaY.x * VOXELS_PER_METER, deltaY.y * VOXELS_PER_METER, deltaY.z * VOXELS_PER_METER);
        deltaZ = make_float3(deltaZ.x * VOXELS_PER_METER, deltaZ.y * VOXELS_PER_METER, deltaZ.z * VOXELS_PER_METER);

        // 5. Convert our absolute starting position into shadow map voxel space
        float3 shadowSpaceOrigin = make_float3(
            (worldP0.x - data->shadowBoxMinCorner.x) * VOXELS_PER_METER,
            (worldP0.y - data->shadowBoxMinCorner.y) * VOXELS_PER_METER,
            (worldP0.z - data->shadowBoxMinCorner.z) * VOXELS_PER_METER
        );

        int voxelIndex = 0;

        // Loop K: Depth (Z Axis)
        float3 k_Pos = shadowSpaceOrigin;
        for (int k = 0; k < e->depth; k++) {
            
            // Loop J: Pitch (Y Axis)
            float3 j_Pos = k_Pos;
            for (int j = 0; j < e->pitch; j++) {
                
                // Loop I: Stride (X Axis)
                float3 i_Pos = j_Pos;
                for (int i = 0; i < e->stride; i++) {
                    u8 flags = e->data[voxelIndex++];

                    if (flags & VOXEL_OCCUPIED) {
                        // Cast directly to integers — no matrix math or transformations!
                        int x = (int)i_Pos.x;
                        int y = (int)i_Pos.y;
                        int z = (int)i_Pos.z;

                        if (x >= 0 && x < SHADOW_MAP_WIDTH &&
                            y >= 0 && y < SHADOW_MAP_HEIGHT &&
                            z >= 0 && z < SHADOW_MAP_DEPTH) {
                            
                            int index = (z * strideZ) + (y * strideY) + x;
                            data->destBuffer[index] = 1; 

                            if (index < localDirtyMin) localDirtyMin = index;
                            if (index > localDirtyMax) localDirtyMax = index;
                        }
                    }
                    
                    // Step forward sequentially along the local X axis
                    i_Pos.x += deltaX.x; i_Pos.y += deltaX.y; i_Pos.z += deltaX.z;
                }
                // Step forward sequentially along the local Y axis
                j_Pos.x += deltaY.x; j_Pos.y += deltaY.y; j_Pos.z += deltaY.z;
            }
            // Step forward sequentially along the local Z axis
            k_Pos.x += deltaZ.x; k_Pos.y += deltaZ.y; k_Pos.z += deltaZ.z;
        }
    }

    data->dirtyMinIndex = localDirtyMin;
    data->dirtyMaxIndex = localDirtyMax;
}

void writeShadowMapEntities(void *data_) {
    ShadowMapData *data = (ShadowMapData *)data_;
    
    const int strideY = SHADOW_MAP_WIDTH;
    const int strideZ = SHADOW_MAP_HEIGHT * SHADOW_MAP_WIDTH;

    int localDirtyMin = data->dirtyMinIndex;
    int localDirtyMax = data->dirtyMaxIndex;

    // 1. Create native NEON registers for boundary constraints
    float32x4_t minBounds = vdupq_n_f32(0.0f); // [0.0, 0.0, 0.0, 0.0]
    
    // NEON layout: index 0=X, 1=Y, 2=Z, 3=W
    float32x4_t maxBounds = {
        (float)SHADOW_MAP_WIDTH,
        (float)SHADOW_MAP_HEIGHT,
        (float)SHADOW_MAP_DEPTH,
        0.0f
    };

    for (int entityIndex = data->startIndex; entityIndex < data->endIndex; ++entityIndex) {
        VoxelEntity *e = &data->entities[entityIndex];

        float16 T = sqt_to_float16(e->T.rotation, make_float3(1, 1, 1), e->T.pos);

        // (Keeping your base math pipeline for delta calculations intact)
        float3 worldP0 = make_float3(
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,0)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,0)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,0)).z, 1.0f)).x,
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,0)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,0)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,0)).z, 1.0f)).y,
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,0)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,0)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,0)).z, 1.0f)).z
        );

        float3 worldPX = make_float3(
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(1,0,0)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(1,0,0)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(1,0,0)).z, 1.0f)).x,
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(1,0,0)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(1,0,0)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(1,0,0)).z, 1.0f)).y,
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(1,0,0)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(1,0,0)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(1,0,0)).z, 1.0f)).z
        );
        
        float3 worldPY = make_float3(
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,1,0)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,1,0)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,1,0)).z, 1.0f)).x,
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,1,0)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,1,0)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,1,0)).z, 1.0f)).y,
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,1,0)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,1,0)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,1,0)).z, 1.0f)).z
        );

        float3 worldPZ = make_float3(
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,1)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,1)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,1)).z, 1.0f)).x,
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,1)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,1)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,1)).z, 1.0f)).y,
            float16_transform(T, make_float4(getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,1)).x, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,1)).y, getVoxelPositionInModelSpaceFromCenter(e, make_float3(0,0,1)).z, 1.0f)).z
        );

        float3 dX = minus_float3(worldPX, worldP0);
        float3 dY = minus_float3(worldPY, worldP0);
        float3 dZ = minus_float3(worldPZ, worldP0);

        // 2. Load delta step increments into NEON float registers [X, Y, Z, W]
        float32x4_t simdDeltaX = { dX.x * VOXELS_PER_METER, dX.y * VOXELS_PER_METER, dX.z * VOXELS_PER_METER, 0.0f };
        float32x4_t simdDeltaY = { dY.x * VOXELS_PER_METER, dY.y * VOXELS_PER_METER, dY.z * VOXELS_PER_METER, 0.0f };
        float32x4_t simdDeltaZ = { dZ.x * VOXELS_PER_METER, dZ.y * VOXELS_PER_METER, dZ.z * VOXELS_PER_METER, 0.0f };

        float3 origin = make_float3(
            (worldP0.x - data->shadowBoxMinCorner.x) * VOXELS_PER_METER,
            (worldP0.y - data->shadowBoxMinCorner.y) * VOXELS_PER_METER,
            (worldP0.z - data->shadowBoxMinCorner.z) * VOXELS_PER_METER
        );
        float32x4_t shadowSpaceOrigin = { origin.x, origin.y, origin.z, 0.0f };

        int voxelIndex = 0;

        float32x4_t k_Pos = shadowSpaceOrigin;
        for (int k = 0; k < e->depth; k++) {
            
            float32x4_t j_Pos = k_Pos;
            for (int j = 0; j < e->pitch; j++) {
                
                float32x4_t i_Pos = j_Pos;
                for (int i = 0; i < e->stride; i++) {
                    u8 flags = e->data[voxelIndex++];

                    if (flags & VOXEL_OCCUPIED) {
                        // 3. Parallel Boundary Check using ARM NEON
                        // Checks: i_Pos >= minBounds AND i_Pos < maxBounds
                        uint32x4_t gteMin = vcgeq_f32(i_Pos, minBounds);
                        uint32x4_t ltMax  = vcltq_f32(i_Pos, maxBounds);
                        uint32x4_t inside = vandq_u32(gteMin, ltMax);
                        
                        // Extract lanes 0, 1, 2 (X, Y, Z) and combine them to evaluate pass state
                        uint32_t x_ok = vgetq_lane_u32(inside, 0);
                        uint32_t y_ok = vgetq_lane_u32(inside, 1);
                        uint32_t z_ok = vgetq_lane_u32(inside, 2);

                        // On ARM, comparison masks set all bits to 1 (0xFFFFFFFF) if true
                        if (x_ok && y_ok && z_ok) {
                            
                            // 4. Vectorized Truncation (Float to Int)
                            // Converts X, Y, Z lanes simultaneously to integers via hardware
                            int32x4_t indices = vcvtq_s32_f32(i_Pos);

                            int x = vgetq_lane_s32(indices, 0);
                            int y = vgetq_lane_s32(indices, 1);
                            int z = vgetq_lane_s32(indices, 2);

                            int index = (z * strideZ) + (y * strideY) + x;
                            data->destBuffer[index] = 1; 

                            if (index < localDirtyMin) localDirtyMin = index;
                            if (index > localDirtyMax) localDirtyMax = index;
                        }
                    }
                    
                    // 5. Native ARM Vector addition in one pipeline step
                    i_Pos = vaddq_f32(i_Pos, simdDeltaX);
                }
                j_Pos = vaddq_f32(j_Pos, simdDeltaY);
            }
            k_Pos = vaddq_f32(k_Pos, simdDeltaZ);
        }
    }

    data->dirtyMinIndex = localDirtyMin;
    data->dirtyMaxIndex = localDirtyMax;
}

void updateShadowMapMultThreaded(GameState *gameState, u8 *destBuffer) {
    size_t mapSizeInBytes = SHADOW_MAP_WIDTH * SHADOW_MAP_HEIGHT * SHADOW_MAP_DEPTH;
    
    // Fast SIMD clear of the final destination buffer
    memset(destBuffer, 0, mapSizeInBytes);
    
    float half_width  = (VOXEL_SIZE_IN_METERS * SHADOW_MAP_WIDTH)  / 2.0f;
    float half_height = (VOXEL_SIZE_IN_METERS * SHADOW_MAP_HEIGHT) / 2.0f;
    float half_depth  = (VOXEL_SIZE_IN_METERS * SHADOW_MAP_DEPTH)  / 2.0f;
    float3 shadowBoxMinCorner = make_float3(-half_width, -half_height, -half_depth);
    
    int threadCount = gameState->threadsInfo.threadCount;
    int chunkCount = gameState->voxelEntityCount / threadCount;

    ShadowMapData **jobPackets = pushArray(&globalPerFrameArena, threadCount, ShadowMapData*);

    // --- LAUNCH PHASE ---
    for (int i = 0; i < threadCount; i++) {
        int startIndex = i * chunkCount; 
        int endIndex = startIndex + chunkCount;
        if (i == threadCount - 1) {
            endIndex = gameState->voxelEntityCount;
        }

        ShadowMapData *data = pushStruct(&globalPerFrameArena, ShadowMapData);
        data->entities = gameState->voxelEntities;
        data->startIndex = startIndex;
        data->endIndex = endIndex;
        data->shadowBoxMinCorner = shadowBoxMinCorner;

        // FIX 1: Point to our persistent, pre-allocated pool instead of making a new allocation!
        data->destBuffer = &gameState->persistentThreadBuffers[i * mapSizeInBytes];
        
        // Fast SIMD clear on memory that is already warm and mapped by the OS
        memset(data->destBuffer, 0, mapSizeInBytes);

        // Track the bounding box indices of what this thread actually alters
        // (Initialize to inverted extremes)
        data->dirtyMinIndex = mapSizeInBytes;
        data->dirtyMaxIndex = 0;

        jobPackets[i] = data;

        pushWorkOntoQueue(&gameState->threadsInfo, writeShadowMapEntities, data, &gameState->threadsInfo.perFrameQueue_);
    }

    // --- SYNC PHASE ---
    waitForPerFrameWorkToFinish(&gameState->threadsInfo);

    // --- MERGE PHASE (Main Thread) ---
    // FIX 2: Process 8 bytes (voxels) at a time using uint64_t operations
    uint64_t *destAsU64 = (uint64_t *)destBuffer;
    
    for (int i = 0; i < threadCount; i++) {
        ShadowMapData *packet = jobPackets[i];
        
        // FIX 3: Skip completely if the thread didn't write any shadow voxels
        if (packet->dirtyMinIndex > packet->dirtyMaxIndex) {
            continue; 
        }

        // Align our dirty boundaries to 8-byte (uint64_t) chunks
        size_t startChunk = packet->dirtyMinIndex / sizeof(uint64_t);
        size_t endChunk   = (packet->dirtyMaxIndex / sizeof(uint64_t)) + 1;
        
        uint64_t *srcAsU64 = (uint64_t *)packet->destBuffer;
        
        // Only merge the narrow window of memory that was actually touched!
        for (size_t chunkIdx = startChunk; chunkIdx < endChunk; ++chunkIdx) {
            destAsU64[chunkIdx] |= srcAsU64[chunkIdx];
        }
    }
}

void singleThreadedShadowMap(GameState *gameState) {
    // Note: Syncing now happens inside the function below before it returns!
    updateShadowMapMultThreaded(gameState, gameState->shadowMapGPUReady);
    update3dTextureDataPbo(gameState->renderer->shadowMapVoxelHandle, gameState->renderer->shadowMapPboVoxelHandle, gameState->shadowMapGPUReady);
}