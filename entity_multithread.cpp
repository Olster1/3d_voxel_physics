

void collideEntitiesMultiThread(GameState *gameState, VoxelEntity *a, VoxelEntity *b, float dt) {
   
    VoxelCollideData *data = 0;
    if(gameState->voxelCollideDataFreeList) {
        data = gameState->voxelCollideDataFreeList;
        gameState->voxelCollideDataFreeList = data->next;
    } else {
        data = pushStruct(&globalLongTermArena, VoxelCollideData);
    }

    data->a = a;
    data->b = b;
    data->pointCount = 0;
    data->next = 0;
    data->deltaStep = dt;

    data->next  = gameState->voxelCollideData;
    gameState->voxelCollideData = data;
    

    MemoryBarrier();
    ReadWriteBarrier();

    //NOTE: Multi-threaded version
    pushWorkOntoQueue(&gameState->threadsInfo, collideVoxelEntities, data);
    // collideVoxelEntities(data);
    
}
