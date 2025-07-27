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

float getFractionalPart(float number) {
    // Use truncf to get the integer part of the float
    float integerPart = truncf(number);
    return number - integerPart;
}

int doesVoxelCollide(float3 worldP, VoxelEntity *e, int idX, int idY, int idZ, bool swap, CollisionPoint *points, VoxelPhysicsFlag flags) {
    float3 p = worldPToVoxelP(e, worldP);

    int x = (int)(p.x);
    int y = (int)(p.y);
    int z = (int)(p.z);

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

        if(isVoxelOccupied(e, testX, testY, testZ, flags)) {
            float3 voxelWorldP;
            {
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
                result.z1 = testZ;
                e->data[getVoxelIndex(e, testX, testY, testZ)] |= VOXEL_COLLIDING;
                result.point = lerp_float3(worldP, voxelWorldP, 0.5f);
                
                result.normal = normalize_float3(diff);//TODO: I think this is causing an issue since the normal value shouldn't 
                result.seperation = sqrt(distanceSqr) - VOXEL_SIZE_IN_METERS; //NOTE: Just resolves the individual voxel
                result.Pn = 0;
                result.velocityBias = 0;

                assert(pointCount < MAX_CONTACT_POINTS_PER_PAIR);
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

bool boundingBoxOverlapWithMargin(VoxelEntity *a, VoxelEntity *b, Rect3f *aRect, Rect3f *bRect, float margin) {
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

    *aRect = aCheck;
    *bRect = bCheck;

    return result;
}

int checkCornerAndEdgeVoxels(VoxelCollideData *collideData, VoxelEntity *a, VoxelEntity *b, Rect3f rect, bool flip) {
    int totalPointsFound = 0;
    //NOTE: Check corners with corners & edges first
    for(int i = 0; i < getArrayLength(a->corners); i++) {
        float3 corner = a->corners[i];
        int x = corner.x;
        int y = corner.y;
        int z = corner.z;
        u8 byte = getByteFromVoxelEntity(a, x, y, z);

        float3 voxelP = voxelToWorldP(a, x, y, z);
        VoxelPhysicsFlag flagToTest = VOXEL_OCCUPIED; //(VoxelPhysicsFlag)(VOXEL_CORNER | VOXEL_EDGE | VOXEL_FACE); 
        
        if(in_rect3f_bounds(rect, voxelP)) 
        {
            assert(((byte & VOXEL_CORNER) && !(byte & VOXEL_EDGE)) || ((byte & VOXEL_EDGE) && !(byte & VOXEL_CORNER)));
            if(byte & VOXEL_EDGE) {
                assert(!(byte & VOXEL_CORNER));
                //NOTE: This is an optimisation that means only corners test everthing and edges only ever have to test against other edges
                #if TEST_EDGES_EDGES
                flagToTest = VOXEL_EDGE;
                #endif
            }
            CollisionPoint pointsFound[MAX_CONTACT_POINTS_PER_PAIR];
            int numPointsFound = doesVoxelCollide(voxelP, b, x, y, z, flip, pointsFound, flagToTest);

            if(numPointsFound > 0) {
                //NOTE: Found a point
                a->data[getVoxelIndex(a, x, y, z)] |= VOXEL_COLLIDING;

                for(int j = 0; j < numPointsFound; ++j) {
                    // assert(pointCount < arrayCount(points));
                    if(collideData->pointCount < arrayCount(collideData->points)) {
                        collideData->points[collideData->pointCount++] = pointsFound[j];
                    }
                }
                totalPointsFound += numPointsFound;
            }
        }
    }
    return totalPointsFound;
}

float getRelativeSpeed(VoxelEntity *a, VoxelEntity *b) {
     //NOTE: Get the distance of the point from center of mass 
    float3 dpPointA = plus_float3(a->dP, float3_cross(a->dA, a->furtherestVoxel));
    float3 dpPointB = plus_float3(b->dP, float3_cross(b->dA, b->furtherestVoxel));

    float result = float3_magnitude(minus_float3(dpPointB, dpPointA));

    return result;
}


void collideVoxelEntities(void *data_) {

    VoxelCollideData *collideData = (VoxelCollideData *)data_;

    VoxelEntity *a = collideData->a;
    VoxelEntity *b = collideData->b;
    
    collideData->pointCount = 0;
    
    a->inBounds = true;
    b->inBounds = true;

    //NOTE: Keep the order consistent with the order in the arbiter
    if(a > b) {
        VoxelEntity *temp = b;
        b = a;
        a = temp;
    }

    TransformX aT = a->T;
    TransformX bT = b->T;

    float3 aDA = a->dA;
    aDA.x *= -1;
    aDA.y *= -1;
    aDA.z *= -1;

    float3 bDA = b->dA;
    bDA.x *= -1;
    bDA.y *= -1;
    bDA.z *= -1;

    int minSpeculativeCollisionIterations = 2;
    int maxSpeculativeCollisionIterations = 100;
    float marginFactor = 1.0f;

    float relSpeed = getRelativeSpeed(a, b);

    float dtAccum = 0;
    if(relSpeed != 0) {
        dtAccum = ((marginFactor*VOXEL_SIZE_IN_METERS) / relSpeed); 
    }

    int increments = 0;

    if(dtAccum > 0) {
        increments = ceil(collideData->deltaStep / dtAccum);
    }

    if(increments > maxSpeculativeCollisionIterations) {
        //NOTE: clamp the loop to a certain number of steps
        increments = maxSpeculativeCollisionIterations;
        dtAccum = collideData->deltaStep / (float)maxSpeculativeCollisionIterations;
    } else if(increments < minSpeculativeCollisionIterations) {
        increments = minSpeculativeCollisionIterations;
        dtAccum = collideData->deltaStep / (float)minSpeculativeCollisionIterations;
    } else {
        int h = 0;
    }
    
    float dt = 0;
    bool search = true;

    VoxelEntity tempA = *a;
    VoxelEntity tempB = *b;

    assert(dtAccum < collideData->deltaStep);

    for(int i = 0; i < increments && search; i++) {
        tempA.T.rotation = integrateAngularVelocity(aT.rotation, aDA, dt);
        tempA.T.pos = plus_float3(aT.pos, scale_float3(dt, a->dP));
        tempB.T.rotation = integrateAngularVelocity(bT.rotation, bDA, dt);
        tempB.T.pos = plus_float3(bT.pos, scale_float3(dt, b->dP));

        Rect3f aRect;
        Rect3f bRect; 
        bool collided = boundingBoxOverlapWithMargin(&tempA, &tempB, &aRect, &bRect, BOUNDING_BOX_MARGIN);
        if(collided) {
            //TODO: Do we need this??
            aRect = rect3f_expand_uniform(aRect, VOXEL_SIZE_IN_METERS);
            bRect = rect3f_expand_uniform(bRect, VOXEL_SIZE_IN_METERS);

            int points1 = checkCornerAndEdgeVoxels(collideData, &tempA, &tempB, aRect, true);
            int points2 = checkCornerAndEdgeVoxels(collideData, &tempB, &tempA, bRect, false);

            if((points1 + points2) > 0) {
                search = false;
            }
        }

        dt += dtAccum;
    }
}