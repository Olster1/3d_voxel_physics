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
#include "./resize_array.cpp"
#include "./memory_arena.h"
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
#include "./entity.cpp"
#include "./render.cpp"
#include "./opengl.cpp"
#include "./font.cpp"
#include "./particles.cpp"
#include "./load_gltf.cpp"



Renderer *initRenderer(Texture grassTexture, Texture breakBlockTexture, Texture atlasTexture) {
    
    Renderer *renderer = (Renderer *)malloc(sizeof(Renderer));
    
    renderer->cubeCount = 0;
    renderer->atlasQuadCount = 0;
    renderer->glyphCount = 0;
    renderer->terrainTextureHandle = grassTexture.handle;
    renderer->breakBlockTexture = breakBlockTexture.handle;
    renderer->atlasTexture = atlasTexture.handle;

    renderer->blockShader = loadShader(blockVertexShader, blockFragShader);
    renderer->blockGreedyShader = loadShader(blockGreedyVertexShader, blockFragShader);
    renderer->quadTextureShader = loadShader(quadVertexShader, quadTextureFragShader);
    renderer->fontTextureShader = loadShader(quadVertexShader, fontTextureFragShader);
    renderer->lineShader = loadShader(lineVertexShader, lineFragShader);

    renderer->rayCastShader = loadShader(fullScreenVertexShader, rayCastFragShader);
    renderer->skyboxShader = loadShader(skyboxVertexShader, skyboxFragShader);
    renderer->quadShader = loadShader(quadVertexShader, quadFragShader);
    renderer->blockPickupShader = loadShader(blockPickupVertexShader, blockPickupFragShader);
    renderer->skeletalModelShader = loadShader(skeletalVertexShader, skeletalFragShader);
    renderer->blockSameTextureShader = loadShader(blockSameTextureVertexShader, blockPickupFragShader);
    renderer->blockColorShader = loadShader(blockVertexShader, blockFragShader);

    renderer->plainBlockColorShader = loadShader(blockSameColorVertexShader, blockSameColorFragShader);
    
    renderer->blockModel = generateVertexBuffer(global_cubeData, 24, global_cubeIndices, 36);
    renderer->quadModel = generateVertexBuffer(global_quadData, 4, global_quadIndices, 6, ATTRIB_INSTANCE_TYPE_MODEL_MATRIX);
    renderer->lineModel = generateVertexBuffer(global_lineModelData, 2, global_lineIndicies, 2, ATTRIB_INSTANCE_TYPE_MODEL_MATRIX);
    renderer->blockModelWithInstancedT = generateVertexBuffer(global_cubeData, 24, global_cubeIndices, 36, ATTRIB_INSTANCE_TYPE_MODEL_MATRIX);
    renderer->blockModelSameTexture = generateVertexBuffer(global_cubeData_sameTexture, 24, global_cubeIndices, 36, ATTRIB_INSTANCE_TYPE_MODEL_MATRIX);

    return renderer;
}

#include "./sound.cpp"
#include "./perlin.cpp"
#include "./SimplexNoise.cpp"
#include "./interaction.cpp"
#include "./texture_atlas.cpp"
#include "./gameState.cpp"
#include "./entity_multithread.cpp"
#include "./chunk.cpp"
#include "./player.cpp"
#include "./camera.cpp"
#include "./perlin_noise_test.cpp"
#include "./imgui.cpp"
#include "./minecraft_animations.cpp"
#include "./animation.cpp"


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

static int physicsLoopsCount = 0;


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

    float yAppend = 0;
    if(global_profiler && global_profiler->data) {
        for(int i = 0; i < getArrayLength(global_profiler->data); i++) {
                ProfileData *d = &global_profiler->data[i];

                float dividend = global_timeInPhysicsUpdate_cycles;

                if(easyString_stringsMatch_nullTerminated(d->name, "RENDER_CUBES")) {
                    dividend = global_totalLoopTime;
                }

                char s[255];
                int charsRendered = sprintf (s, "%s: %d%%", d->name, (int)((d->totalTime / dividend)*100.0f));
                assert(charsRendered < arrayCount(s));
                renderText(gameState->renderer, &gameState->mainFont, s, make_float2(10, 10 + 25 + yAppend), 0.1f);
                yAppend += 5;
        }
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
    } else { 
        releaseMemoryMark(&perFrameArenaMark);
        perFrameArenaMark = takeMemoryMark(&globalPerFrameArena);
    }
    
    updateCamera(gameState);
  
    float16 screenGuiT = make_ortho_matrix_origin_center(100, 100*gameState->aspectRatio_y_over_x, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
    float16 textGuiT = make_ortho_matrix_top_left_corner_y_down(100, 100*gameState->aspectRatio_y_over_x, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);

    float16 screenT = make_perspective_matrix_origin_center(gameState->camera.fov, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE, 1.0f / gameState->aspectRatio_y_over_x);
    float16 cameraT = transform_getInverseX(gameState->camera.T);
    float16 cameraTWithoutTranslation = getCameraX_withoutTranslation(gameState->camera.T);

    float16 rot = eulerAnglesToTransform(gameState->camera.T.rotation.y, gameState->camera.T.rotation.x, gameState->camera.T.rotation.z);
    float3 lookingAxis = make_float3(rot.E_[2][0], rot.E_[2][1], rot.E_[2][2]);

    gameState->physicsAccum += gameState->dt;

    float minStep = PHYSICS_TIME_STEP;
    global_updateArbCount = 0;

    for(int i = 0; i < gameState->voxelEntityCount; ++i) {
        VoxelEntity *e = &gameState->voxelEntities[i];
        e->ddPForFrame = make_float3(0, 0, 0);
        e->ddAForFrame = make_float3(0, 0, 0);
        if(e->inverseMass > 0 && e != gameState->grabbed) {
            e->ddPForFrame.y -= 10.0f; //NOTE: Gravity
            // e->ddAForFrame = make_float3(0, 0, 1);
        }

        e->inBounds = false;

        //NOTE: Remove debug flags
        for(int z = 0; z < e->depth; z++) {
            for(int y = 0; y < e->pitch; y++) {
                for(int x = 0; x < e->stride; x++) {
                    e->data[getVoxelIndex(e, x, y, z)] &= ~(VOXEL_COLLIDING);
                }
            }
        }
    }

    physicsLoopsCount = 0;
    Uint32 startPhysics = profiler_getCount();

    const int maxContinousIterations = 8;
    int interations = 0;
    
    //NOTE: Physics loop
    while(gameState->physicsAccum >= minStep && interations < maxContinousIterations) {
        physicsLoopsCount++;
        interations++;
        float dt = minStep;
     

        float maxRelSpeed = 0;
        {
            for(int i = 0; i < gameState->voxelEntityCount; ++i) {
                VoxelEntity *e = &gameState->voxelEntities[i];

                for(int j = i + 1; j < gameState->voxelEntityCount; ++j) {
                    VoxelEntity *e1 = &gameState->voxelEntities[j];

                    if(e != e1) {
                        if (!(e->inverseMass == 0.0f && e1->inverseMass == 0.0f)) {
                            float relSpeed = getRelativeSpeed(e, e1);

                            if(maxRelSpeed < relSpeed) {
                                maxRelSpeed = relSpeed;
                            }
                            collideEntitiesMultiThread(gameState, e, e1); 
                        }
                    } else {
                        assert(false);
                    }
                }
            }
        }   

        {
            PROFILE_FUNC(WaitForThreadsCollision);
            waitForWorkToFinish(&gameState->threadsInfo);
        }
        #if CONTINOUS_COLLISION_DETECTION
        if(maxRelSpeed > 0) {
            dt = MathMinf(VOXEL_SIZE_IN_METERS / maxRelSpeed, dt); // sec / m * voxel_size = sec
            dt = MathMaxf(dt, 1.0f / 480.0f);
        }
        #endif

        float prevDt = dt;
        dt = (gameState->gamePaused) ? 0 : dt;

        {
            PROFILE_FUNC(mergePointsToArbiter);
            VoxelCollideData *d = gameState->voxelCollideData;
            while(d) {
                mergePointsToArbiter(&gameState->physicsWorld, d->points, d->pointCount, d->a, d->b);
                VoxelCollideData *dTemp = d;
                d = d->next;

                dTemp->next = gameState->voxelCollideDataFreeList;
                gameState->voxelCollideDataFreeList = dTemp;
                
            }
            gameState->voxelCollideData = 0;
        }
        

        //NOTE: Integrate forces
        {
            PROFILE_FUNC(IntegrateForces);
            for(int i = 0; i < gameState->voxelEntityCount; ++i) {
                VoxelEntity *e = &gameState->voxelEntities[i];
                
                e->dP = plus_float3(e->dP, scale_float3(dt, e->ddPForFrame));
                e->dA = plus_float3(e->dA, scale_float3(dt, e->ddAForFrame));

                float size = float2_magnitude(e->dP.xy);
                
                if(size < PHYSICS_VELOCITY_SLEEP_THRESHOLD) {
                    if(!e->asleep) {
                        e->sleepTimer += dt;
                    }
                } else {
                    //NOTE: Make sure the entity is awake
                    e->sleepTimer = 0;
                    e->asleep = false;
                }

                if(e->sleepTimer >= PHYSICS_SLEEP_TIME_THRESHOLD) {
                    //NOTE: Object is asleep now
                    // e->asleep = true;
                }
            }
        }

        {
            PROFILE_FUNC(PrestepArbiters);
            //NOTE: Calculate constant values and conditions the iteration solver should use
            prestepAllArbiters(&gameState->physicsWorld, 1.0f / dt);
        }
        
         {
            PROFILE_FUNC(UpdateArbiters);
            //NOTE: Apply impluses
            updateAllArbiters(&gameState->physicsWorld);
         }
        
        {
            PROFILE_FUNC(IntegrateVelocities);
            //NOTE: Integrate velocities
            for(int i = 0; i < gameState->voxelEntityCount; ++i) {
                VoxelEntity *e = &gameState->voxelEntities[i];

                // if(!e->asleep) 
                {   
                    float3 v = e->dA;
                    v.x *= -1;
                    v.y *= -1;
                    v.z *= -1;
                    e->T.rotation = integrateAngularVelocity(e->T.rotation, v, dt);
                    e->T.pos = plus_float3(e->T.pos, scale_float3(dt, e->dP));
                }
            }
        }

        gameState->physicsAccum -= prevDt;
    }

    global_timeInPhysicsUpdate_cycles = profiler_getCount() - startPhysics;//SDL_GetTicks()

    
    for(int i = 0; i < gameState->voxelEntityCount; ++i) {
        PROFILE_FUNC(RENDER_CUBES);
        VoxelEntity *e = &gameState->voxelEntities[i];

        // TransformX TTemp = e->T;
        // //TODO: ROTATION
        // TTemp.rotation.z = radiansToDegrees(TTemp.rotation.z);
        // //NOTE: Draw with colliding margin
        // TTemp.scale = plus_float3(make_float3(BOUNDING_BOX_MARGIN, BOUNDING_BOX_MARGIN, BOUNDING_BOX_MARGIN), e->worldBounds);
        // float16 A = getModelToViewSpace(TTemp);
        // float16 I = float16_identity();

        // float16 T0 =  float16_multiply(A, float16_set_pos(I, make_float3(0, -0.5f, 0)));
        // float16 T1 =  float16_multiply(A, float16_set_pos(I, make_float3(0, 0.5f, 0)));
        // float16 T2 =  float16_multiply(A, float16_set_pos(eulerAnglesToTransform(0, 0, 90), make_float3(-0.5f, 0, 0)));
        // float16 T3 =  float16_multiply(A, float16_set_pos(eulerAnglesToTransform(0, 0, 90), make_float3(0.5f, 0, 0)));
        

        // //NOTE: Debug lines
        // float4 lineColor = make_float4(0, 0, 0, 1);
        // if(e->inBounds) {
        //     lineColor.y = 1;
        // }
        // pushLine(gameState->renderer, T0, lineColor);
        // pushLine(gameState->renderer, T1, lineColor);
        // pushLine(gameState->renderer, T2, lineColor);
        // pushLine(gameState->renderer, T3, lineColor);
        
        float3 center = make_float3(0.5f*e->worldBounds.x, 0.5f*e->worldBounds.y, 0.5f*e->worldBounds.z);
        
        const float16 T = sqt_to_float16(e->T.rotation, make_float3(1, 1, 1), e->T.pos);
        float halfVoxel = 0.5f*VOXEL_SIZE_IN_METERS;
        float4 color = make_float4(1, 0.5f, 0, 1);
        if(e->inverseMass == 0) {
            color.x = 0;
        }
        
        for(int z = 0; z < e->depth; ++z) {
            for(int y = 0; y < e->pitch; ++y) {
                for(int x = 0; x < e->stride; ++x) {
                    u8 state = e->data[getVoxelIndex(e, x, y, z)];

                    if(state & VOXEL_OCCUPIED) 
                    {
                        if(state & VOXEL_INSIDE) {
                            
                        } else 
                        {
                            float4 color = make_float4(1, 0.5f, 0, 1);
                            // if(state & VOXEL_COLLIDING) {
                            //     // color = make_float4(0, 0.5f, 0, 1);
                            // } else 
                            if(state & VOXEL_CORNER) {
                                color = make_float4(1, 0, 1, 1);
                            } else if(state & VOXEL_EDGE) {
                                color = make_float4(0, 1, 1, 1);
                            }

                            float x1 = x*VOXEL_SIZE_IN_METERS + halfVoxel - center.x;
                            float y1 = y*VOXEL_SIZE_IN_METERS + halfVoxel - center.y;
                            float z1 = z*VOXEL_SIZE_IN_METERS + halfVoxel - center.z;

                            pushBlockItem(gameState->renderer, T, make_float4(x1, y1, z1, VOXEL_SIZE_IN_METERS), color);
                        }
                    }
                }
            }
        }
    }
    {
        float2 p = scale_float2(3.0f, getPlaneSize(gameState->camera.fov, 1.0f / gameState->aspectRatio_y_over_x));
        float x = lerp(-p.x, p.x, make_lerpTValue(gameState->mouseP_01.x));
        float y = lerp(-p.y, p.y, make_lerpTValue(1.0f + gameState->mouseP_01.y));

        if(gameState->keys.keys[KEY_T] == MOUSE_BUTTON_PRESSED && !gameState->grabbed) {
            gameState->voxelEntities[gameState->voxelEntityCount++] = createVoxelCircleEntity(1, make_float3(x, y, 0), 1.0f / 1.0f, gameState->randomStartUpID);
        }
        if(gameState->keys.keys[KEY_Y] == MOUSE_BUTTON_PRESSED && !gameState->grabbed) {
            gameState->voxelEntities[gameState->voxelEntityCount++] = createVoxelSquareEntity(1, 1, 1, make_float3(x, y, 0), 1.0f / 1.0f, gameState->randomStartUpID);    
        }
        if(gameState->keys.keys[KEY_P] == MOUSE_BUTTON_PRESSED) {
            gameState->gamePaused = !gameState->gamePaused;
        }
        

        if(gameState->grabbed) {
            gameState->grabbed->T.pos.x = x;
            gameState->grabbed->T.pos.y = y;
            // gameState->grabbed->T.rotation.z = 0;
            gameState->grabbed->T.rotation.z += gameState->dt;
        }
    }

    

    TimeOfDayValues timeOfDayValues = getTimeOfDayValues(gameState);
    updateAndDrawDebugCode(gameState);
    rendererFinish(gameState->renderer, screenT, cameraT, screenGuiT, textGuiT, lookingAxis, cameraTWithoutTranslation, timeOfDayValues, gameState->perlinTestTexture.handle);

    Uint32 end = SDL_GetTicks();
    global_totalLoopTime = (end - start);
}