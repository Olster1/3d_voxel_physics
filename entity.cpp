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
    BLOCK_CLOUD,
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

    float sleepTimer;
    bool asleep;

    float3 *corners;
    float3 *edges;

    int occupiedCount;
    u8 *data;
    int stride; //x
    int pitch;//y
    int depth;//z
};

#include "./physics.cpp"

struct VoxelCollideData {
    VoxelEntity *a;
    VoxelEntity *b;

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

float3 getVoxelPositionInModelSpace(float3 pos) {
    pos.x *= VOXEL_SIZE_IN_METERS;
    pos.y *= VOXEL_SIZE_IN_METERS;
    pos.z *= VOXEL_SIZE_IN_METERS;

    //NOTE: To account for a voxel posiiton being in the center of a voxel
    pos.x += 0.5f*VOXEL_SIZE_IN_METERS;
    pos.y += 0.5f*VOXEL_SIZE_IN_METERS;
    pos.z += 0.5f*VOXEL_SIZE_IN_METERS;
    return pos;
}


float3 getVoxelPositionInModelSpaceFromCenter(VoxelEntity *e, float3 pos) {
    pos = getVoxelPositionInModelSpace(pos);
    float3 center = make_float3(0.5f*e->worldBounds.x, 0.5f*e->worldBounds.y, 0.5f*e->worldBounds.z);
    pos = minus_float3(pos, center);

    return pos;
}

struct FloodFillVoxel {
    int x; 
    int y;
    FloodFillVoxel *next;
    FloodFillVoxel *prev;
};

void pushOnVoxelQueue(VoxelEntity *e, FloodFillVoxel *queue, bool *visited, int x, int y) {

	if(!visited[e->stride*y + x]) {
		FloodFillVoxel *node = pushStruct(&globalPerFrameArena, FloodFillVoxel);
		node->x = x;
        node->y = y;

		queue->next->prev = node;
		node->next = queue->next;

		queue->next = node;
		node->prev = queue;

		//push node to say you visited it
        visited[e->stride*y + x] = true;
		
	}
		
}

FloodFillVoxel *popOffVoxelQueue(FloodFillVoxel *queue) {
	FloodFillVoxel *result = 0;

	if(queue->prev != queue) { //something on the queue
		result = queue->prev;

		queue->prev = result->prev;
		queue->prev->next = queue;
	} 

	return result;
}

float3 voxelToWorldP(VoxelEntity *e, int x, int y, int z) {
    float3 modelSpace = getVoxelPositionInModelSpaceFromCenter(e, make_float3(x, y, z));
    float3 p = float16_transform(getModelToViewSpaceWithoutScaleSQT(e->T), make_float4(modelSpace.x, modelSpace.y, modelSpace.z, 1)).xyz; 
    return p;
}

float3 worldPToVoxelP(VoxelEntity *e, float3 worldP) {
    float16 T = transform_getInverseX_SQT(e->T);
    float3 p = float16_transform(T, make_float4(worldP.x, worldP.y, worldP.z, 1)).xyz;
    p = plus_float3(p, make_float3(0.5f*e->worldBounds.x, 0.5f*e->worldBounds.y, 0.5f*e->worldBounds.z));

    //NOTE: convert to voxel space
    p.x *= VOXELS_PER_METER;
    p.y *= VOXELS_PER_METER;
    p.z *= VOXELS_PER_METER;

    p.x = (int)p.x;
    p.y = (int)p.y;
    p.z = (int)p.z;

    return p;
}


// float findSeperationForShape(VoxelEntity *e, int startX, int startY, float2 startWorldP, float2 unitVector) {
//     float x = startX;
//     float y = startY;

//     while(isVoxelOccupied(e, (int)x, (int)y, (int)z)) {
//         x += unitVector.x;
//         y += unitVector.y;
//     }

//     float2 v = minus_float2(voxelToWorldP(e, x, y), startWorldP);

//     return float2_magnitude(v) * signOf(float2_dot(v, unitVector));
// }

// float2 getSeperationViaFloodFill(VoxelEntity *e, int startX, int startY, float2 startWorldP) {
//     float2 result = {};
//     perFrameArenaMark = takeMemoryMark(&globalPerFrameArena);

//     bool *visited = pushArray(&globalPerFrameArena, e->stride*e->pitch, bool);
//     easyMemory_zeroSize(visited, sizeof(bool)*e->stride*e->pitch);

//     FloodFillVoxel queue = {};
//     queue.next = queue.prev = &queue; 
// 	pushOnVoxelQueue(e, &queue, visited, startX, startY);

// 	bool searching = true;
// 	while(searching) {	
// 		FloodFillVoxel *node = popOffVoxelQueue(&queue);
// 		if(node) {
// 			int x = node->x;
// 			int y = node->y;

// 			if(!isVoxelOccupied(e, x, y)) {
//                 result = minus_float2(voxelToWorldP(e, x, y), startWorldP);
// 				searching = false;
// 				break;
// 			} else {
// 				//push on more directions   
// 				pushOnVoxelQueue(e, &queue, visited, x + 1, y);
// 				pushOnVoxelQueue(e, &queue, visited, x - 1, y);
// 				pushOnVoxelQueue(e, &queue, visited, x, y + 1);
// 				pushOnVoxelQueue(e, &queue, visited, x, y - 1);

// 				//Diagonal movements
//                 pushOnVoxelQueue(e, &queue, visited, x + 1, y + 1);
// 				pushOnVoxelQueue(e, &queue, visited, x + 1, y - 1);
// 				pushOnVoxelQueue(e, &queue, visited, x - 1, y - 1);
// 				pushOnVoxelQueue(e, &queue, visited, x - 1, y + 1);
// 			}
// 		} else {
// 			searching = false;
// 			break;
// 		}
// 	}

//     releaseMemoryMark(&perFrameArenaMark);
//     return result;
// }


float getFractionalPart(float number) {
    // Use truncf to get the integer part of the float
    float integerPart = truncf(number);
    return number - integerPart;
}

int doesVoxelCollide(float3 worldP, VoxelEntity *e, int idX, int idY, int idZ, bool swap, CollisionPoint *points, VoxelEntity *otherE) {
    PROFILE_FUNC(doesVoxelCollide);
    float3 p = worldPToVoxelP(e, worldP);

    int x = (int)(p.x);
    int y = (int)(p.y);
    int z = (int)(p.z);

    //TODO: This needs to be an array of 8 not 27
    // float3 voxels[27] = {
    //     make_float3(-1, -1, -1), 
    //     make_float3(-1, -1,  0), 
    //     make_float3(-1, -1,  1), 
    //     make_float3(-1,  0, -1), 
    //     make_float3(-1,  0,  0), 
    //     make_float3(-1,  0,  1), 
    //     make_float3(-1,  1, -1), 
    //     make_float3(-1,  1,  0), 
    //     make_float3(-1,  1,  1), 
    //     make_float3( 0, -1, -1), 
    //     make_float3( 0, -1,  0), 
    //     make_float3( 0, -1,  1), 
    //     make_float3( 0,  0, -1), 
    //     make_float3( 0,  0,  0),
    //     make_float3( 0,  0,  1), 
    //     make_float3( 0,  1, -1), 
    //     make_float3( 0,  1,  0), 
    //     make_float3( 0,  1,  1), 
    //     make_float3( 1, -1, -1), 
    //     make_float3( 1, -1,  0), 
    //     make_float3( 1, -1,  1), 
    //     make_float3( 1,  0, -1), 
    //     make_float3( 1,  0,  0), 
    //     make_float3( 1,  0,  1), 
    //     make_float3( 1,  1, -1), 
    //     make_float3( 1,  1,  0), 
    //     make_float3( 1,  1,  1)
    // };  

    float xOffset = getFractionalPart(p.x) > 0.5f ? 1 : -1;
    float yOffset = getFractionalPart(p.y) > 0.5f ? 1 : -1;
    float zOffset = getFractionalPart(p.z) > 0.5f ? 1 : -1;

    float3 voxels[8] = {
        make_float3(0,  yOffset,  zOffset), 
        make_float3(xOffset,  0,  zOffset),  
        make_float3(xOffset,  yOffset,  zOffset), 

        make_float3(0,  yOffset,  0), 
        make_float3(xOffset,  0,  0),  
        make_float3(xOffset,  yOffset,  0), 

        make_float3(0, 0, 0), 
        make_float3(0, 0, zOffset), 
    };  

    int pointCount = 0;

    for(int i = 0; i < arrayCount(voxels); ++i) {
        float3 voxelSpace = plus_float3(make_float3(x, y, z), voxels[i]);

        int testX = (int)voxelSpace.x;
        int testY = (int)voxelSpace.y;
        int testZ = (int)voxelSpace.z;

        if(isVoxelOccupied(e, testX, testY, testZ)) {
            float3 voxelWorldP;
            {
                PROFILE_FUNC(voxelToWorldP);
                voxelWorldP = voxelToWorldP(e, testX, testY, testZ);
            }

            float3 diff = minus_float3(worldP, voxelWorldP);

            if(swap) {
                diff = minus_float3(voxelWorldP, worldP);
            }

            float distanceSqr = float3_dot(diff, diff);

            if(distanceSqr <= VOXEL_SIZE_IN_METERS_SQR) {
                CollisionPoint result = {};

                result.entityId = e->id;
                result.x = idX;
                result.y = idY;
                result.z = idZ;
                result.x1 = testX;
                result.y1 = testY;
                e->data[testZ*e->stride*e->pitch + testY*e->stride + testX] |= VOXEL_COLLIDING;
                result.point = lerp_float3(worldP, voxelWorldP, 0.5f);
                
                result.normal = normalize_float3(diff);//TODO: I think this is causing an issue since the normal value shouldn't 
                result.seperation = sqrt(distanceSqr) - VOXEL_SIZE_IN_METERS; //NOTE: Just resolves the individual voxel
                // result.seperation = findSeperationForShape(e, testX, testY, result.point, result.normal); //NOTE: Just resolves the individual voxel
                result.Pn = 0;
                result.inverseMassNormal = 0;
                result.velocityBias = 0;

                // assert(pointCount < MAX_CONTACT_POINTS_PER_PAIR);
                if(pointCount < MAX_CONTACT_POINTS_PER_PAIR) {
                    points[pointCount++] = result;
                }
            }
        }
    }

    return pointCount;
}

float3 modelSpaceToWorldSpace(VoxelEntity *e, float x, float y, float z) {
    float16 T = quaternionToMatrix(e->T.rotation);
    float3 p = plus_float3(float16_transform(T, make_float4(x, y, z, 1)).xyz, e->T.pos); 
    return plus_float3(make_float3(x, y, z), e->T.pos);
    // return make_float3(x, y, z);
}

bool boundingBoxOverlapWithMargin(VoxelEntity *a, VoxelEntity *b, float margin) {
    const Rect3f aH = make_rect3f_center_dim(make_float3(0, 0, 0), plus_float3(make_float3(margin, margin, margin), a->worldBounds));
    const Rect3f bH = make_rect3f_center_dim(make_float3(0, 0, 0), plus_float3(make_float3(margin, margin, margin), b->worldBounds));

    assert(a != b);

    float3 pointsA[8] = { 
        modelSpaceToWorldSpace(a, aH.minX, aH.minY, aH.minZ), 
        modelSpaceToWorldSpace(a, aH.minX, aH.maxY, aH.minZ), 
        modelSpaceToWorldSpace(a, aH.maxX, aH.maxY, aH.minZ),  
        modelSpaceToWorldSpace(a, aH.maxX, aH.minY, aH.minZ), 

        modelSpaceToWorldSpace(a, aH.minX, aH.minY, aH.maxZ), 
        modelSpaceToWorldSpace(a, aH.minX, aH.maxY, aH.maxZ), 
        modelSpaceToWorldSpace(a, aH.maxX, aH.maxY, aH.maxZ),  
        modelSpaceToWorldSpace(a, aH.maxX, aH.minY, aH.maxZ), 
    };

    Rect3f aCheck = make_rect3f(FLT_MAX, FLT_MAX, FLT_MAX, -FLT_MAX,-FLT_MAX, -FLT_MAX);
    Rect3f bCheck = aCheck;
    
    for(int i = 0; i < arrayCount(pointsA); ++i) {
        float3 p = pointsA[i];
        if(p.x <= aCheck.minX) {
            aCheck.minX = p.x;
        }
        if(p.y <= aCheck.minY) {
            aCheck.minY = p.y;
        }
        if(p.z <= aCheck.minZ) {
            aCheck.minZ = p.z;
        }

        if(p.x >= aCheck.maxX) {
            aCheck.maxX = p.x;
        }
        if(p.y >= aCheck.maxY) {
            aCheck.maxY = p.y;
        }
        if(p.z >= aCheck.maxZ) {
            aCheck.maxZ = p.z;
        }
    }

    float3 pointsB[8] = { 
        modelSpaceToWorldSpace(b, bH.minX, bH.minY, bH.minZ), 
        modelSpaceToWorldSpace(b, bH.minX, bH.maxY, bH.minZ), 
        modelSpaceToWorldSpace(b, bH.maxX, bH.maxY, bH.minZ),  
        modelSpaceToWorldSpace(b, bH.maxX, bH.minY, bH.minZ), 

        modelSpaceToWorldSpace(b, bH.minX, bH.minY, bH.maxZ), 
        modelSpaceToWorldSpace(b, bH.minX, bH.maxY, bH.maxZ), 
        modelSpaceToWorldSpace(b, bH.maxX, bH.maxY, bH.maxZ),  
        modelSpaceToWorldSpace(b, bH.maxX, bH.minY, bH.maxZ), 
    };

    for(int i = 0; i < arrayCount(pointsB); ++i) {
        float3 p = pointsB[i];
        if(p.x <= bCheck.minX) {
            bCheck.minX = p.x;
        }
        if(p.y <= bCheck.minY) {
            bCheck.minY = p.y;
        }
        if(p.z <= bCheck.minZ) {
            bCheck.minZ = p.z;
        }

        if(p.x >= bCheck.maxX) {
            bCheck.maxX = p.x;
        }
        if(p.y >= bCheck.maxY) {
            bCheck.maxY = p.y;
        }
        if(p.z >= bCheck.maxZ) {
            bCheck.maxZ = p.z;
        }
    }

    bool result = false;
    
    if (aCheck.maxX < bCheck.minX || bCheck.maxX < aCheck.minX) {
        // Check for overlap on the X-axis
    } else if (aCheck.maxY < bCheck.minY || bCheck.maxY < aCheck.minY) {
        // Check for overlap on the Y-axis
    } else if (aCheck.maxZ < bCheck.minZ || bCheck.maxZ < aCheck.minZ) {
        // Check for overlap on the Z-axis
    } else {
        result = true;
    }

    return result;
}

void collideVoxelEntities(void *data_) {
    PROFILE_FUNC(collideVoxelEntities);

    VoxelCollideData *collideData = (VoxelCollideData *)data_;

    VoxelEntity *a = collideData->a;
    VoxelEntity *b = collideData->b;

    
    collideData->pointCount = 0;

    
    {
        a->inBounds = true;
        b->inBounds = true;

        //NOTE: Keep the order consistent with the order in the arbiter
        if(a > b) {
            VoxelEntity *temp = b;
            b = a;
            a = temp;
        }
        
        //NOTE: Check corners with corners & edges first
        for(int i = 0; i < getArrayLength(a->corners); i++) {
            float3 corner = a->corners[i];
            int x = corner.x;
            int y = corner.y;
            int z = corner.z;
            u8 byte = getByteFromVoxelEntity(a, x, y, z);
            
            assert(byte & VOXEL_CORNER || byte & VOXEL_EDGE);
            CollisionPoint pointsFound[MAX_CONTACT_POINTS_PER_PAIR];
            int numPointsFound = doesVoxelCollide(voxelToWorldP(a, x, y, z), b, x, y, z, true, pointsFound, a);

            if(numPointsFound > 0) {
                //NOTE: Found a point
                a->data[getVoxelIndex(a, x, y, z)] |= VOXEL_COLLIDING;

                for(int j = 0; j < numPointsFound; ++j) {
                    // assert(pointCount < arrayCount(points));
                    if(collideData->pointCount < arrayCount(collideData->points)) {
                        collideData->points[collideData->pointCount++] = pointsFound[j];
                    }
                }
            }
        }

        for(int i = 0; i < getArrayLength(b->corners); i++) {
            float3 corner = b->corners[i];
            int x = corner.x;
            int y = corner.y;
            int z = corner.z;
            u8 byte = getByteFromVoxelEntity(b, x, y, z);
            
            assert(byte & VOXEL_CORNER || byte & VOXEL_EDGE);
            CollisionPoint pointsFound[MAX_CONTACT_POINTS_PER_PAIR];
            int numPointsFound = doesVoxelCollide(voxelToWorldP(b, x, y, z), a, x, y, z, false, pointsFound, b);

            if(numPointsFound > 0) {
                //NOTE: Found a point
                b->data[getVoxelIndex(b, x, y, z)] |= VOXEL_COLLIDING;

                for(int j = 0; j < numPointsFound; ++j) {
                    // assert(pointCount < arrayCount(points));
                    if(collideData->pointCount < arrayCount(collideData->points)) {
                        collideData->points[collideData->pointCount++] = pointsFound[j];
                    }
                }
            }
        }
       
    } 
}

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


void classifyPhysicsShapeAndIntertia(VoxelEntity *e, bool isInfiniteShape = false) {
    float inertia = 0;
    float massPerVoxel = (1.0f / e->inverseMass) / (float)e->occupiedCount;
    if(e->corners) {
        freeResizeArray(e->corners);
    }
    e->corners = initResizeArray(float3);
    e->edges = initResizeArray(float3);

    CornerPair pairs[] = {
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
                    

    for(int z = 0; z < e->depth; z++) {
        for(int y = 0; y < e->pitch; y++) {
            for(int x = 0; x < e->stride; x++) {
                u8 flags = e->data[getVoxelIndex(e, x, y, z)];

                if(flags & VOXEL_OCCUPIED) {
                    //NOTE: Clear flags
                    flags &= ~(VOXEL_CORNER | VOXEL_EDGE | VOXEL_FACE | VOXEL_INSIDE);

                    float3 modelP = getVoxelPositionInModelSpaceFromCenter(e, make_float3(x, y, z));
                    inertia += massPerVoxel*(modelP.x*modelP.x + modelP.y*modelP.y + modelP.z*modelP.z);
                    
                    bool found = false;

                    //NOTE: Check if corner
                    
                    for(int i = 0; i < arrayCount(pairs) && !found; i++) {
                        CornerPair p = pairs[i];
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

                        if(empty && !isInfiniteShape) {
                            float3 a = make_float3(x, y, z);
                            pushArrayItem(&e->corners, a, float3);
                            found = true;
                            flags |= VOXEL_EDGE;
                            break;
                        } else {
                            int h = 0;
                        }
                    }

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

struct CloudBlock {
    //NOTE: Local to the Chunk they're in
    int x;
    int z;
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

struct CloudChunk {
    int x;
    int z;

    int cloudCount;
    CloudBlock clouds[CHUNK_DIM*CHUNK_DIM];

    CloudChunk *next;
};


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