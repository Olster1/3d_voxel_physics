struct EntityChunkInfo {
    EntityID entityID;
    Chunk *chunk;
};

struct InventoryItem {
    BlockType type;
    int count;
};

struct AOOffset {
    float3 offsets[3];
};




struct ChunkListItem {
    Chunk *chunk;
    ChunkListItem *next;
};


enum BlockFlags {
    BLOCK_FLAGS_NONE = 0,
    BLOCK_EXISTS_COLLISION = 1 << 0,
    BLOCK_EXISTS = 1 << 1, //NOTE: All blocks have this
    BLOCK_FLAG_STACKABLE = 1 << 2, //NOTE: Whether you can put a block ontop of this one
    BLOCK_NOT_PICKABLE = 1 << 3, //NOTE: Whether it destroys without dropping itself to be picked up i.e. grass just gets destroyed.
    BLOCK_FLAGS_NO_MINE_OUTLINE = 1 << 4, //NOTE: Whether it shows the mining outline
    BLOCK_FLAGS_AO = 1 << 5, //NOTE: Whether it shows the mining outline
    BLOCK_FLAGS_UNSAFE_UNDER = 1 << 6, //NOTE: Whether the block should be destroyed if underneath block destroyed
    BLOCK_FLAGS_MINEABLE = 1 << 7, 

};

#define CHUNK_LIST_SIZE 4096*4

struct GameState {
    bool inited;
    float dt;
    float screenWidth;
    float aspectRatio_x_over_y;

    TextureAtlas spriteTextureAtlas;

    Interaction currentInteraction;

    float2 mouseP_screenSpace;
    float2 mouseP_01;
    float2 lastMouseP;

    int particlerCount;
    Particler particlers[512];
    
    Font mainFont;

    Texture grassTexture;
    Texture soilTexture;
    Texture circleTexture;
    Texture circleOutlineTexture;

    ChunkInfo *chunksList;

    VoxelModel buildingModel;
    
    
    MouseKeyState mouseLeftBtn;

    Block *currentMiningBlock;
    float placeBlockTimer;
    float mineBlockTimer;
    float showCircleTimer;

    WavFile cardFlipSound[2];

    int currentInventoryHotIndex;

    //NOTE: First 8 are what player as equipped in the hotbar
    int inventoryCount;
    InventoryItem playerInventory[64];

    float3 cameraOffset;
    MultiThreadedMeshList meshGenerator;

    WavFile blockBreakSound;
    WavFile blockFinishSound;
    WavFile bgMusic;
    WavFile fallBigSound;
    WavFile pickupSound;

    float16 cameraRotation;

    float3 startP;

    ThreadsInfo threadsInfo;

    PlayingSound *miningSoundPlaying;

    //NOTE: linked hash maps
    Chunk *chunks[CHUNK_LIST_SIZE];

    Renderer *renderer;

    SDL_AudioSpec audioSpec;

    Camera camera;

    float timeOfDay; //NOTE: 0 - 1 is one day

    int randomStartUpID;

    Entity player;

    #define MAX_ENTITY_COUNT 1024
    int entityCount;
    Entity *entitiesForFrame[MAX_ENTITY_COUNT]; //NOTE: Point to the entities stored on the chunk
    EntityChunkInfo entitiesForFrameChunkInfo[MAX_ENTITY_COUNT]; //NOTE: Point to the entities stored on the chunk

    int entitiesToAddCount;
    Entity entitiesToAddAfterFrame[MAX_ENTITY_COUNT]; //NOTE: Point to the entities stored on the chunk

    int entityToDeleteCount;
    int entitiesToDelete[MAX_ENTITY_COUNT];

    KeyStates keys;

    float3 cardinalOffsets[6];
    AOOffset aoOffsets[24];

    int DEBUG_BlocksDrawnForFrame;

    bool drawBlocks;

    PhysicsWorld physicsWorld;

    float3 searchOffsets[26];
    float3 searchOffsetsSmall[6];

    Texture perlinTestTexture;

    GuiState guiState;
    bool useCameraMovement;
    float3 perlinNoiseValue;

    int voxelEntityCount;
    VoxelEntity voxelEntities[64];
    float physicsAccum;

    VoxelEntity *grabbed;

    uint64_t blockFlags[BLOCK_TYPE_COUNT];

    SkeletalModel foxModel;
    float3 modelLocation;

    bool gamePaused;

    VoxelCollideData *voxelCollideDataFreeList;
    VoxelCollideData *voxelCollideData;
};

void createBlockFlags(GameState *gameState) {
    for(int i = 0; i < arrayCount(gameState->blockFlags); ++i) {
        uint64_t flags = BLOCK_EXISTS | BLOCK_FLAGS_AO | BLOCK_FLAG_STACKABLE | BLOCK_EXISTS_COLLISION | BLOCK_FLAGS_MINEABLE;
        BlockType t = (BlockType)i;
        switch(t) {
            case BLOCK_WATER: {
                flags = flags | BLOCK_FLAGS_NONE | BLOCK_FLAGS_NO_MINE_OUTLINE;
                flags &= ~(BLOCK_FLAGS_AO | BLOCK_EXISTS_COLLISION | BLOCK_FLAGS_MINEABLE);
            } break;
            case BLOCK_GRASS_SHORT_ENTITY:
            case BLOCK_GRASS_TALL_ENTITY: {
                flags = (BlockFlags)(flags | BLOCK_NOT_PICKABLE | BLOCK_FLAGS_NO_MINE_OUTLINE | BLOCK_FLAGS_UNSAFE_UNDER);
                flags &= ~(BLOCK_FLAGS_AO | BLOCK_FLAG_STACKABLE | BLOCK_EXISTS_COLLISION);
            } break;
            case BLOCK_TREE_LEAVES: {
                flags = (BlockFlags)(flags | BLOCK_EXISTS_COLLISION | BLOCK_FLAGS_NO_MINE_OUTLINE);
                flags &= ~(BLOCK_FLAGS_AO);
            } break;
            case BLOCK_NONE: {
                flags = 0;
            } break;
            default: {
                
            };
        }
        gameState->blockFlags[i] = flags;
    }
}

void createCardinalDirections(GameState *gameState) {
    gameState->cardinalOffsets[0] = make_float3(0, 1*VOXEL_SIZE_IN_METERS, 0);
    gameState->cardinalOffsets[1] = make_float3(0, -1*VOXEL_SIZE_IN_METERS, 0);
    gameState->cardinalOffsets[2] = make_float3(0, 0, 1*VOXEL_SIZE_IN_METERS);
    gameState->cardinalOffsets[3] = make_float3(0, 0, -1*VOXEL_SIZE_IN_METERS);
    gameState->cardinalOffsets[4] = make_float3(-1*VOXEL_SIZE_IN_METERS, 0, 0);
    gameState->cardinalOffsets[5] = make_float3(1*VOXEL_SIZE_IN_METERS, 0, 0);
}

void createCardinalDirectionsForMeshGenerator(MultiThreadedMeshList *meshGenerator) {
    meshGenerator->cardinalOffsets[0] = make_float3(0, 1, 0);
    meshGenerator->cardinalOffsets[1] = make_float3(0, -1, 0);
    meshGenerator->cardinalOffsets[2] = make_float3(0, 0, 1);
    meshGenerator->cardinalOffsets[3] = make_float3(0, 0, -1);
    meshGenerator->cardinalOffsets[4] = make_float3(-1, 0, 0);
    meshGenerator->cardinalOffsets[5] = make_float3(1, 0, 0);
}

void createAOOffsets(GameState *gameState) {
    for(int i = 0; i < arrayCount(global_cubeData); ++i) {
        assert(i < arrayCount(gameState->aoOffsets));

        Vertex v = global_cubeData[i];
        float3 normal = v.normal;
        float3 sizedOffset = scale_float3(2, v.pos);

        float3 masks[2] = {};
        int maskCount = 0;

        for(int k = 0; k < 3; k++) {
            if(normal.E[k] == 0) {
                assert(maskCount < arrayCount(masks));
                float3 m = make_float3(0, 0, 0);
                m.E[k] = -sizedOffset.E[k];

                masks[maskCount++] = m;
            }
        }

        gameState->aoOffsets[i].offsets[0] = plus_float3(sizedOffset, masks[0]);
        gameState->aoOffsets[i].offsets[1] = sizedOffset; 
        gameState->aoOffsets[i].offsets[2] = plus_float3(sizedOffset, masks[1]);
    }
}

void createSearchOffsets(GameState *gameState) {
    int index = 0;
    for(int z = -1; z <= 1; z++) {
        for(int y = -1; y <= 1; y++) {
            for(int x = -1; x <= 1; x++) {
                if(x == 0 && y == 0 && z == 0) {
                    continue;
                } else {
                    gameState->searchOffsets[index++] = make_float3(x, y, z);
                }

            }
        }
    }
    assert(index == 26);

    gameState->searchOffsetsSmall[0] = make_float3(1, 0, 0); 
    gameState->searchOffsetsSmall[0] = make_float3(0, 0, 1); 
    gameState->searchOffsetsSmall[0] = make_float3(-1, 0, 0); 
    gameState->searchOffsetsSmall[0] = make_float3(0, 0, -1); 

    gameState->searchOffsetsSmall[0] = make_float3(0, 1, 0); 
    gameState->searchOffsetsSmall[0] = make_float3(0, -1, 0); 
}

void initGameState(GameState *gameState) {
    srand(time(NULL));
    gameState->voxelCollideDataFreeList = 0;
    gameState->voxelCollideData = 0;
    gameState->gamePaused = false;

    initThreadQueue(&gameState->threadsInfo);

    gameState->meshGenerator.meshesToCreate = 0;
    gameState->meshGenerator.meshesToCreateFreeList = 0;
    gameState->meshGenerator.threadsInfo = &gameState->threadsInfo;
    createAOOffsets(gameState);
    createCardinalDirectionsForMeshGenerator(&gameState->meshGenerator);
    createCardinalDirections(gameState);

    gameState->randomStartUpID = rand();
    float inverseMass = 1.0f / 50000.0f;
    {
        createVoxelCircleEntity(&gameState->voxelEntities[gameState->voxelEntityCount++], &gameState->meshGenerator, 1.0f, make_float3(0, 0, 0), inverseMass, gameState->randomStartUpID);
        createVoxelCircleEntity(&gameState->voxelEntities[gameState->voxelEntityCount++], &gameState->meshGenerator, 1.0f, make_float3(2, 2, 0), inverseMass, gameState->randomStartUpID);
        createVoxelSquareEntity(&gameState->voxelEntities[gameState->voxelEntityCount++], &gameState->meshGenerator, 1, 1, 1, make_float3(0, 2, 0), inverseMass, gameState->randomStartUpID);
        createVoxelSquareEntity(&gameState->voxelEntities[gameState->voxelEntityCount++], &gameState->meshGenerator, 1, 1, 1, make_float3(0, 4, 0), inverseMass, gameState->randomStartUpID);
        createVoxelCircleEntity(&gameState->voxelEntities[gameState->voxelEntityCount++], &gameState->meshGenerator, 1.0f, make_float3(0, 6, 0), inverseMass, gameState->randomStartUpID);
        createVoxelSquareEntity(&gameState->voxelEntities[gameState->voxelEntityCount++], &gameState->meshGenerator, 1, 1, 1, make_float3(0, 12, 0), inverseMass, gameState->randomStartUpID);
        createVoxelSquareEntity(&gameState->voxelEntities[gameState->voxelEntityCount++], &gameState->meshGenerator, 1, 1, 1, make_float3(0, 14, 0), inverseMass, gameState->randomStartUpID);
        createVoxelSquareEntity(&gameState->voxelEntities[gameState->voxelEntityCount++], &gameState->meshGenerator, 1, 1, 1, make_float3(0, 16, 0), inverseMass, gameState->randomStartUpID);
        createVoxelPlaneEntity(&gameState->voxelEntities[gameState->voxelEntityCount++], &gameState->meshGenerator, 70.0f, make_float3(0, -3, 0), 0, gameState->randomStartUpID);
        // gameState->grabbed = &gameState->voxelEntities[2]; 
    }
    
    assert(BLOCK_TYPE_COUNT < 255);
    gameState->camera.fov = 60;
    gameState->camera.T.pos = make_float3(0, 0, -10);
    gameState->camera.followingPlayer = false;
    gameState->cameraOffset = CAMERA_OFFSET;
    gameState->camera.shakeTimer = -1;
    gameState->camera.runShakeTimer = -1;

    gameState->currentInventoryHotIndex = 0;

    createBlockFlags(gameState);
    memset(gameState->chunks, 0, arrayCount(gameState->chunks)*sizeof(Chunk *));

    gameState->entitiesToAddCount = 0;

    gameState->timeOfDay = 0.4f;
    
    initPlayer(&gameState->player, gameState->randomStartUpID);
    gameState->player.T.pos = gameState->camera.T.pos;

    gameState->physicsWorld.positionCorrecting = true;
    gameState->physicsWorld.warmStarting = true;
    gameState->physicsWorld.accumulateImpulses = true;

    // loadWavFile(&gameState->cardFlipSound[0], "./sounds/cardFlip.wav", &gameState->audioSpec);
    // loadWavFile(&gameState->cardFlipSound[1], "./sounds/cardFlip1.wav", &gameState->audioSpec);
    // loadWavFile(&gameState->blockBreakSound, "./sounds/blockBreak.wav", &gameState->audioSpec);
    // loadWavFile(&gameState->blockFinishSound, "./sounds/blockFinish.wav", &gameState->audioSpec);
    // loadWavFile(&gameState->fallBigSound, "./sounds/fallbig.wav", &gameState->audioSpec);
    // loadWavFile(&gameState->pickupSound, "./sounds/pop.wav", &gameState->audioSpec);
    // loadWavFile(&gameState->bgMusic, "./sounds/sweeden.wav", &gameState->audioSpec);

    gameState->lastMouseP = gameState->mouseP_screenSpace;

    gameState->grassTexture = loadTextureToGPU("./images/grass_block.png");
    Texture breakBlockTexture = loadTextureToGPU("./images/break_block.png");
    Texture atlasTexture = loadTextureToGPU("./images/atlas.png");
    Texture whiteTexture = loadTextureToGPU("./images/white.png");

    gameState->buildingModel = loadVoxFile("./models/TallBuilding01.vox");
    gameState->buildingModel.colorPalletteId = 1;
    
    int rowCount = 4;
    u32 *colors = pushArray(&globalPerFrameArena, 256*rowCount, u32);

    easyPlatform_copyMemory(colors, voxelGrassBitmap, sizeof(u32)*256);
    if(gameState->buildingModel.colors) {
        u8 *at = (u8 *)(colors + (256*gameState->buildingModel.colorPalletteId)); //NOTE: Move to the next row
        easyPlatform_copyMemory(at, gameState->buildingModel.colors, sizeof(u32)*256);
    }

    Texture voxelColorPallete = createGPUTexture(256, rowCount, colors);

    gameState->currentMiningBlock = 0;

    gameState->renderer = initRenderer(gameState->grassTexture, breakBlockTexture, atlasTexture, whiteTexture, voxelColorPallete);
    gameState->renderer->numColorPalettes = rowCount;

    gameState->mainFont = initFontAtlas("./fonts/Roboto-Regular.ttf");
    
    gameState->renderer->fontAtlasTexture = gameState->mainFont.textureHandle;

    gameState->placeBlockTimer = -1;
    gameState->mineBlockTimer = -1;
    gameState->showCircleTimer = -1;

    gameState->inventoryCount = 0;
    gameState->entityToDeleteCount = 0;

    playSound(&gameState->bgMusic);

    gameState->drawBlocks = false;


    gameState->particlerCount = 0;

    gameState->spriteTextureAtlas = readTextureAtlas("./texture_atlas.json", "./texture_atlas.png");
    
    GLint maxUniformBlockSize;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    assert((maxUniformBlockSize / sizeof(float16)) > MAX_BONES_PER_MODEL);
    
    createSearchOffsets(gameState);

    gameState->perlinTestTexture = createGPUTexture(PERLIN_SIZE, PERLIN_SIZE, 0);

    gameState->guiState.grabbedId = 0;

    gameState->useCameraMovement = true;
    gameState->perlinNoiseValue.x = 0.5f;
    gameState->perlinNoiseValue.y = 0.5f;
    gameState->perlinNoiseValue.z = 0.5f;

    gameState->inited = true;

}

