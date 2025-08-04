float3 getBlockWorldPos(BlockChunkPartner b) {
    float3 p = {};

    p.x = b.block->x + (CHUNK_DIM*b.chunk->x);
    p.y = b.block->y + (CHUNK_DIM*b.chunk->y);
    p.z = b.block->z + (CHUNK_DIM*b.chunk->z);

    return p;
}

BlockChunkPartner castRayAgainstBlock(GameState *gameState, float3 dir, float length, float3 start, BlockFlags blockFlags = BLOCK_EXISTS) {
    int blockRadius = length;
    float shortestT = FLT_MAX;
    BlockChunkPartner block = {};
    block.block = 0;

    for(int z = -blockRadius; z <= blockRadius; ++z) {
        for(int x = -blockRadius; x <= blockRadius; ++x) {
            for(int y = -blockRadius; y <= blockRadius; ++y) {
                float3 pos = start;
                //NOTE: Get the block pos in world
                float3 blockPos = convertRealWorldToBlockCoords(pos);
                int worldx = (int)blockPos.x + x;
                int worldy = (int)blockPos.y + y;
                int worldz = (int)blockPos.z + z;

                BlockChunkPartner blockTemp = blockExists(gameState, worldx, worldy, worldz, blockFlags);

                if(blockTemp.block) {
                    //NOTE: Ray Cast against the block
                    Rect3f b = make_rect3f_center_dim(make_float3(worldx, worldy, worldz), make_float3(1, 1, 1));
                    float3 hitPoint;
                    float3 normalVector;
                    float tAt;
                    if(easyMath_rayVsAABB3f(pos, dir, b, &hitPoint, &tAt, &normalVector)) {
                        if(tAt <= length && tAt < shortestT) {
                            shortestT = tAt;
                            block = blockTemp;
                            block.sideNormal = normalVector;
                        }
                    }
                }
            }
        }
    }

    return block;
}

Particler *findParticler(GameState *gameState, ParticlerId id) {
    Particler *result = 0;
    for(int i = 0; i < gameState->particlerCount && !result; ++i) {
        Particler *p = &gameState->particlers[i];

        if(particlerIdsMatch(p->id, id)) {
            result = p;
            break;
        }
    }

    return result;

}

void highlightBlockLookingAt(GameState *gameState, float3 lookingAxis, Entity *e) {
    BlockChunkPartner b = castRayAgainstBlock(gameState, lookingAxis, DISTANCE_CAN_PLACE_BLOCK, plus_float3(gameState->cameraOffset, e->T.pos), BLOCK_FLAGS_MINEABLE);

    if(b.block) {
        float3 blockWorldP = getBlockWorldPos(b);
        pushAlphaCube(gameState->renderer, blockWorldP, BLOCK_OUTLINE, make_float4(0, 0, 0, 0.1f));
        gameState->DEBUG_BlocksDrawnForFrame++;
    }
}

void updatePlayer(GameState *gameState) {
    VoxelEntity cameraEntity = {};
    cameraEntity.T = gameState->camera.T;
    cameraEntity.worldBounds = make_float3(1, 1, 1);
    cameraEntity.T.scale = make_float3(cameraEntity.worldBounds.x, cameraEntity.worldBounds.y, cameraEntity.worldBounds.z);

    float16 rot = eulerAnglesToTransform(gameState->camera.T.rotation.y, gameState->camera.T.rotation.x, gameState->camera.T.rotation.z);
    float3 zAxis = make_float3(rot.E_[2][0], rot.E_[2][1], rot.E_[2][2]);
    cameraEntity.T.pos = plus_float3(cameraEntity.T.pos, scale_float3(3, zAxis));

    if(gameState->playerHolding) {
        gameState->playerHolding->T.pos = cameraEntity.T.pos;
        // gameState->playerHolding->T.rotation = eulerAnglesToQuaternion(cameraEntity.T.rotation.y + 90, 0, 0);

        //NOTE: Attack with the item 
        if(gameState->keys.keys[KEY_X] == MOUSE_BUTTON_DOWN) {
            gameState->playerHolding->ddAForFrame.x = 10000;
        }

        if(gameState->keys.keys[KEY_SPACE] == MOUSE_BUTTON_PRESSED) {
            gameState->playerHolding->dP = scale_float3(5, zAxis);
            gameState->playerHolding = 0;
        }
    }

    for(int i = 0; i < gameState->voxelEntityCount; ++i) {
        VoxelEntity *e = &gameState->voxelEntities[i];
        bool modifiedData = false;

        if((e->flags & PLAYER_CAN_PICKUP) && !gameState->playerHolding)
        {
            VoxelEntity playerEntity = cameraEntity;
            playerEntity.worldBounds = make_float3(1, 1, 1);
            playerEntity.T.scale = make_float3(1, 1, 1);
            playerEntity.T.rotation = identityQuaternion();

            Rect3f aRect;
            Rect3f bRect; 
            bool overlap = boundingBoxOverlapWithMargin(&playerEntity, e, &aRect, &bRect, 1);

            if(overlap) {
                renderText(gameState->renderer, &gameState->mainFont, "PICKUP ITEM", make_float2(50, 90*gameState->aspectRatio_y_over_x), 0.1f);
            }
            if(gameState->keys.keys[KEY_SPACE] == MOUSE_BUTTON_PRESSED) {
                gameState->playerHolding = e;
                e->flags &= ~(GRAVITY_AFFECTED);
                // removeVoxelEntityAngularInertia(e);
                e->T.rotation = identityQuaternion();
                e->dA = make_float3(0, 0, 0);
            }
            
        }
    }
}

void updatePlayer_OLD(GameState *gameState, Entity *e) {
    if(gameState->camera.followingPlayer) {
        float rotSpeed = 13.0f;

        float2 mouseDelta = minus_float2(gameState->mouseP_screenSpace, gameState->lastMouseP);

        e->T.rotation.y += gameState->dt*rotSpeed*-mouseDelta.x;
        e->T.rotation.x += gameState->dt*rotSpeed*-mouseDelta.y;

        if(e->T.rotation.x > 89) {
            e->T.rotation.x = 89;
        }
        if(e->T.rotation.x < -89) {
            e->T.rotation.x = -89;
        }
    }

    float16 rot = eulerAnglesToTransform(e->T.rotation.y, e->T.rotation.x, e->T.rotation.z);
    float3 movement = make_float3(0, 0, 0);

    if(gameState->camera.followingPlayer) {

        float3 zAxis = make_float3(rot.E_[2][0], 0, rot.E_[2][2]); //NOTE: rot.E_[2][1] in y if we want to fly
        zAxis = normalize_float3(zAxis);
        float3 xAxis = make_float3(rot.E_[0][0], rot.E_[0][1], rot.E_[0][2]);

        float3 playerSpeed = make_float3(1, 1, 1);
        float magnitude = gameState->dt*WALK_SPEED;

        if(gameState->keys.keys[KEY_LEFT]) {
            movement = plus_float3(movement, float3_negate(xAxis));
        }
        if(gameState->keys.keys[KEY_RIGHT]) {
            movement = plus_float3(movement, xAxis);
        }
        if(gameState->keys.keys[KEY_DOWN]) {
            movement = plus_float3(movement, float3_negate(zAxis));
        }
        if(gameState->keys.keys[KEY_UP]) {
            movement = plus_float3(movement, zAxis);

            if(gameState->keys.keys[KEY_SHIFT] && e->grounded) {
                //NOTE: Player is running
                magnitude *= 2.0f;

                if(!e->running) {
                    e->running = true;
                    
                    gameState->camera.runShakeTimer = 0;
                }
                
            }
        }

        if(e->running) {
            gameState->camera.targetFov = 80;
            e->stamina -= gameState->dt*STAMINA_DRAIN_SPEED;

            if(e->stamina < 0) {
                e->stamina = 0;
            }
        } else {
            e->stamina += gameState->dt*STAMINA_RECHARGE_SPEED;

            if(e->stamina >= 1) {
                e->stamina = 1;
            }
        }

        movement = normalize_float3(movement);
        movement = scale_float3(magnitude, movement);

        if(!gameState->keys.keys[KEY_SHIFT] || e->stamina <= 0) {
            e->running = false;
            gameState->camera.runShakeTimer = -1;
        }
        
        if((gameState->keys.keys[KEY_SPACE] == MOUSE_BUTTON_PRESSED || e->tryJump) && e->grounded) {
            //NOTE: JUMP
            e->dP.y += JUMP_POWER;
        }
        e->tryJump = false;
    } 
    
}