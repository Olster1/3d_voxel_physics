
void generateVoxelEntityMesh_multiThread(void *data_) {
    /*
        This is the function that turns the voxel data into a drawable mesh. 
    */
    GenerateMeshDataWithList *data = (GenerateMeshDataWithList *)data_;

    MultiThreadedMeshList *meshGenerator = data->meshList;
    ChunkVertexToCreate *info = data->info;
    VoxelEntity *e = info->voxelEntity;

    float halfVoxel = 0.5f*VOXEL_SIZE_IN_METERS;
    float3 center = make_float3(0.5f*e->worldBounds.x, 0.5f*e->worldBounds.y, 0.5f*e->worldBounds.z);

    if(e->mesh.generateState & CHUNK_MESH_BUILDING) {
         for(int z = 0; z < e->depth; z++) {
            for(int y = 0; y < e->pitch; y++) {
                for(int x = 0; x < e->stride; x++) {
                    u8 eflags = e->data[getVoxelIndex(e, x, y, z)];

                    if(eflags & VOXEL_OCCUPIED) {
                        float3 voxelP = make_float3(x, y, z);

                        //NOTE: Run Greedy mesh algorithm
                        for(int k = 0; k < arrayCount(meshGenerator->cardinalOffsets); k++) {
                            float3 p = plus_float3(voxelP, meshGenerator->cardinalOffsets[k]);
                            
                            if(!isVoxelOccupied(e, p.x, p.y, p.z))
                            {
                                //NOTE: Face is exposed so add it to the mesh
                                int vertexCount = getArrayLength(info->triangleData);
                                //NOTE: 4 vertices for a cube face
                                for(int i = 0; i < 4; ++i) {
                                    int indexIntoCubeData = i + k*4;
                                    const Vertex v = global_cubeData_sameTexture[indexIntoCubeData];

                                    float3 modelP = {};
                                    modelP.x = x*VOXEL_SIZE_IN_METERS + halfVoxel - center.x;
                                    modelP.y = y*VOXEL_SIZE_IN_METERS + halfVoxel - center.y;
                                    modelP.z = z*VOXEL_SIZE_IN_METERS + halfVoxel - center.z;

                                    int palleteId = e->colorPalletteId;
                                    int colorId = e->colorData[getVoxelIndex(e, x, y, z)];
                                    VoxelVertex vForChunk = initVoxelVertex(plus_float3(modelP, scale_float3(VOXEL_SIZE_IN_METERS, v.pos)), v.normal, colorId, palleteId);

                                    pushArrayItem(&info->triangleData, vForChunk, VoxelVertex);
                                }
                                pushQuadIndicies(&info->indicesData, vertexCount);
                            }
                        }
                    }
                }
            }
        }
    }
    
    MemoryBarrier();
    ReadWriteBarrier();

    info->ready = true;

    free(data_);
    data_ = 0;
    
}

void processVoxelEntityMeshData(ChunkVertexToCreate *info) {
    assert(info->ready);
    VoxelEntity *e = info->voxelEntity;
    
    if(e->mesh.generationAt <= info->generation && e->mesh.generateState & CHUNK_MESH_BUILDING) {
        {
            
            int indexCount = getArrayLength(info->indicesData);
            int vertexCount = getArrayLength(info->triangleData);
            if(indexCount > 0 && vertexCount > 0) {
                
                if(e->mesh.modelBuffer.handle) {
                    //TODO: Change to just do sub-buffer data and not delete the vao
                    deleteVao(e->mesh.modelBuffer.handle);
                    e->mesh.modelBuffer.indexCount = 0;
                }
                e->mesh.modelBuffer = generateVertexBuffer(info->triangleData, vertexCount, info->indicesData, indexCount, ATTRIB_INSTANCE_TYPE_VOXEL_ENTITY);
                assert(e->mesh.modelBuffer.handle > 0);
            } 
        }

        e->mesh.generationAt = info->generation;
        assert(e->mesh.generateState & CHUNK_MESH_BUILDING);
        e->mesh.generateState &= ~CHUNK_MESH_BUILDING;
        assert(!(e->mesh.generateState & CHUNK_MESH_BUILDING));
    }

    freeResizeArray(info->triangleData);
    freeResizeArray(info->indicesData);
} 

void pushCreateVoxelEntityMeshToThreads(MultiThreadedMeshList *meshGenerator, VoxelEntity *e) {
    assert(e->mesh.generateState & CHUNK_MESH_DIRTY);
    e->mesh.generateState &= ~CHUNK_MESH_DIRTY;
    e->mesh.generateState |= CHUNK_MESH_BUILDING;
    assert(e->mesh.generateState & CHUNK_MESH_BUILDING);
    assert(!(e->mesh.generateState & CHUNK_MESH_DIRTY));
    
    
    MemoryBarrier();
    ReadWriteBarrier();

    GenerateMeshDataWithList *data = (GenerateMeshDataWithList *)malloc(sizeof(GenerateMeshDataWithList));

    data->meshList = meshGenerator;

    ChunkVertexToCreate *info = 0;

    if(meshGenerator->meshesToCreateFreeList) {
        info = meshGenerator->meshesToCreateFreeList;
        //NOTE: Take off free list
        meshGenerator->meshesToCreateFreeList = info->next;

        easyMemory_zeroSize(info, sizeof(ChunkVertexToCreate));
    } else {
        info = pushStruct(&globalLongTermArena, ChunkVertexToCreate);
        //TODO: Change this to a fixed sized array with each threads own memory arena
    }
    assert(info);

    info->triangleData = initResizeArray(VoxelVertex);
    info->indicesData = initResizeArray(u32);

    //NOTE: Add to the list
    info->next = meshGenerator->meshesToCreate;
    meshGenerator->meshesToCreate = info;
    info->ready = false;
    info->voxelEntity = e;
    info->generation = e->mesh.generation++;

    data->info = info;

    //NOTE: Multi-threaded
    // generateVoxelEntityMesh_multiThread(data);
    pushWorkOntoQueue(meshGenerator->threadsInfo, generateVoxelEntityMesh_multiThread, data);

}
