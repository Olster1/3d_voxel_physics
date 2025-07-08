void generateVoxelEntityMesh_multiThread(void *data_) {
    /*
        This is the function that turns the voxel data into a drawable mesh. 
        It uses a greedy mesh algorithm, but need to change it to actually be greedy!
    */
    GenerateMeshDataWithList *data = (GenerateMeshDataWithList *)data_;

    MultiThreadedMeshList *meshGenerator = data->meshList;
    ChunkVertexToCreate *info = data->info;
    VoxelEntity *e = info->voxelEntity;

    if(e->mesh.generateState & CHUNK_MESH_BUILDING) {
         for(int z = 0; z < e->depth; z++) {
            for(int y = 0; y < e->pitch; y++) {
                for(int x = 0; x < e->stride; x++) {
                    u8 eflags = e->data[getVoxelIndex(e, x, y, z)];

                    if(eflags & VOXEL_OCCUPIED) {
                        float3 worldP = make_float3(x, y, z);

                        //NOTE: Run Greedy mesh algorithm
                        for(int k = 0; k < arrayCount(meshGenerator->cardinalOffsets); k++) {
                            float3 p = plus_float3(worldP, meshGenerator->cardinalOffsets[k]);
                            
                            if(!isVoxelOccupied(e, p.x, p.y, p.z))
                            {
                                //NOTE: Face is exposed so add it to the mesh
                                int vertexCount = getArrayLength(info->triangleData);
                                //NOTE: 4 vertices for a cube face
                                for(int i = 0; i < 4; ++i) {
                                    int indexIntoCubeData = i + k*4;
                                    const VertexForChunk v = global_cubeDataForChunk[indexIntoCubeData];

                                    VertexForChunk vForChunk = v;
                                    vForChunk.pos = plus_float3(worldP, scale_float3(VOXEL_SIZE_IN_METERS, v.pos));
                                    float2 uvAtlas = make_float2(0.1f, 0.2f);
                                    vForChunk.texUV.y = lerp(uvAtlas.x, uvAtlas.y, make_lerpTValue(vForChunk.texUV.y));

                                    pushArrayItem(&info->triangleData, vForChunk, VertexForChunk);
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
                e->mesh.modelBuffer = generateChunkVertexBuffer(info->triangleData, vertexCount, info->indicesData, indexCount);
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
    } else {
        info = pushStruct(&globalLongTermArena, ChunkVertexToCreate);
        //TODO: Change this to a fixed sized array with each threads own memory arena
    }
    assert(info);

    info->triangleData = initResizeArray(VertexForChunk);
    info->indicesData = initResizeArray(u32);

    //NOTE: Add to the list
    info->next = meshGenerator->meshesToCreate;
    meshGenerator->meshesToCreate = info;
    info->ready = false;
    info->voxelEntity = e;
    info->generation = e->mesh.generation++;

    data->info = info;

    //NOTE: Multi-threaded
    pushWorkOntoQueue(meshGenerator->threadsInfo, generateVoxelEntityMesh_multiThread, data);

}
