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
#include <OpenGL/gl3.h>
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

    renderer->rayTraceShader = loadShader(rayTraceVertexShader, rayTraceFragShader);

    renderer->plainBlockColorShader = loadShader(blockSameColorVertexShader, blockSameColorFragShader);
    
    renderer->blockModel = generateVertexBuffer(global_cubeData, 24, global_cubeIndices, 36);
    renderer->quadModel = generateVertexBuffer(global_quadData, 4, global_quadIndices, 6, ATTRIB_INSTANCE_TYPE_MODEL_MATRIX);
    renderer->rayTraceModel = generateRayTraceVertexBuffer(global_quadRaytraceData, 4, global_quadIndices, 6);
    
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

void updateBufferDataWithCameraRays(Renderer *renderer, float3 *cameraRays) {
    size_t vertexSize = sizeof(global_quadRaytraceData[0]);

    global_quadRaytraceData[0].normal = cameraRays[0];
    global_quadRaytraceData[1].normal = cameraRays[1];
    global_quadRaytraceData[2].normal = cameraRays[2];
    global_quadRaytraceData[3].normal = cameraRays[3];

    assert(vertexSize == sizeof(Vertex));
    updateInstanceDataSub(renderer->rayTraceModel.instanceBufferhandle, global_quadRaytraceData, arrayCount(global_quadRaytraceData)*vertexSize);
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
    
  
    float16 screenGuiT = make_ortho_matrix_origin_center(100, 100*gameState->aspectRatio_x_over_y, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
    float16 textGuiT = make_ortho_matrix_top_left_corner_y_down(100, 100*gameState->aspectRatio_x_over_y, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);

    float16 screenT = make_perspective_matrix_origin_center(gameState->camera.fov, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE, 1.0f / gameState->aspectRatio_x_over_y);
    float16 cameraT = transform_getInverseX(gameState->camera.T);
    float16 cameraToWorldT = getModelToViewSpace(gameState->camera.T);
    float16 cameraTWithoutTranslation = getCameraX_withoutTranslation(gameState->camera.T);

    gameState->renderer->cameraP = gameState->camera.T.pos;

    float16 rot = eulerAnglesToTransform(gameState->camera.T.rotation.y, gameState->camera.T.rotation.x, gameState->camera.T.rotation.z);
    float3 lookingAxis = make_float3(rot.E_[2][0], rot.E_[2][1], rot.E_[2][2]);

    // updatePhysicsSim(gameState);
    renderVoxelEntities(gameState);

    // drawChunkWorld(gameState, screenT, cameraT, lookingAxis, rot);

    {
        float2 p = scale_float2(3.0f, getPlaneSize(gameState->camera.fov, 1.0f / gameState->aspectRatio_x_over_y));
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

    float3 cameraCornerRays[4];
    {

        float2 halfPlaneSize = scale_float2(0.5f, getPlaneSize(gameState->camera.fov, gameState->aspectRatio_x_over_y));
        cameraCornerRays[0] = normalize_float3(make_float3(-halfPlaneSize.x, halfPlaneSize.y, 1));
        cameraCornerRays[1] = normalize_float3(make_float3(-halfPlaneSize.x, -halfPlaneSize.y, 1));
        cameraCornerRays[2] = normalize_float3(make_float3(halfPlaneSize.x, -halfPlaneSize.y, 1));
        cameraCornerRays[3] = normalize_float3(make_float3(-halfPlaneSize.x, halfPlaneSize.y, 1));
    }

    updateBufferDataWithCameraRays(gameState->renderer, cameraCornerRays);

    

    TimeOfDayValues timeOfDayValues = getTimeOfDayValues(gameState);
    updateAndDrawDebugCode(gameState);
    rendererFinish(gameState->renderer, screenT, cameraT, screenGuiT, textGuiT, lookingAxis, cameraTWithoutTranslation, timeOfDayValues, gameState->perlinTestTexture.handle, &gameState->voxelEntities[0], cameraToWorldT);

    Uint32 end = SDL_GetTicks();
    global_totalLoopTime = (end - start);
}