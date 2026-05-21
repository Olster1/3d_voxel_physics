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