u32 voxelGrassBitmap[256] = {0xFF1D3E28, 0xFF1F4A2A, 0xFF20552D, 0xFF226030, 0xFF246B33,
0xFF267535, 0xFF288038, 0xFF2A8A3A, 0xFF2C953D, 0xFF2FA03F,
0xFF31AA42, 0xFF34B445, 0xFF37BD47, 0xFF39C649, 0xFF3CCF4B,
0xFF3FD74D, 0xFF42DF4F, 0xFF45E651, 0xFF48EC53, 0xFF4CF155,
0xFF4FF557, 0xFF56F959, 0xFF56FB5B, 0xFF5AFD5D, 0xFF5EFF5F,
0xFF5AF85D, 0xFF55F05A, 0xFF50E756, 0xFF4BDD52, 0xFF47D24F,
0xFF42C74B, 0xFF3EBB47, 0xFF3AAF44, 0xFF36A340, 0xFF32963D,
0xFF2E8A39, 0xFF2B7D36, 0xFF277133, 0xFF24642F, 0xFF21582C,
0xFF1E4B29, 0xFF1B3F25, 0xFF183322, 0xFF16281E, 0xFF131C1B,
0xFF101218, 0xFF111820, 0xFF121E28, 0xFF132430, 0xFF142A38,
0xFF153040, 0xFF163648, 0xFF173C50, 0xFF184258, 0xFF194860,
0xFF1A4E68, 0xFF1C5470, 0xFF1D5A77, 0xFF1E6080, 0xFF206687,
0xFF216C8F, 0xFF237297, 0xFF24789F, 0xFF267EA7, 0xFF2784AF,
0xFF298AB7, 0xFF2A90BF, 0xFF2C96C7, 0xFF2D9CCF, 0xFF2FA2D7,
0xFF32A6D6, 0xFF34AAD5, 0xFF36AED4, 0xFF38B2D3, 0xFF3AB6D2,
0xFF3CBAD1, 0xFF3EBECF, 0xFF40C2CE, 0xFF42C6CD, 0xFF44CACC,
0xFF46CECB, 0xFF48D2CA, 0xFF4CD6C9, 0xFF4CDAC8, 0xFF4EDEC7,
0xFF50E2C6, 0xFF52E6C5, 0xFF54EAC4, 0xFF56EEC3, 0xFF58F2C2,
0xFF5AF6C1, 0xFF5CFAC0, 0xFF5EFEBD, 0xFF5DFDBA, 0xFF5CFCB7,
0xFF5BFBB4, 0xFF5AFAB1, 0xFF59F9AE, 0xFF58F8AB, 0xFF57F7A8,
0xFF56F6A5, 0xFF55F5A2, 0xFF54F4A0, 0xFF53F39D, 0xFF52F29A,
0xFF51F197, 0xFF50F093, 0xFF4FEF90, 0xFF4EEE8D, 0xFF4DED8A,
0xFF4CEC87, 0xFF4BEB84, 0xFF4AEA81, 0xFF49E97E, 0xFF48E87B,
0xFF47E778, 0xFF46E675, 0xFF45E572, 0xFF44E46F, 0xFF43E36C,
0xFF42E269, 0xFF41E166, 0xFF40E063, 0xFF3FDF60, 0xFF3EDE5D,
0xFF3DDD5A, 0xFF3CDC57, 0xFF3BDB54, 0xFF3ADA51, 0xFF39D94E,
0xFF38D84B, 0xFF37D748, 0xFF36D645, 0xFF35D542, 0xFF34D43F,
0xFF33D33C, 0xFF32D239, 0xFF31D136, 0xFF30D033, 0xFF2FCF30,
0xFF2ECE2D, 0xFF2DCD2A, 0xFF2ACC27, 0xFF2BCB24, 0xFF2ACA21,
0xFF29C91E, 0xFF28C81B, 0xFF27C718, 0xFF26C615, 0xFF25C512,
0xFF24C40F, 0xFF23C30C, 0xFF22C209, 0xFF21C106, 0xFF20C003,
0xFF1FBE00, 0xFF1EBB00, 0xFF1DB800, 0xFF1CB500, 0xFF1AB200,
0xFF19AF00, 0xFF18AC00, 0xFF17A900, 0xFF16A600, 0xFF15A300,
0xFF14A000, 0xFF139D00, 0xFF129A00, 0xFF119700, 0xFF109400,
0xFF0F9100, 0xFF0E8E00, 0xFF0D8B00, 0xFF0C8800, 0xFF0B8500,
0xFF0A8200, 0xFF097F00, 0xFF087C00, 0xFF077900, 0xFF067600,
0xFF057300, 0xFF047000, 0xFF036D00, 0xFF026A00, 0xFF016700,
0xFF006400, 0xFF006100, 0xFF005F0A, 0xFF005D14, 0xFF005B1E,
0xFF005928, 0xFF005732, 0xFF00553C, 0xFF005346, 0xFF005150,
0xFF004F5A, 0xFF004D64, 0xFF004B6E, 0xFF004978, 0xFF004782,
0xFF00458C, 0xFF004396, 0xFF0041A0, 0xFF003FAA, 0xFF003DB4,
0xFF003BBE, 0xFF0039C8, 0xFF0037D2, 0xFF0035DC, 0xFF0033E6,
0xFF0031F0, 0xFF002FFA, 0xFF002CF4, 0xFF0029EE, 0xFF0026E8,
0xFF0023E2, 0xFF0020DC, 0xFF001DD6, 0xFF001AD0, 0xFF0017CA,
0xFF0014C4, 0xFF0011BE, 0xFF000EB8, 0xFF000BB2, 0xFF0008AC,
0xFF0005A6, 0xFF0002A0, 0xFF00009A, 0xFF000094, 0xFF00008E,
0xFF000088, 0xFF000082, 0xFF00007C, 0xFF000076, 0xFF000070};

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