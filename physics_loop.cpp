
void renderVoxelEntities(GameState *gameState) {
    
    for(int i = 0; i < gameState->voxelEntityCount; ++i) {
        PROFILE_FUNC(RENDER_CUBES);
        VoxelEntity *e = &gameState->voxelEntities[i];

        const float16 T = sqt_to_float16(e->T.rotation, e->T.scale, e->T.pos);

        {
            //TODO: Change this to a perframe dynamic array
            ModelBufferList *l = pushStruct(&globalPerFrameArena, ModelBufferList);
            l->modelBuffer = e->mesh.modelBuffer;
            l->palletteId = e->colorPalletteId;

            if(!e->centerOnPosition) {
                l->min_AABB = make_float3(0, 0, 0);
                l->max_AABB = e->T.scale;
            } else {
                float3 halfScale = scale_float3(0.5f, e->T.scale);
                l->min_AABB = float3_negate(halfScale);
                l->max_AABB = halfScale;
            }

            l->data = getInstanceDataWithRotation(T, make_float4(1, 0.5f, 0, 1), make_float4(0.25f, 0.5f, 0, 0.25f));
            l->next = gameState->renderer->voxelEntityMeshes;
            gameState->renderer->voxelEntityMeshes = l;
        } 
        
        
        if(false)
        {
            float3 center = make_float3(0.5f*e->worldBounds.x, 0.5f*e->worldBounds.y, 0.5f*e->worldBounds.z);
            
            
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
    }
}

static int physicsLoopsCount = 0;
void updatePhysicsSim(GameState *gameState) {
    gameState->physicsAccum += gameState->dt;

    float minStep = PHYSICS_TIME_STEP;
    global_updateArbCount = 0;

    for(int i = 0; i < gameState->voxelEntityCount; ++i) {
        VoxelEntity *e = &gameState->voxelEntities[i];
        e->ddPForFrame = make_float3(0, 0, 0);
        e->ddAForFrame = make_float3(0, 0, 0);
        if(e->inverseMass > 0 && e != gameState->grabbed && (e->flags & GRAVITY_AFFECTED)) {
            e->ddPForFrame.y -= 10.0f; //NOTE: Gravity
        }

        e->inBounds = false;
    }

    physicsLoopsCount = 0;
    Uint32 startPhysics = profiler_getCount();

    int interations = 0;

    gameState->physicsWorld.entitiesNeedRebuilding = initResizeArrayArena(VoxelEntity *, &globalPerFrameArena);
    
    //NOTE: Physics loop
    while(gameState->physicsAccum >= minStep && interations < 4) {
        physicsLoopsCount++;
        interations++;
        float dt = minStep;
        
        {
            for(int i = 0; i < gameState->voxelEntityCount; ++i) {
                VoxelEntity *e = &gameState->voxelEntities[i];

                for(int j = i + 1; j < gameState->voxelEntityCount; ++j) {
                    VoxelEntity *e1 = &gameState->voxelEntities[j];

                    if(e != e1) {
                        if (!(e->inverseMass == 0.0f && e1->inverseMass == 0.0f)) {
                            
                            collideEntitiesMultiThread(gameState, e, e1, dt); 
                        }
                    } else {
                        assert(false);
                    }
                }
            }
        }   

        {
            PROFILE_FUNC(WaitForThreadsCollision);
            waitForPerFrameWorkToFinish(&gameState->threadsInfo);
        }

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
            PROFILE_FUNC(RunPGSSolver);
            //NOTE: Apply impluses
            runPGSSolver(&gameState->physicsWorld);
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

    global_timeInPhysicsUpdate_cycles = profiler_getCount() - startPhysics;
}