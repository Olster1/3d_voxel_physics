u32 voxelGrassBitmap[256] = {
    // Index 0: BLOCK_NONE (Invisible / Air)
    0x00000000,

    // Indices 1 - 28: BLOCK_GRASS (4 Clean Greens)
    // Indices 1 - 28: BLOCK_GRASS (4 Clean Light Greens)
    0xff27b300, 0xff27b300, 0xff27b300, 0xff27b300, 0xff27b300, 0xff27b300, 0xff27b300, // Original Light Green
    0xff36cc00, 0xff36cc00, 0xff36cc00, 0xff36cc00, 0xff36cc00, 0xff36cc00, 0xff36cc00, // Bright Lime Green
    0xff4fe61a, 0xff4fe61a, 0xff4fe61a, 0xff4fe61a, 0xff4fe61a, 0xff4fe61a, 0xff4fe61a, // Pale Mint-Lime
    0xff63ff40, 0xff63ff40, 0xff63ff40, 0xff63ff40, 0xff63ff40, 0xff63ff40, 0xff63ff40, // Vibrant Pastel Green

    // Indices 29 - 56: BLOCK_SOIL (4 Clean Browns)
    0xff2a4d6e, 0xff2a4d6e, 0xff2a4d6e, 0xff2a4d6e, 0xff2a4d6e, 0xff2a4d6e, 0xff2a4d6e,
    0xff3c668c, 0xff3c668c, 0xff3c668c, 0xff3c668c, 0xff3c668c, 0xff3c668c, 0xff3c668c,
    0xff1f3347, 0xff1f3347, 0xff1f3347, 0xff1f3347, 0xff1f3347, 0xff1f3347, 0xff1f3347,
    0xff3e5269, 0xff3e5269, 0xff3e5269, 0xff3e5269, 0xff3e5269, 0xff3e5269, 0xff3e5269,

    // Indices 57 - 84: BLOCK_STONE (4 Clean Grays)
    0xff7a7a7a, 0xff7a7a7a, 0xff7a7a7a, 0xff7a7a7a, 0xff7a7a7a, 0xff7a7a7a, 0xff7a7a7a,
    0xffa6a6a6, 0xffa6a6a6, 0xffa6a6a6, 0xffa6a6a6, 0xffa6a6a6, 0xffa6a6a6, 0xffa6a6a6,
    0xff4d4d4d, 0xff4d4d4d, 0xff4d4d4d, 0xff4d4d4d, 0xff4d4d4d, 0xff4d4d4d, 0xff4d4d4d,
    0xff404040, 0xff404040, 0xff404040, 0xff404040, 0xff404040, 0xff404040, 0xff404040,

    // Indices 85 - 112: BLOCK_TREE_WOOD (4 Clean Wood Tones)
    0xff1f3852, 0xff1f3852, 0xff1f3852, 0xff1f3852, 0xff1f3852, 0xff1f3852, 0xff1f3852,
    0xff294663, 0xff294663, 0xff294663, 0xff294663, 0xff294663, 0xff294663, 0xff294663,
    0xff16283b, 0xff16283b, 0xff16283b, 0xff16283b, 0xff16283b, 0xff16283b, 0xff16283b,
    0xff20354d, 0xff20354d, 0xff20354d, 0xff20354d, 0xff20354d, 0xff20354d, 0xff20354d,

    // Indices 113 - 140: BLOCK_TREE_LEAVES (4 Clean Canopy Greens)
    0xff0d421d, 0xff0d421d, 0xff0d421d, 0xff0d421d, 0xff0d421d, 0xff0d421d, 0xff0d421d,
    0xff145c2b, 0xff145c2b, 0xff145c2b, 0xff145c2b, 0xff145c2b, 0xff145c2b, 0xff145c2b,
    0xff1e783a, 0xff1e783a, 0xff1e783a, 0xff1e783a, 0xff1e783a, 0xff1e783a, 0xff1e783a,
    0xff104520, 0xff104520, 0xff104520, 0xff104520, 0xff104520, 0xff104520, 0xff104520,

    // Indices 141 - 168: BLOCK_WATER (4 Clean Blues)
    0xff9c5214, 0xff9c5214, 0xff9c5214, 0xff9c5214, 0xff9c5214, 0xff9c5214, 0xff9c5214,
    0xffb86b21, 0xffb86b21, 0xffb86b21, 0xffb86b21, 0xffb86b21, 0xffb86b21, 0xffb86b21,
    0xff7a3b0a, 0xff7a3b0a, 0xff7a3b0a, 0xff7a3b0a, 0xff7a3b0a, 0xff7a3b0a, 0xff7a3b0a,
    0xff5c2a03, 0xff5c2a03, 0xff5c2a03, 0xff5c2a03, 0xff5c2a03, 0xff5c2a03, 0xff5c2a03,

    // Indices 169 - 196: BLOCK_COAL (4 Clean Dark Minerals)
    0xff141414, 0xff141414, 0xff141414, 0xff141414, 0xff141414, 0xff141414, 0xff141414,
    0xff1f1f1f, 0xff1f1f1f, 0xff1f1f1f, 0xff1f1f1f, 0xff1f1f1f, 0xff1f1f1f, 0xff1f1f1f,
    0xff2b2b2b, 0xff2b2b2b, 0xff2b2b2b, 0xff2b2b2b, 0xff2b2b2b, 0xff2b2b2b, 0xff2b2b2b,
    0xff1a1d21, 0xff1a1d21, 0xff1a1d21, 0xff1a1d21, 0xff1a1d21, 0xff1a1d21, 0xff1a1d21,

    // Indices 197 - 255: BLOCK_IRON (4 Clean Slate/Iron Tones padded out to the end)
    0xff243342, 0xff243342, 0xff243342, 0xff243342, 0xff243342, 0xff243342, 0xff243342, 0xff243342, 0xff243342, 0xff243342, 0xff243342, 0xff243342, 0xff243342, 0xff243342,
    0xff314559, 0xff314559, 0xff314559, 0xff314559, 0xff314559, 0xff314559, 0xff314559, 0xff314559, 0xff314559, 0xff314559, 0xff314559, 0xff314559, 0xff314559, 0xff314559,
    0xff405973, 0xff405973, 0xff405973, 0xff405973, 0xff405973, 0xff405973, 0xff405973, 0xff405973, 0xff405973, 0xff405973, 0xff405973, 0xff405973, 0xff405973, 0xff405973,
    0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630, 0xff1b2630
};
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

                float3 maxBounds = make_float3(0, 0, 0);
                float3 minBounds = make_float3(FLT_MAX, FLT_MAX, FLT_MAX);

                for(int i = 0; i < voxelModel.totalVoxelCount; i++) {
                    u32 coords = voxelModel.voxelData[i];

                    int x = coords & 0xFF;
                    int y = (coords >> 16) & 0xFF;
                    int z = (coords >> 8) & 0xFF;
                    int c = (coords >> 24) & 0xFF;

                    //NOTE: We flip the y & z value since .vox file z is gravity direction and in our engine y is gravity direction.
                    voxelModel.voxelData[i] = (x) | (y << 8) | (z << 16) | (c << 24);

                    if(x < minBounds.x) {
                        minBounds.x = x;
                    }
                    if(y < minBounds.y) {
                        minBounds.y = y;
                    }
                    if(z < minBounds.z) {
                        minBounds.z = z;
                    }

                    if(x > maxBounds.x) {
                        maxBounds.x = x;
                    }
                    if(y > maxBounds.y) {
                        maxBounds.y = y;
                    }
                    if(z > maxBounds.z) {
                        maxBounds.z = z;
                    }
                }
                maxBounds.x++;
                maxBounds.y++;
                maxBounds.z++;
                voxelModel.maxBounds = maxBounds;
                voxelModel.minBounds = minBounds;
                voxelModel.voxelDim = minus_float3(maxBounds, minBounds);

                if(voxelModel.voxelDim.x < 0) voxelModel.voxelDim.x = 0;
                if(voxelModel.voxelDim.y < 0) voxelModel.voxelDim.y = 0;
                if(voxelModel.voxelDim.z < 0) voxelModel.voxelDim.z = 0;

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