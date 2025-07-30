#include "./game_defines.h"

uint64_t getBitWiseFlag(uint64_t bitWise) {
    return ((uint64_t)1 << bitWise);
}

static float global_fogFarDistance;
static float global_fogSeeDistance;

static int global_updateArbCount = 0;
static float global_timeInPhysicsUpdate_cycles = 0;
static float global_totalLoopTime = 0;

#include "./easy_memory.h"
#include "./memory_arena.h"
#include "./resize_array.cpp"
#include "./easy_string_utf8.h"
#include "./profiler.cpp"
#include "./easy_string.h"
#include "./easy_files.h"
#include "./easy_lex.h"
#include "./imgui.h"
#include "./render.h"
#include "./transform.cpp"
#include "./animation.h"
#define GJK_IMPLEMENTATION 1
#include "./easy_gjk.h"
#include <OpenGL/gl3.h>
#include "./opengl.h"
#include "./entity.cpp"
#include "./render.cpp"
#include "./opengl.cpp"
#include "./font.cpp"
#include "./particles.cpp"
#include "./load_gltf.cpp"

Renderer *initRenderer(Texture grassTexture, Texture breakBlockTexture, Texture atlasTexture, Texture whiteTexture, Texture voxelColorPallete) {
    
    Renderer *renderer = (Renderer *)malloc(sizeof(Renderer));
    
    renderer->cubeCount = 0;
    renderer->atlasQuadCount = 0;
    renderer->glyphCount = 0;
    renderer->terrainTextureHandle = grassTexture.handle;
    renderer->breakBlockTexture = breakBlockTexture.handle;
    renderer->atlasTexture = atlasTexture.handle;
    renderer->whiteTexture = whiteTexture.handle;
    renderer->voxelColorPallete = voxelColorPallete.handle;

    renderer->blockShader = loadShader(blockVertexShader, blockFragShader);
    renderer->blockGreedyShader = loadShader(blockGreedyVertexShader, blockFragShader);
    renderer->quadTextureShader = loadShader(quadVertexShader, quadTextureFragShader);
    renderer->fontTextureShader = loadShader(quadVertexShader, fontTextureFragShader);
    renderer->lineShader = loadShader(lineVertexShader, lineFragShader);
    renderer->voxelEntityShader = loadShader(voxelEntityVertexShader, voxelEntityFragShader);
    renderer->voxelEntityShaderRayCast = loadShader(voxelEntityRaycastVertexShader, voxelEntityRaycastFragShader);

    renderer->rayCastShader = loadShader(fullScreenVertexShader, rayCastFragShader);
    renderer->skyboxShader = loadShader(skyboxVertexShader, skyboxFragShader);
    renderer->quadShader = loadShader(quadVertexShader, quadFragShader);
    renderer->blockPickupShader = loadShader(blockPickupVertexShader, blockPickupFragShader);
    renderer->skeletalModelShader = loadShader(skeletalVertexShader, skeletalFragShader);
    renderer->blockSameTextureShader = loadShader(blockSameTextureVertexShader, blockPickupFragShader);
    renderer->blockColorShader = loadShader(blockVertexShader, blockFragShader);
    renderer->voxelChunkShader = loadShader(voxelChunkVertexShader, voxelChunkFragShader);

    renderer->plainBlockColorShader = loadShader(blockSameColorVertexShader, blockSameColorFragShader);
    
    renderer->blockModel = generateVertexBuffer(global_cubeData, 24, global_cubeIndices, 36);
    renderer->quadModel = generateVertexBuffer(global_quadData, 4, global_quadIndices, 6, ATTRIB_INSTANCE_TYPE_MODEL_MATRIX);
    renderer->rayTraceModel = generateRayTraceVertexBuffer(global_quadRaytraceData, 4, global_quadIndices, 6);
    
    renderer->lineModel = generateVertexBuffer(global_lineModelData, 2, global_lineIndicies, 2, ATTRIB_INSTANCE_TYPE_MODEL_MATRIX);
    renderer->blockModelWithInstancedT = generateVertexBuffer(global_cubeData, 24, global_cubeIndices, 36, ATTRIB_INSTANCE_TYPE_MODEL_MATRIX);
    renderer->blockModelSameTexture = generateVertexBuffer(global_cubeData_sameTexture, 24, global_cubeIndices, 36, ATTRIB_INSTANCE_TYPE_MODEL_MATRIX);
    

    renderer->voxelEntityMeshes = 0;

    return renderer;
}

#include "./sound.cpp"
#include "./perlin.cpp"
#include "./SimplexNoise.cpp"
#include "./interaction.cpp"
#include "./texture_atlas.cpp"
#include "./load_vox_file.cpp"
#include "./post_chunk_fill.cpp"
#include "./gameState.cpp"
#include "./entity_multithread.cpp"
#include "./chunk.cpp"
#include "./multithreaded_mesh_builder.cpp"
#include "./player.cpp"
#include "./camera.cpp"
#include "./perlin_noise_test.cpp"
#include "./imgui.cpp"
#include "./minecraft_animations.cpp"
#include "./animation.cpp"
#include "./physics_loop.cpp"

TimeOfDayValues getTimeOfDayValues(GameState *gameState) {
    float4 a;
    float4 b;

    float4 dayA = make_float4(0.678, 0.847, 0.901, 1);
    float4 dayB = make_float4(0.126, 0.162, 0.529, 1);
    
    TimeOfDayValues result = {};
    result.skyColorA = dayA;
    result.skyColorB = dayB;

    return result;
}


void updateAndDrawDebugCode(GameState *gameState) {
    {
        char s[255];
        int charsRendered = sprintf (s, "Frame Rate: %dFPS", (int)(round((1.0f / gameState->dt))));
        assert(charsRendered < arrayCount(s));
        renderText(gameState->renderer, &gameState->mainFont, s, make_float2(10, 10 + 5), 0.1f);
    }
    // {
    //     char s[255];
    //     int charsRendered = sprintf (s, "Physics Loops: %d", (int)(physicsLoopsCount));
    //     assert(charsRendered < arrayCount(s));
    //     renderText(gameState->renderer, &gameState->mainFont, s, make_float2(10, 10 + 10), 0.1f);
    // }
    // {
    //     char s[255];
    //     int charsRendered = sprintf (s, "Arb update loops: %d", global_updateArbCount);
    //     assert(charsRendered < arrayCount(s));
    //     renderText(gameState->renderer, &gameState->mainFont, s, make_float2(10, 10 + 15), 0.1f);
    // }
    {
        char s[255];
        //NOTE: 16 milliseconds is 60fps
        int charsRendered = sprintf (s, "Total Physics: %f milli seconds", (global_timeInPhysicsUpdate_cycles));
        assert(charsRendered < arrayCount(s));
        renderText(gameState->renderer, &gameState->mainFont, s, make_float2(10, 10 + 10), 0.1f);
    }

    // float yAppend = 0;
    // if(global_profiler && global_profiler->data) {
    //     for(int i = 0; i < getArrayLength(global_profiler->data); i++) {
    //             ProfileData *d = &global_profiler->data[i];

    //             float dividend = global_timeInPhysicsUpdate_cycles;

    //             if(easyString_stringsMatch_nullTerminated(d->name, "RENDER_CUBES")) {
    //                 dividend = global_totalLoopTime;
    //             }

    //             char s[255];
    //             int charsRendered = sprintf (s, "%s: %d%%", d->name, (int)((d->totalTime / dividend)*100.0f));
    //             assert(charsRendered < arrayCount(s));
    //             renderText(gameState->renderer, &gameState->mainFont, s, make_float2(10, 10 + 25 + yAppend), 0.1f);
    //             yAppend += 5;
    //     }
    // }
}

void processBuildingStructures(GameState *gameState) {
      //NOTE: Consider it's the end of the frame for chunk generation. So clear the current push node
    gameState->chunkPostFillInfo.currentPool = 0;

    PoolChunkGeneration **p = &gameState->chunkPostFillInfo.generationPools;

    //NOTE: Check all inflight chunk pools to see if they have finished 
    //      so we can label them as generated.
    while(*p) {
        assert((*p)->next != *p);
        if(getAtomicInt(&((*p)->value)) == ((*p)->targetValue)) {
            PoolChunkGeneration *entry = *p;
            for(int i = 0; i < entry->chunksInPoolCount; ++i) {
                FillChunkData *d = entry->chunksInPool[i];
                assert(d);
                assert(d->chunk);
                if(d && d->chunk) {
                    assert(d->chunk->generateState & CHUNK_GENERATING);
                    d->chunk->generateState = CHUNK_GENERATED | CHUNK_MESH_DIRTY;
                }
                easyPlatform_freeMemory(d);
            }
            
            //NOTE: Remove from the parent list
            *p = (*p)->next;

            // //NOTE: Add to this free list
            entry->next = gameState->chunkPostFillInfo.generationPoolFreeList;
            gameState->chunkPostFillInfo.generationPoolFreeList = entry;
            
        } else {
            p = &((*p)->next);
        }
    }
}

void processVoxelMeshes(GameState *gameState) {
    {
        int count = 0;
        ChunkVertexToCreate **infoPtr = &gameState->meshGenerator.meshesToCreate;
        int maxLoopCount = 10;
        
        while(*infoPtr && count < maxLoopCount) {
            ChunkVertexToCreate *info = *infoPtr;
            if(info->ready) {
                if(info->voxelEntity) {
                    assert(false);
                    // processVoxelEntityMeshData(info);
                } else {
                    processMeshData(info);
                }
                
                //NOTE: Take off list
                *infoPtr = info->next;

                //NOTE: Add to free list
                info->next = gameState->meshGenerator.meshesToCreateFreeList;
                gameState->meshGenerator.meshesToCreateFreeList = info;
                
                count++;
            } else {
                infoPtr = &info->next;
            }
        }
    }
}

void updateHotKeys(GameState *gameState) {
    float16 rot = eulerAnglesToTransform(gameState->camera.T.rotation.y, gameState->camera.T.rotation.x, gameState->camera.T.rotation.z);
    float3 zAxis = make_float3(rot.E_[2][0], rot.E_[2][1], rot.E_[2][2]);
    float3 startP = plus_float3(gameState->camera.T.pos, scale_float3(3, zAxis));

    float speed = 30;

    if(gameState->keys.keys[KEY_E] == MOUSE_BUTTON_PRESSED && !gameState->grabbed) {
        VoxelEntity *e = createVoxelCircleEntity(&gameState->voxelEntities[gameState->voxelEntityCount++], &gameState->meshGenerator, 0.4f, startP, 1.0f / 1.0f, 0);
        e->dP = scale_float3(speed, zAxis);
    }
    if(gameState->keys.keys[KEY_R] == MOUSE_BUTTON_PRESSED && !gameState->grabbed) {
        VoxelEntity *e = createVoxelSquareEntity(&gameState->voxelEntities[gameState->voxelEntityCount++], &gameState->meshGenerator, 0.4f, 0.4f, 0.4f, startP, 1.0f / 1.0f, 0);    
        e->dP = scale_float3(speed, zAxis);
    }
    if(gameState->keys.keys[KEY_P] == MOUSE_BUTTON_PRESSED) {
        gameState->gamePaused = !gameState->gamePaused;
    }
}

void updateGame(GameState *gameState) {
    Uint32 start = SDL_GetTicks();

    clearProfiler();
    
    if(!gameState->inited) {
        globalLongTermArena = createArena(Kilobytes(200));
        globalPerFrameArena = createArena(Kilobytes(100));
        perFrameArenaMark = takeMemoryMark(&globalPerFrameArena);
        initGameState(gameState);
        // DEBUG_ArrayTests();
    } else { 
        releaseMemoryMark(&perFrameArenaMark);
        perFrameArenaMark = takeMemoryMark(&globalPerFrameArena);
        gameState->renderer->voxelEntityMeshes = 0;
    }
    
    updateCamera(gameState);
  
    float16 screenGuiT = make_ortho_matrix_origin_center(100, 100*gameState->aspectRatio_y_over_x, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
    float16 textGuiT = make_ortho_matrix_top_left_corner_y_down(100, 100*gameState->aspectRatio_y_over_x, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);

    float16 screenT = make_perspective_matrix_origin_center(gameState->camera.fov, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE, gameState->aspectRatio_y_over_x);
    float16 cameraT = transform_getInverseX(gameState->camera.T);
    float16 cameraToWorldT = getModelToViewSpace(gameState->camera.T);
    float16 cameraTWithoutTranslation = getCameraX_withoutTranslation(gameState->camera.T);

    gameState->renderer->viewport = make_float2(gameState->screenWidth, gameState->screenWidth * gameState->aspectRatio_y_over_x) ; 
    gameState->renderer->projectionPlaneSize = getProjectionPlaneSize(gameState->camera.fov, gameState->aspectRatio_y_over_x);
    gameState->renderer->invViewMatrix = cameraToWorldT;

    gameState->renderer->cameraP = gameState->camera.T.pos;

    float16 rot = eulerAnglesToTransform(gameState->camera.T.rotation.y, gameState->camera.T.rotation.x, gameState->camera.T.rotation.z);
    float3 lookingAxis = make_float3(rot.E_[2][0], rot.E_[2][1], rot.E_[2][2]);

    updatePlayer(gameState);

    processVoxelMeshes(gameState);

    updatePhysicsSim(gameState);
    renderVoxelEntities(gameState);
    // drawChunkWorld(gameState, screenT, cameraT, lookingAxis, rot);

    updateHotKeys(gameState);
    
    processBuildingStructures(gameState);

    // completePhysicsDestructionForFrame(&gameState->physicsWorld, &gameState->meshGenerator);

    TimeOfDayValues timeOfDayValues = getTimeOfDayValues(gameState);
    updateAndDrawDebugCode(gameState);
    rendererFinish(gameState->renderer, screenT, cameraT, screenGuiT, textGuiT, lookingAxis, cameraTWithoutTranslation, timeOfDayValues, gameState->perlinTestTexture.handle, &gameState->voxelEntities[0], cameraToWorldT);

    Uint32 end = SDL_GetTicks();
    global_totalLoopTime = (end - start);
}