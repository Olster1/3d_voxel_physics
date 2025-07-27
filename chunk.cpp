enum DimensionEnum {
    DIMENSION_X,
    DIMENSION_Y,
    DIMENSION_Z
};

void initPickupItem(GameState *gameState, Chunk *chunk, float3 pos, BlockType itemType, int randomStartUpID) {
    if(!chunk->entities) {
        chunk->entities = initResizeArray(Entity);
    }

    Entity *e = 0;
    assert(gameState->entitiesToAddCount < arrayCount(gameState->entitiesToAddAfterFrame));
    if(gameState->entitiesToAddCount < arrayCount(gameState->entitiesToAddAfterFrame)) {
        e = &gameState->entitiesToAddAfterFrame[gameState->entitiesToAddCount++];
    }
    assert(e);
    if(e) {
        initBaseEntity(e, randomStartUpID);
        e->T.pos = pos;
        float scale = 0.3f;
        e->T.scale = make_float3(scale, scale, scale);
        e->type = ENTITY_PICKUP_ITEM;
        e->offset = make_float3(0, 0, 0);
        e->grounded = false;
        e->itemType = itemType;
        e->flags = SHOULD_ROTATE;
        e->dP.x = randomBetween(-2, 2);
        e->dP.z = randomBetween(-2, 2);
        e->dP.y = 5;
    }

}



BlockFlags getBlockFlags(GameState *gameState, u8 type) {
    BlockType t = (BlockType)type;
    assert(t < arrayCount(gameState->blockFlags));
    return (BlockFlags)gameState->blockFlags[t];

}

float convertRealWorldToBlockCoords(float p) {
    p = round(p*10)*0.1;

    return p;
}

float3 convertRealWorldToBlockCoords(float3 p) {
    //NOTE: The origin is at the center of a block
    //NOTE: So 0.5 & 1.4 should both map to 1 - I think the +0.5
    //NOTE: So -0.5 & -1.4 should both map to -1 - I think the -0.5
    //NOTE: So -0.4 & 0.4 should both map to 0 - I think the -0.5
    p.x = round(p.x*10)*0.1;
    p.y = round(p.y*10)*0.1;
    p.z = round(p.z*10)*0.1;

    return p;
}

int roundChunkCoord(float value) {
    return floor(round(value*10)*0.1f);
}

uint32_t getHashForChunk(int x, int y, int z) {
    int values[3] = {x, y, z};
    uint32_t hash = get_crc32((char *)values, arrayCount(values)*sizeof(int));
    hash = hash & (CHUNK_LIST_SIZE - 1);
    assert(hash < CHUNK_LIST_SIZE);
    assert(hash >= 0);
    return hash;
}

float getBlockTime(BlockType type) {
    float result = 2.0f;

    if(type == BLOCK_TREE_WOOD) {
        result = 4.0f;
    } else if(type == BLOCK_TREE_LEAVES) {
        result = 0.1f;
    } else if(type == BLOCK_GRASS_TALL_ENTITY || type == BLOCK_GRASS_SHORT_ENTITY) {
        result = 0.05f;
    }

    // result = 0.01f;

    return result;
}

struct AoMaskData {
    GameState *gameState;
    float3 worldP;
    BlockFlags blockFlags; 
    Block *b;
};

uint64_t getInvalidAoMaskValue() {
    return (((uint64_t)(1)) << AO_BIT_INVALID);
}

float2 getBlockColorIdRange(BlockType type) {
    float2 result = make_float2(0, 255);

    if(type == BLOCK_GRASS) {
        result = make_float2(0, 2);
    } else if(type == BLOCK_SOIL) {
        result = make_float2(3, 5);
    } else if(type == BLOCK_STONE) {
        result = make_float2(6, 8);
    }

    return result;
}

Block spawnBlock(int x, int y, int z, BlockType type, int colorPalleteId = -1, int palletteId = 0) {
    //NOTE: Input positions are local to chunk
    Block b = {};

    b.x = x;
    b.y = y;
    b.z = z;

    b.type = type;
    if(colorPalleteId < 0) {
        float2 range = getBlockColorIdRange(type);
        b.colorId = round(randomBetween(range.x, range.y)); //NOTE: Random color between 0 - 255 inclusive. 
    } else {
        b.colorId = colorPalleteId;
    }
    
    b.palleteId = palletteId;

    b.timeLeft = getBlockTime((BlockType)type);

    b.aoMask = 0;

    b.exists = true;
    
    return b;
}

int getBlockIndex(int x, int y, int z) {
    int result = 0;

    result += abs(z)*CHUNK_DIM*CHUNK_DIM;
    result += abs(y)*CHUNK_DIM;
    result += abs(x);

    assert(result < CHUNK_DIM*CHUNK_DIM*CHUNK_DIM);

    return result;
}

#define getChunkNoGenerate(gameState, x, y, z) getChunk_(gameState, x, y, z, true, false)
#define getChunk(gameState, x, y, z) getChunk_(gameState, x, y, z, true, true)
#define getChunkReadOnly(gameState, x, y, z) getChunk_(gameState, x, y, z, false, false)

Chunk *generateChunk(GameState *gameState, int x, int y, int z, uint32_t hash);
Chunk *getChunk_(GameState *gameState, int x, int y, int z, bool shouldGenerateChunk, bool shouldGenerateFully);

#include "./generate.cpp"

Chunk *getChunk_(GameState *gameState, int x, int y, int z, bool shouldGenerateChunk, bool shouldGenerateFully) {
    uint32_t hash = getHashForChunk(x, y, z);
    
    Chunk *chunk = gameState->chunks[hash];

    bool found = false;

    while(chunk && !found) {
        if(chunk->x == x && chunk->y == y && chunk->z == z) {
            found = true;
            break;
        }
        chunk = chunk->next;
    }

    if((!chunk && shouldGenerateChunk)) {
        chunk = generateChunk(gameState, x, y, z, hash);
    }

    if(chunk && shouldGenerateFully && (chunk->generateState & CHUNK_NOT_GENERATED)) {
        //NOTE: Launches multi-thread work
        fillChunk(gameState, chunk);
    } 

    return chunk;
}


void getAOMaskForBlock(GameState *gameState, const float3 worldP, BlockFlags blockFlags, Block *b) {
    if(b->aoMask & (((uint64_t)(1)) << AO_BIT_CREATING)) {
        //NOTE: Generating so don't start a new generation
        // assert(false);
    } else {
        b->aoMask |= (((uint64_t)(1)) << AO_BIT_CREATING);
        b->aoMask |= (((uint64_t)(1)) << AO_BIT_INVALID);

        MemoryBarrier();
        ReadWriteBarrier();

        //NOTE: Multi-threaded version
        AoMaskData *data = (AoMaskData *)malloc(sizeof(AoMaskData));
        
        data->gameState = gameState;
        data->worldP = worldP;
        data->blockFlags = blockFlags; 
        data->b = b;

        pushWorkOntoQueue(&gameState->threadsInfo, getAOMask_multiThreaded, data);

    }
}

Chunk *generateChunk(GameState *gameState, int x, int y, int z, uint32_t hash) {
    Chunk *chunk = (Chunk *)malloc(sizeof(Chunk));
    assert(chunk);
    memset(chunk, 0, sizeof(Chunk));

    chunk->x = x;
    chunk->y = y;
    chunk->z = z;
    chunk->generateState = CHUNK_NOT_GENERATED;
    chunk->entities = 0;

    chunk->next = gameState->chunks[hash];
    gameState->chunks[hash] = chunk;

    return chunk;
}

Block *blockExistsReadOnly_withBlock(GameState *gameState, float worldx, float worldy, float worldz, BlockFlags flags) {
    int chunkX = roundChunkCoord((float)worldx * INVERSE_CHUNK_DIM_METRES);
    int chunkY = roundChunkCoord((float)worldy * INVERSE_CHUNK_DIM_METRES);
    int chunkZ = roundChunkCoord((float)worldz * INVERSE_CHUNK_DIM_METRES);

    int localx = worldPToVoxelLocalP(chunkX, worldx);
    int localy = worldPToVoxelLocalP(chunkY, worldy);
    int localz = worldPToVoxelLocalP(chunkZ, worldz);

    assert(localx < CHUNK_DIM);
    assert(localy < CHUNK_DIM);
    assert(localz < CHUNK_DIM);
    
    Chunk *c = getChunkReadOnly(gameState, chunkX, chunkY, chunkZ);
    Block *found = 0;
    if(c && c->blocks) {
        int blockIndex = getBlockIndex(localx, localy, localz);
        assert(blockIndex < BLOCKS_PER_CHUNK);
        if(blockIndex < BLOCKS_PER_CHUNK && c->blocks[blockIndex].exists && (getBlockFlags(gameState, c->blocks[blockIndex].type) & flags)) {
            found = &c->blocks[blockIndex];
        }
    } 

    return found;
}


bool blockExistsReadOnly(GameState *gameState, float worldx, float worldy, float worldz, BlockFlags flags) {
    int chunkX = roundChunkCoord((float)worldx * INVERSE_CHUNK_DIM_METRES);
    int chunkY = roundChunkCoord((float)worldy * INVERSE_CHUNK_DIM_METRES);
    int chunkZ = roundChunkCoord((float)worldz * INVERSE_CHUNK_DIM_METRES);

    int localx = worldPToVoxelLocalP(chunkX, worldx);
    int localy = worldPToVoxelLocalP(chunkY, worldy);
    int localz = worldPToVoxelLocalP(chunkZ, worldz);
    
    assert(localx < CHUNK_DIM);     
    assert(localy < CHUNK_DIM);
    assert(localz < CHUNK_DIM);
    
    Chunk *c = getChunkReadOnly(gameState, chunkX, chunkY, chunkZ);
    bool found = false;
    if(c && c->blocks) {
        int blockIndex = getBlockIndex(localx, localy, localz);
        assert(blockIndex < BLOCKS_PER_CHUNK);
        if(blockIndex < BLOCKS_PER_CHUNK && c->blocks[blockIndex].exists && (getBlockFlags(gameState, c->blocks[blockIndex].type) & flags)) {
            found = true;
        }
    } else {
        //NOTE: Find out what the block type should be since the chunk hasn't generated yet
        BlockType t = worldGeneration_shouldBlockExist(worldx, worldy, worldz);

        if(getBlockFlags(gameState, t) & flags) {
            found = true;
        }
    }

    return found;
}


void getAOMask_multiThreaded(void *data_) {
    AoMaskData *data = (AoMaskData *)data_;

    GameState *gameState = data->gameState;
    float3 worldP = data->worldP;
    BlockFlags blockFlags = data->blockFlags; 
    Block *b = data->b;

    // assert((b->aoMask & (((uint64_t)(1)) << 62))); //NOTE: It might get invalidated while it's on the queue, so we want to ignore this work
    // assert((b->aoMask & (((uint64_t)(1)) << 63)));

    uint64_t result = 0;

    if((blockFlags & BLOCK_FLAGS_AO)) {
        for(int i = 0; i < arrayCount(global_cubeData); ++i) {
            Vertex v = global_cubeData[i];

            bool blockValues[3] = {false, false, false};
            
            for(int j = 0; j < arrayCount(blockValues); j++) {
                float3 p = plus_float3(worldP, gameState->aoOffsets[i].offsets[j]);
                if(blockExistsReadOnly(gameState, p.x, p.y, p.z, BLOCK_FLAGS_AO)) {
                    blockValues[j] = true; 
                }
            }

            //NOTE: Get the ambient occulusion level
            uint64_t value = 0;
            //SPEED: Somehow make this not into if statments
            if(blockValues[0] && blockValues[2])  {
                value = 3;
            } else if((blockValues[0] && blockValues[1])) {
                assert(!blockValues[2]);
                value = 2;
            } else if((blockValues[1] && blockValues[2])) {
                assert(!blockValues[0]);
                value = 2;
            } else if(blockValues[0]) {
                assert(!blockValues[1]);
                assert(!blockValues[2]);
                value = 1;
            } else if(blockValues[1]) {
                assert(!blockValues[0]);
                assert(!blockValues[2]);
                value = 1;
            } else if(blockValues[2]) {
                assert(!blockValues[0]);
                assert(!blockValues[1]);
                value = 1;
            } 
            
            //NOTE: Times 2 because each value need 2 bits to write 0 - 3. 
            result |= (value << (uint64_t)(i*2)); //NOTE: Add the mask value
            assert(((i + 1)*2) < AO_BIT_NOT_VISIBLE); //NOTE: +1 to account for the top bit
        }
    }

    //NOTE: Calculate whether this block can be seen by the player. This is if there is a neighouring block in all caridnal directions.
    uint64_t unseen = getBitWiseFlag(AO_BIT_NOT_VISIBLE);
    for(int j = 0; j < arrayCount(gameState->cardinalOffsets) && unseen != 0; j++) {
        float3 p = plus_float3(worldP, gameState->cardinalOffsets[j]);
        if(!blockExistsReadOnly(gameState, p.x, p.y, p.z, BLOCK_FLAGS_AO)) {
            unseen = 0;
        }
    }

    result = (uint64_t)result | (uint64_t)unseen;

    MemoryBarrier();
    ReadWriteBarrier();

    b->aoMask = result;
    
    //NOTE: make sure these bits aren't set
    assert(!(b->aoMask & (((uint64_t)(1)) << AO_BIT_CREATING)));
    assert(!(b->aoMask & (((uint64_t)(1)) << AO_BIT_INVALID)));

    free(data_);
    data_ = 0;
}

void pushQuadIndicies(unsigned int **array, int vertexCount) {
    u32 a = global_quadIndices[0] + vertexCount;
    u32 b = global_quadIndices[1] + vertexCount;
    u32 c = global_quadIndices[2] + vertexCount;
    u32 d = global_quadIndices[3] + vertexCount;
    u32 e = global_quadIndices[4] + vertexCount;
    u32 f = global_quadIndices[5] + vertexCount;
    pushArrayItem(array, a, u32);
    pushArrayItem(array, b, u32);
    pushArrayItem(array, c, u32);
    pushArrayItem(array, d, u32);
    pushArrayItem(array, e, u32);
    pushArrayItem(array, f, u32);
}

#include "./multi_thread_chunk_mesh_builder.cpp"

void drawChunk(GameState *gameState, Renderer *renderer, Chunk *c) {
    if((c->generateState & CHUNK_MESH_DIRTY) || (c->generateState & CHUNK_MESH_BUILDING)) 
    {
        if(c->generateState & CHUNK_MESH_DIRTY) {
            pushCreateMeshToThreads(gameState, c);
        }
    } else if(c->modelBuffer.indexCount > 0) {
        glDrawElements(GL_TRIANGLES, c->modelBuffer.indexCount, GL_UNSIGNED_INT, 0); 
        renderCheckError();
        
    }
}

void removeEntityFromChunk(Chunk *chunk, EntityID id) {
    int entityCount = getArrayLength(chunk->entities);
    assert(entityCount > 0);
    bool found = false;

    for(int i = 0; i < entityCount && !found; ++i) {
        //NOTE: Check the string pointer is the same
        if(id.stringID == chunk->entities[i].id.stringID) {
            //NOTE: Double check the hash is the same for sanity check
            assert(chunk->entities[i].id.crc32Hash == id.crc32Hash);
            found = true;
            ResizeArrayHeader *header = getResizeArrayHeader((u8 *)chunk->entities);
            chunk->entities[i] = chunk->entities[--header->elementsCount];
        }
    }
    assert(found);

}

void addItemToInventory(GameState *gameState, Entity *e, int count) {
    InventoryItem *foundItem = 0;

    //NOTE: Check first if there is a slot already with the item
    for(int i = 0; i < gameState->inventoryCount && !foundItem; ++i) {
        InventoryItem *item = &gameState->playerInventory[i];
        int totalCount = item->count + count;
        if(item->type == e->itemType && totalCount <= ITEM_PER_SLOT_COUNT) {
            foundItem = item;
        }
    }
    
    if(!foundItem && gameState->inventoryCount < arrayCount(gameState->playerInventory)) {
        //NOTE: Has room in the inventory
        foundItem = &gameState->playerInventory[gameState->inventoryCount++];
    }

    if(foundItem) {
        foundItem->count += count;
        foundItem->type = e->itemType;
    }
}

void updateParticlers(GameState *gameState) {
    float3 cameraPos = plus_float3(gameState->cameraOffset, gameState->player.T.pos);
    
    for(int i = 0; i < gameState->particlerCount; ) {
        int addend = 1;
        

        Particler *p = &gameState->particlers[i];

        bool shouldRemove = updateParticler(gameState->renderer, p, cameraPos, gameState->dt);

        if(shouldRemove) {
            //NOTE: Move from the end
            gameState->particlers[i] = gameState->particlers[--gameState->particlerCount];
            addend = 0;
        } 

        i += addend;
    }
    
}


BlockChunkPartner blockExists(GameState *gameState, float worldx, float worldy, float worldz, BlockFlags flags) {
    BlockChunkPartner found = {};
    found.block = 0;

    int chunkX = roundChunkCoord((float)worldx * INVERSE_CHUNK_DIM_METRES);
    int chunkY = roundChunkCoord((float)worldy * INVERSE_CHUNK_DIM_METRES);
    int chunkZ = roundChunkCoord((float)worldz * INVERSE_CHUNK_DIM_METRES);

    int localx = round(worldx - (CHUNK_DIM*chunkX*VOXEL_SIZE_IN_METERS) * VOXELS_PER_METER); 
    int localy = round(worldy - (CHUNK_DIM*chunkY*VOXEL_SIZE_IN_METERS)  * VOXELS_PER_METER); 
    int localz = round(worldz - (CHUNK_DIM*chunkZ*VOXEL_SIZE_IN_METERS)  * VOXELS_PER_METER); 

    assert(localx < CHUNK_DIM);
    assert(localy < CHUNK_DIM);
    assert(localz < CHUNK_DIM);
    
    Chunk *c = getChunk(gameState, chunkX, chunkY, chunkZ);

    if(c && c->blocks) {
            found.chunk = c;
            assert(localx < CHUNK_DIM);
            assert(localy < CHUNK_DIM);
            assert(localz < CHUNK_DIM);
            int blockIndex = getBlockIndex(localx, localy, localz);
            assert(blockIndex < BLOCKS_PER_CHUNK);
            if(blockIndex < BLOCKS_PER_CHUNK && c->blocks[blockIndex].exists && getBlockFlags(gameState, c->blocks[blockIndex].type) & flags) {
                // c->blocks[blockIndex].hitBlock = true;
                found.block = &c->blocks[blockIndex];
                found.blockIndex = blockIndex;
            } else {
                // found.block = 0;
            }
    }

    return found;
}

float3 findClosestFreePosition(GameState *gameState, float3 startP, float3 defaultDir, float3 *searchOffsets, int searchOffsetCount, int priorityIndex) {
    float3 result = defaultDir;
    float closestDist = FLT_MAX;
    float3 startP_block = convertRealWorldToBlockCoords(startP);
    bool found = false;

    for(int i = 0; i < searchOffsetCount; ++i) {
        if(found && i >= priorityIndex) {
            //NOTE: Prioritise the first 4 positions
            break;
        }
        float3 offset = searchOffsets[i];

        float3 blockP = plus_float3(offset, startP_block);

        BlockChunkPartner blockPtr = blockExists(gameState, blockP.x, blockP.y, blockP.z, BLOCK_EXISTS_COLLISION);
        if(!blockPtr.block) {
            float3 dirVector = minus_float3(blockP, startP);
            float d = float3_magnitude_sqr(dirVector);

            if(d < closestDist) {
                result = dirVector;
                closestDist = d;
                found = true;
            }
        }
    }
    
    return result;
}


void updateRecoverMovement(GameState *gameState, Entity *e) {
    //NOTE: Apply drag
    e->recoverDP = scale_float3(0.95f, e->recoverDP);

    //NOTE:Integrate velocity
    e->T.pos = plus_float3(e->T.pos, scale_float3(gameState->dt, e->recoverDP));
}

void drawChunkWorld(GameState *gameState, float16 screenT, float16 cameraT, float3 lookingAxis, float16 rot) {
    // float3 worldP = convertRealWorldToBlockCoords(gameState->camera.T.pos);
    float3 worldP = make_float3(1, 1, 1);
    
    int chunkX = roundChunkCoord(worldP.x * INVERSE_CHUNK_DIM_METRES);
    int chunkY = roundChunkCoord(worldP.y * INVERSE_CHUNK_DIM_METRES);
    int chunkZ = roundChunkCoord(worldP.z * INVERSE_CHUNK_DIM_METRES);
    
    int chunkRadiusY = 4;
    int chunkRadiusXZ = 10;

    for(int z = -chunkRadiusXZ; z <= chunkRadiusXZ; ++z) {
        for(int x = -chunkRadiusXZ; x <= chunkRadiusXZ; ++x) {
            for(int y = -chunkRadiusY; y <= chunkRadiusY; ++y) {
    
                Chunk *chunk = getChunk(gameState, chunkX + x, chunkY + y, chunkZ + z);
                if(chunk) {
                    // Rect3f rect = make_rec   t3f_min_dim((chunkX + x)*CHUNK_SIZE_IN_METERS, (chunkY + y)*CHUNK_SIZE_IN_METERS, (chunkZ + z)*CHUNK_SIZE_IN_METERS, CHUNK_SIZE_IN_METERS, CHUNK_SIZE_IN_METERS, CHUNK_SIZE_IN_METERS);
                    // if(rect3fInsideViewFrustrum(rect, worldP, rot, gameState->camera.fov, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE, gameState->aspectRatio_x_over_y)) 
                    {
                        if(chunk->modelBuffer.indexCount > 0) {
                            assert(chunk->generateState == CHUNK_GENERATED);
                            prepareChunkRender(gameState->renderer, &chunk->modelBuffer, &gameState->renderer->voxelChunkShader, gameState->renderer->voxelColorPallete, screenT, cameraT, lookingAxis, gameState->renderer->underWater);
                        }

                        drawChunk(gameState, gameState->renderer, chunk);

                        if(chunk->modelBuffer.indexCount > 0) {
                            assert(chunk->generateState == CHUNK_GENERATED);
                            endChunkRender();
                        }
                    } 
                    
                } else {
                    int h = 0;
                }
            }
        }
    }

}