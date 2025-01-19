enum EntityType {
    ENTITY_NONE,
    ENTITY_PLAYER,
    ENTITY_PICKUP_ITEM,
    ENTITY_GRASS_SHORT,
    ENTITY_GRASS_LONG,
    ENTITY_FOX,
    ENTITY_HORSE,
};

enum BlockType {
    BLOCK_NONE,
    BLOCK_GRASS,
    BLOCK_SOIL,
    BLOCK_STONE,
    BLOCK_TREE_WOOD,
    BLOCK_TREE_LEAVES,
    BLOCK_WATER,
    BLOCK_GRASS_SHORT_ENTITY,
    BLOCK_GRASS_TALL_ENTITY,
    BLOCK_COAL,
    BLOCK_IRON,
    BLOCK_OUTLINE,


    //NOTHING PAST HERE
    BLOCK_TYPE_COUNT
};

struct TimeOfDayValues {
    float4 skyColorA;
    float4 skyColorB;
};

enum VoxelPhysicsFlag {
    VOXEL_NONE = 0,
    VOXEL_OCCUPIED = 1 << 0,
    VOXEL_CORNER = 1 << 1,
    VOXEL_EDGE = 1 << 2,
    VOXEL_FACE = 1 << 3,
    VOXEL_INSIDE = 1 << 4,
    VOXEL_COLLIDING = 1 << 5,
};

struct EntityID {
    size_t stringSizeInBytes; //NOTE: Not include null terminator
    char stringID[256]; 
    uint32_t crc32Hash;
};

struct Texture3d {
    int w;
    int h;
    int d;

    uint32_t handle;
};

Texture3d upload3dTexture(int width, int height, int depth, void *data);

void delete3dTexture(Texture3d *t) {
    glDeleteTextures(1, &t->handle);
}

static int global_entityIdCreated = 0;

EntityID makeEntityId(int randomStartUpID) {
    EntityID result = {};

    time_t timeSinceEpoch = time(0);

    #define ENTITY_ID_PRINTF_STRING "%ld-%d-%d", timeSinceEpoch, randomStartUpID, global_entityIdCreated

    //NOTE: Allocate the string
    size_t bufsz = snprintf(NULL, 0, ENTITY_ID_PRINTF_STRING) + 1;
    assert(bufsz < arrayCount(result.stringID));
    result.stringSizeInBytes = MathMin_sizet(arrayCount(result.stringID), bufsz);
    snprintf(result.stringID, bufsz, ENTITY_ID_PRINTF_STRING);

    result.crc32Hash = get_crc32(result.stringID, result.stringSizeInBytes);

    #undef ENTITY_ID_PRINTF_STRING

    //NOTE: This would have to be locked in threaded application
    global_entityIdCreated++;

    return result;
} 

bool areEntityIdsEqual(EntityID a, EntityID b) {
    bool result = false;
    if(a.crc32Hash == b.crc32Hash && easyString_stringsMatch_nullTerminated(a.stringID, b.stringID)) {
        result = true;
    }
    return result;
}

struct VoxelEntity {
    EntityID id;
    TransformX T;

    bool inBounds;

    //NOTE: Physics details
    float3 dP;
    float3 ddPForFrame;

    float3 dA;
    float3 ddAForFrame;//NOTE: Torque for frame

    float inverseMass;
    float invI;
    float coefficientOfRestitution;
    float friction;

    //NOTE: Voxel data
    float3 worldBounds;
    float3 furtherestVoxel; //NOTE: To calculate the max speed any voxel can have 

    float sleepTimer;
    bool asleep;

    float3 *corners;

    int occupiedCount;
    u8 *data;
    u8 *colorData;
    int stride; //x
    int pitch;//y
    int depth;//z
    Texture3d textureData;
};

#include "./physics.cpp"

struct VoxelCollideData {
    VoxelEntity *a;
    VoxelEntity *b;

    Rect3f aRect;
    Rect3f bRect;

    int pointCount;
    CollisionPoint points[MAX_CONTACT_POINTS_PER_PAIR];

    VoxelCollideData *next;
};

int getVoxelIndex(VoxelEntity *e, int x, int y, int z) {
    return (z*e->pitch*e->stride + y*e->stride + x);
}

u8 getByteFromVoxelEntity(VoxelEntity *e, int x, int y, int z) {
    return e->data[getVoxelIndex(e, x, y, z)];
}

bool isVoxelOccupied(VoxelEntity *e, int x, int y, int z, u32 flags = VOXEL_OCCUPIED) {
    bool result = false;
    if(x >= 0 && x < e->stride && y >= 0 && y < e->pitch && z >= 0 && z < e->depth) {
        result = (e->data[getVoxelIndex(e, x, y, z)] & flags);
    }
    return result;
}

#include "collision.cpp"

struct CornerPair {
    float3 a[3];
};

CornerPair makeCornerPair(float3 a, float3 b, float3 c) {
    CornerPair p = {};

    p.a[0] = a;
    p.a[1] = b;
    p.a[2] = c;

    return p;
}

struct CorneChecks {
    float3 a[7];
};

CorneChecks makeCornerCheck(float3 a, float3 b, float3 c, float3 d, float3 e, float3 f, float3 g) {
    CorneChecks p = {};

    p.a[0] = a;
    p.a[1] = b;
    p.a[2] = c;
    p.a[3] = d;
    p.a[4] = e;
    p.a[5] = f;
    p.a[6] = g;
    
    return p;
}

float getRelativeSpeed(VoxelEntity *a, VoxelEntity *b) {
     //NOTE: Get the distance of the point from center of mass 
    float3 dpPointA = plus_float3(a->dP, float3_cross(a->dA, a->furtherestVoxel));
    float3 dpPointB = plus_float3(b->dP, float3_cross(b->dA, b->furtherestVoxel));

    float result = float3_magnitude(minus_float3(dpPointB, dpPointA));

    return result;
}


void classifyPhysicsShapeAndIntertia(VoxelEntity *e, bool isInfiniteShape = false) {
    float inertia = 0;
    float massPerVoxel = (1.0f / e->inverseMass) / (float)e->occupiedCount;
    if(e->corners) {
        freeResizeArray(e->corners);
    }
    e->corners = initResizeArray(float3);

    CornerPair edgePairs[] = {
        makeCornerPair(make_float3(1, 0, 0), make_float3(0, 1, 0), make_float3(1, 1, 0)),
        makeCornerPair(make_float3(1, 0, 0), make_float3(0, -1, 0), make_float3(1, -1, 0)),
        makeCornerPair(make_float3(-1, 0, 0), make_float3(0, 1, 0), make_float3(-1, 1, 0)),
        makeCornerPair(make_float3(-1, 0, 0), make_float3(0, -1, 0), make_float3(-1, -1, 0)),

        makeCornerPair(make_float3(0, 1, 0), make_float3(0, 0, 1), make_float3(0, 1, 1)),
        makeCornerPair(make_float3(0, -1, 0), make_float3(0, 0, 1), make_float3(0, -1, 1)),
        makeCornerPair(make_float3(0, 1, 0), make_float3(0, 0, -1), make_float3(0, 1, -1)),
        makeCornerPair(make_float3(0, -1, 0), make_float3(0, 0, -1), make_float3(0, -1, -1)),

        makeCornerPair(make_float3(1, 0, 0), make_float3(0, 0, 1), make_float3(1, 0, 1)),
        makeCornerPair(make_float3(-1, 0, 0), make_float3(0, 0, 1), make_float3(-1, 0, 1)),
        makeCornerPair(make_float3(1, 0, 0), make_float3(0, 0, -1), make_float3(1, 0, -1)),
        makeCornerPair(make_float3(-1, 0, 0), make_float3(0, 0, -1), make_float3(-1, 0, -1)),
    };
    
    CorneChecks cornerChecks[8] = {
        
    };  

    float3 offsets[8] = {
        make_float3(1, 1, 1), 
        make_float3(1, -1, 1),
        make_float3(-1, 1, 1),
        make_float3(-1, -1, 1),
        make_float3(1, 1, -1), 
        make_float3(1, -1, -1),
        make_float3(-1, 1, -1),
        make_float3(-1, -1, -1),
        };

    for(int i = 0; i < arrayCount(cornerChecks); i++) {
        float3 o = offsets[i];

        cornerChecks[i] = makeCornerCheck(make_float3(0,  o.y,  o.z), 
        make_float3(o.x,  0,  o.z),  
        make_float3(o.x,  o.y,  o.z), 

        make_float3(0,  o.y,  0), 
        make_float3(o.x,  0,  0),  
        make_float3(o.x,  o.y,  0), 

        make_float3(0, 0, o.z));
    }

    float maxIntertia = 0;

    for(int z = 0; z < e->depth; z++) {
        for(int y = 0; y < e->pitch; y++) {
            for(int x = 0; x < e->stride; x++) {
                u8 flags = e->data[getVoxelIndex(e, x, y, z)];

                if(flags & VOXEL_OCCUPIED) {
                    //NOTE: Clear flags
                    flags &= ~(VOXEL_CORNER | VOXEL_EDGE | VOXEL_FACE | VOXEL_INSIDE);
                    
                    float3 p = make_float3(x, y, z);
                    float3 modelP = getVoxelPositionInModelSpaceFromCenter(e, p);
                    
                    float perInertia = massPerVoxel*(modelP.x*modelP.x + modelP.y*modelP.y + modelP.z*modelP.z);
                    inertia += perInertia;

                    if(maxIntertia < perInertia) {
                        e->furtherestVoxel = modelP;
                    }
                    
                    bool found = false;

                    if(!isInfiniteShape) {

                        //NOTE: Check if CORNER
                        for(int i = 0; i < arrayCount(cornerChecks) && !found; i++) {
                            CorneChecks p = cornerChecks[i];
                            bool empty = true;
                            for(int j = 0; j < arrayCount(p.a) && empty; j++) {
                                float3 dir = p.a[j];
                                int testX = (int)(x + dir.x);
                                int testY = (int)(y + dir.y);
                                int testZ = (int)(z + dir.z);

                                if(isVoxelOccupied(e, testX, testY, testZ)) {
                                    empty = false;
                                    break;
                                }
                            }

                            if(empty) {
                                float3 a = make_float3(x, y, z);
                                pushArrayItem(&e->corners, a, float3);
                                found = true;
                                flags |= VOXEL_CORNER;
                                break;
                            } else {
                                int h = 0;
                            }
                        }

                        //NOTE: Check if EDGE
                        for(int i = 0; i < arrayCount(edgePairs) && !found; i++) {
                            CornerPair p = edgePairs[i];
                            bool empty = true;
                            for(int j = 0; j < arrayCount(p.a) && empty; j++) {
                                float3 dir = p.a[j];
                                int testX = (int)(x + dir.x);
                                int testY = (int)(y + dir.y);
                                int testZ = (int)(z + dir.z);

                                if(isVoxelOccupied(e, testX, testY, testZ)) {
                                    empty = false;
                                    break;
                                }
                            }

                            if(empty) {
                                float3 a = make_float3(x, y, z);
                                pushArrayItem(&e->corners, a, float3);
                                found = true;
                                flags |= VOXEL_EDGE;
                                break;
                            } else {
                                int h = 0;
                            }
                        }
                    }
                    
                    //NOTE: Check if it's a face
                    if(!found) {
                        float3 offsets[6] = {
                            make_float3(1, 0, 0), 
                            make_float3(-1, 0, 0),
                            make_float3(0, 1, 0), 
                            make_float3(0, -1, 0),
                            make_float3(0, 0, 1), 
                            make_float3(0, 0, -1),

                        };

                        for(int j = 0; j < arrayCount(offsets) && !found; j++) {
                            float3 dir = offsets[j];
                            int testX = (int)(x + dir.x);
                            int testY = (int)(y + dir.y);
                            int testZ = (int)(z + dir.z);

                            if(!isVoxelOccupied(e, testX, testY, testZ)) {
                                found = true;
                                flags |= VOXEL_FACE;
                                break;
                            }
                        }

                    }

                    if(!found) {
                        flags |= VOXEL_INSIDE;
                    }

                    e->data[getVoxelIndex(e, x, y, z)] = flags;
                }
            }
        }
    }

    if(e->inverseMass != 0) {
        e->invI = 1.0f / inertia;
    } else {
        e->invI = 0;
    }

    //NOTE: Upload the 3d texture data to the GPU
    if(e->textureData.handle > 0) {
        delete3dTexture(&e->textureData);
    }   
    e->textureData = upload3dTexture(e->stride, e->pitch, e->depth, e->data);
    
}

void initBaseVoxelEntity(VoxelEntity *e, int randomStartUpID) {
    e->id = makeEntityId(randomStartUpID);
    e->T.rotation = identityQuaternion();
    e->T.pos = make_float3(0, 0, 0);
    e->T.scale = make_float3(0, 0, 0);
}

VoxelEntity createVoxelCircleEntity(float radius, float3 pos, float inverseMass, int randomStartUpID) {
    VoxelEntity result = {};

    initBaseVoxelEntity(&result, randomStartUpID);
    result.sleepTimer = 0;
    result.asleep = false;
    
    result.friction = 0.2f;
    result.T.pos = pos;
    
    result.inverseMass = inverseMass;
    result.coefficientOfRestitution = 0.2f;

    float diameter = 2*radius;
    result.worldBounds = make_float3(diameter, diameter, diameter);

    result.T.scale = make_float3(diameter, diameter, diameter);
    // result.T.rotation.z = 0.25f*PI32;
    
    int diameterInVoxels = round(diameter*VOXELS_PER_METER);
    int t = (int)(diameterInVoxels*diameterInVoxels*diameterInVoxels);
    result.data = (u8 *)easyPlatform_allocateMemory(sizeof(u8)*t, EASY_PLATFORM_MEMORY_ZERO);
    result.colorData = (u8 *)easyPlatform_allocateMemory(sizeof(u8)*t, EASY_PLATFORM_MEMORY_ZERO);

    result.stride = diameterInVoxels;
    result.pitch = diameterInVoxels;
    result.depth = diameterInVoxels;

    result.occupiedCount = 0;

    float3 center = make_float3(radius, radius, radius);
    for(int z = 0; z < result.depth; z++) {
        for(int y = 0; y < result.pitch; y++) {
            for(int x = 0; x < result.stride; x++) {
                float3 pos = getVoxelPositionInModelSpace(make_float3(x, y, z));

                float3 diff = minus_float3(pos, center);

                u8 flags = VOXEL_NONE;

                if(float3_magnitude(diff) <= radius) {
                    flags |= VOXEL_OCCUPIED;
                    result.occupiedCount++;
                } 

                result.data[getVoxelIndex(&result, x, y, z)] = flags;
            }
        }
    }

    classifyPhysicsShapeAndIntertia(&result);

    return result;
}

VoxelEntity createVoxelPlaneEntity(float length, float3 pos, float inverseMass, int randomStartUpID) {
    VoxelEntity result = {};
    initBaseVoxelEntity(&result, randomStartUpID);

    result.T.pos = pos;
    result.inverseMass = inverseMass;
    result.coefficientOfRestitution = 0;
    result.friction = 0.2f;

    result.sleepTimer = 0;
    result.asleep = false;

    result.worldBounds = make_float3(length, VOXEL_SIZE_IN_METERS, length);

    result.T.scale = make_float3(result.worldBounds.x, result.worldBounds.y, result.worldBounds.z);

    result.stride = result.worldBounds.x*VOXELS_PER_METER;
    result.pitch = result.worldBounds.y*VOXELS_PER_METER;
    result.depth = result.worldBounds.z*VOXELS_PER_METER;

    int areaInVoxels = result.stride*result.pitch*result.depth;
    
    result.data = (u8 *)easyPlatform_allocateMemory(sizeof(u8)*areaInVoxels, EASY_PLATFORM_MEMORY_ZERO);
    result.colorData = (u8 *)easyPlatform_allocateMemory(sizeof(u8)*areaInVoxels, EASY_PLATFORM_MEMORY_ZERO);
    result.occupiedCount = 0;

    for(int z = 0; z < result.depth; z++) {
        for(int y = 0; y < result.pitch; y++) {
            for(int x = 0; x < result.stride; x++) {
                result.data[getVoxelIndex(&result, x, y, z)] = VOXEL_OCCUPIED;
                result.occupiedCount++;
            }
        }
    }

    classifyPhysicsShapeAndIntertia(&result, true);

    return result;
}

VoxelEntity createVoxelSquareEntity(float w, float h, float d, float3 pos, float inverseMass, int randomStartUpID) {
    VoxelEntity result = {};

    initBaseVoxelEntity(&result, randomStartUpID);
     
    result.T.pos = pos;
    result.inverseMass = inverseMass;
    result.coefficientOfRestitution = 0.2f;
    result.friction = 0.2f;

    result.sleepTimer = 0;
    result.asleep = false;
    // result.T.rotation.z = 0.25f*PI32;

    result.worldBounds = make_float3(w, h, d);
    result.T.scale = make_float3(result.worldBounds.x, result.worldBounds.y, result.worldBounds.z);

    result.stride = result.worldBounds.x*VOXELS_PER_METER;
    result.pitch = result.worldBounds.y*VOXELS_PER_METER;
    result.depth = result.worldBounds.z*VOXELS_PER_METER;

    int areaInVoxels = result.stride*result.pitch*result.depth;

    result.occupiedCount = 0;
    result.data = (u8 *)easyPlatform_allocateMemory(sizeof(u8)*areaInVoxels, EASY_PLATFORM_MEMORY_ZERO);
    result.colorData = (u8 *)easyPlatform_allocateMemory(sizeof(u8)*areaInVoxels, EASY_PLATFORM_MEMORY_ZERO);

    for(int z = 0; z < result.depth; z++) {
        for(int y = 0; y < result.pitch; y++) {
            for(int x = 0; x < result.stride; x++) {
                result.data[getVoxelIndex(&result, x, y, z)] = VOXEL_OCCUPIED;
                result.occupiedCount++;
            }
        }
    }

    classifyPhysicsShapeAndIntertia(&result);

    return result;
}

struct Block {
    u8 type;

    //NOTE: Local to the Chunk they're in
    int x;
    int y;
    int z;
    volatile uint64_t aoMask; //NOTE: Multiple threads can change this
    //NOTE: Instead of storing this 
    bool exists;

    float timeLeft;
};

enum EntityFlags {
    SHOULD_ROTATE = 1 << 0,
    ENTITY_DESTRUCTIBLE = 1 << 1,
    ENTITY_DELETED = 1 << 2,
};

struct Entity {
    EntityID id;

    float3 offset;
    float floatTime;

    float stamina;
    
    TransformX T;
    float3 dP;
    float3 recoverDP; //NOTE: This is a seperate dP that gets applied no matter what i.e. it doesn't get stopped by collisions. It's so if an entity gets stuck in a block it can move out of it.
    EntityType type;

    Rect3f collisionBox;
    bool grounded;
    bool tryJump;
    bool running;

    AnimationState animationState;

    uint32_t flags;
    BlockType itemType;
};

#define CHUNK_DIM 16
#define BLOCKS_PER_CHUNK CHUNK_DIM*CHUNK_DIM*CHUNK_DIM

enum ChunkGenerationState {
    CHUNK_NOT_GENERATED = 1 << 0, 
    CHUNK_GENERATING = 1 << 1, 
    CHUNK_GENERATED = 1 << 2, 
    CHUNK_MESH_DIRTY = 1 << 3, 
    CHUNK_MESH_BUILDING = 1 << 4, 
};

struct Chunk {
    int x;
    int y;
    int z;

    volatile int64_t generateState; //NOTE: Chunk might not be generated, so check first when you get one

    //NOTE: 16 x 16 x 16
    //NOTE: Z Y X
    Block *blocks; //NOTE: Could be null

    Entity *entities;

    ChunkModelBuffer modelBuffer;
    ChunkModelBuffer alphaModelBuffer;

    Chunk *next;
};

struct ChunkInfo {
    Chunk *chunk;

    ChunkInfo *next;
};

struct BlockChunkPartner {
    Block *block;
    Chunk *chunk;

    int blockIndex;

    float3 sideNormal;
};

struct Camera {
    TransformX T;
    float fov;
    float targetFov;
    float shakeTimer;
    float runShakeTimer;
    bool followingPlayer;
};

void initBaseEntity(Entity *e, int randomStartUpID) {
    e->id = makeEntityId(randomStartUpID);
    e->T.rotation = identityQuaternion();
    e->recoverDP = e->dP = make_float3(0, 0, 0);
    
}

Entity *initPlayer(Entity *e, int randomStartUpID) {
    initBaseEntity(e, randomStartUpID);
    e->T.pos = make_float3(0, 0, 0);
    float playerWidth = 0.7f;
    e->T.scale = make_float3(playerWidth, 1.7f, playerWidth);
    // e->T.scale = make_float3(1, 1, 1);
    
    e->type = ENTITY_PLAYER;
    e->offset = make_float3(0, 0, 0);
    e->grounded = false;
    e->flags = 0;
    e->stamina = 1;
    return e;
}