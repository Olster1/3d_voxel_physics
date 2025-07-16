u32 voxelGrassBitmap[256] = {0xff00781a, 0xff00b327, 0xff0a3000};

#pragma pack(push, 1)
struct VoxHeader {
    char magic[4];     // 'V' 'O' 'X' ' '
    uint32_t version;  // Usually 150
};

struct VoxChunkHeader {
    char id[4];              // e.g., 'MAIN', 'SIZE', 'XYZI'
    int contentSize;    // size of the chunk content
    int childrenSize;   // size of all children chunks (0 if none)
};

struct VoxelColorPallete {
    u32 colors[256];
};

#pragma pack(pop)

#define MAX_VOX_FILE_DIMENSION 256
struct VoxelModel {
    float3 maxBounds;
    int totalVoxelCount;
    u32 *voxelData;
    u32 *colors;
    int colorPalletteId; //NOTE: This is the id given to the voxel model to identify where it's color data is placed in the global color pallette texture. It's not dervied from the .vox file.
};

VoxelModel loadVoxFile(char *absoluteFilePath) {
    VoxelModel voxelModel = {};

    FileContents file = getFileContentsNullTerminate(absoluteFilePath);
    if(file.valid) {

        u8 *start = (u8 *)file.memory;
        VoxHeader *header = (VoxHeader *)start;
        assert(header->magic[0] == 'V' && header->magic[1] == 'O' && header->magic[2] == 'X');

        u8 *at = ((u8 *)file.memory) + sizeof(VoxHeader);

#define checkMagicNumber(id, data) (id[0] == data[0] && id[1] == data[1] && id[2] == data[2] && id[3] == data[3])

        bool parsing = true;
        while(parsing) {
            VoxChunkHeader *chunk = (VoxChunkHeader *)at;
            u8 *data = at + sizeof(VoxChunkHeader);

            if(checkMagicNumber(chunk->id, "PACK")) {
                int numberOfModels = *((int *)data);
                assert(numberOfModels == 1);
            } else if(checkMagicNumber(chunk->id, "SIZE")) {
                voxelModel.maxBounds.x = (float)(((int *)data)[0]);
                voxelModel.maxBounds.y = (float)(((int *)data)[1]);
                voxelModel.maxBounds.z = (float)(((int *)data)[2]);
            } else if(checkMagicNumber(chunk->id, "XYZI")) {
                u8 *d = (u8 *)data;
                voxelModel.totalVoxelCount = ((int *)d)[0];
                d += sizeof(int);

                voxelModel.voxelData = pushArray(&globalLongTermArena, voxelModel.totalVoxelCount, u32);

                easyPlatform_copyMemory(voxelModel.voxelData, d, sizeof(u32)*voxelModel.totalVoxelCount);
                
             } else if(checkMagicNumber(chunk->id, "RGBA")) {
                u32 bufferSizeInBytes = sizeof(u32)*256;
                voxelModel.colors = pushArray(&globalLongTermArena, bufferSizeInBytes, u32);
                easyPlatform_copyMemory(voxelModel.colors, data, bufferSizeInBytes);
            }

            at += chunk->contentSize + sizeof(VoxChunkHeader);
            if(*at == '\0' || ((size_t)(at - start) >= file.fileSize)) {
                parsing = false;
            }

#undef checkMagicNumber
        }
    }

    if(file.memory) {
        free(file.memory);
    }

    return voxelModel;
}