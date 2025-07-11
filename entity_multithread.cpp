

void collideEntitiesMultiThread(GameState *gameState, VoxelEntity *a, VoxelEntity *b) {
    Rect3f aRect;
    Rect3f bRect; 
    bool collided = boundingBoxOverlapWithMargin(a, b, &aRect, &bRect, BOUNDING_BOX_MARGIN);
    if(collided) {
        VoxelCollideData *data = 0;
        if(gameState->voxelCollideDataFreeList) {
            data = gameState->voxelCollideDataFreeList;
            gameState->voxelCollideDataFreeList = data->next;
        } else {
            data = pushStruct(&globalLongTermArena, VoxelCollideData);
        }

        data->a = a;
        data->b = b;
        data->aRect = rect3f_expand_uniform(aRect, VOXEL_SIZE_IN_METERS);
        data->bRect = rect3f_expand_uniform(bRect, VOXEL_SIZE_IN_METERS);
        data->pointCount = 0;
        data->next = 0;

        data->next = gameState->voxelCollideData;
        gameState->voxelCollideData = data;
        

        MemoryBarrier();
        ReadWriteBarrier();

        //NOTE: Multi-threaded version
        pushWorkOntoQueue(&gameState->threadsInfo, collideVoxelEntities, data);
    }
}