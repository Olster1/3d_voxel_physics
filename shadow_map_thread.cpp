
void updateShadowMap(void *gameState_) {
    GameState *gameState = (GameState *)gameState_;
    easyPlatform_clearMemory(gameState->shadowMap, sizeof(u8)*SHADOW_MAP_WIDTH*SHADOW_MAP_HEIGHT*SHADOW_MAP_DEPTH);
    for(int entityIndex = 0; entityIndex < gameState->voxelEntityCount; ++entityIndex) {
        VoxelEntity *e = &gameState->voxelEntities[entityIndex];

        for (int k = 0; k < e->depth; k++) {
            for (int j = 0; j < e->pitch; j++) {
                for (int i = 0; i < e->stride; i++)
                {
                    u8 flags = e->data[getVoxelIndex(e, i, j, k)];

                    if (flags & VOXEL_OCCUPIED) {
                        //NOTE: get proper matrix for conversion 
                        float3 p = make_float3(i, j, k);
                        float3 modelP = getVoxelPositionInModelSpaceFromCenter(e, p);
                        float16 T = sqt_to_float16(e->T.rotation, make_float3(1, 1, 1), e->T.pos);
                        float4 worldP = float16_transform(T, make_float4(modelP.x, modelP.y, modelP.z, 1));

                        float3 origin = make_float3(0, 0, 0);

                        int x = (int)((worldP.x - origin.x) * VOXELS_PER_METER);
                        int y = (int)((worldP.y - origin.y) * VOXELS_PER_METER);
                        int z = (int)((worldP.z - origin.z) * VOXELS_PER_METER);

                        if (isfinite(x) && isfinite(y) && isfinite(z) &&
                            x >= 0 && x < SHADOW_MAP_WIDTH &&
                            y >= 0 && y < SHADOW_MAP_HEIGHT &&
                            z >= 0 && z < SHADOW_MAP_DEPTH) {
                            int index = (z * SHADOW_MAP_HEIGHT * SHADOW_MAP_WIDTH + y * SHADOW_MAP_WIDTH + x);
                            gameState->shadowMap[index] = 1;
                        }
                    }
                }
            }
        }
    }
}

struct ShadowMapThreadInfo {
    GameState *gameState;
    ThreadConditionVariable variables;
};

int shadowMapThreadEntryPoint(void *Info_) {
    ShadowMapThreadInfo *Info = (ShadowMapThreadInfo *)Info_;
    GameState *gameState = Info->gameState;

    for(;;) {
        updateShadowMap(gameState);

        MemoryBarrier();
        ReadWriteBarrier();
        
        SDL_LockMutex(Info->variables.mutex);
        gameState->shouldUploadShadowMapToGPU = true;
        while (!Info->variables.ready) {
            SDL_CondWait(Info->variables.condition, Info->variables.mutex);
        }
        Info->variables.ready = false; // Mark job as taken
        SDL_UnlockMutex(Info->variables.mutex);
        
    }

    return 0;
}

void initShadowMapThread(GameState *gameState) {
    ShadowMapThreadInfo *info = pushStruct(&globalLongTermArena, ShadowMapThreadInfo);
    info->gameState = gameState;
    info->variables.condition = SDL_CreateCond();
    info->variables.mutex = SDL_CreateMutex();
    gameState->shadowMapThreadInfo = info->variables;


    //NOTE: Shadow map thread
    SDL_CreateThread(shadowMapThreadEntryPoint, "", info);
}

void mainThread_signifyRebuild(GameState *gameState) {
    SDL_LockMutex(gameState->shadowMapThreadInfo.mutex);
    if(gameState->shouldUploadShadowMapToGPU) {
        update3dTextureData(gameState->renderer->shadowMapVoxelHandle, gameState->shadowMap);
        gameState->shouldUploadShadowMapToGPU = false;
    }
    gameState->shadowMapThreadInfo.ready = true;
    SDL_CondSignal(gameState->shadowMapThreadInfo.condition);

    SDL_UnlockMutex(gameState->shadowMapThreadInfo.mutex);
}