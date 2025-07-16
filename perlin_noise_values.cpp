//NOTE: Helper funciton since the simplex lib I'm using for 3d noise maps between -1 -> 1, and we want 0 -> 1
float mapSimplexNoiseTo01(float value) {
    value += 1;
    value *= 0.5f;

    assert(value >= 0 && value <= 1);

    return value;
}

float mapSimplexNoiseTo11(float value) {
    return value;
}

float convertRealWorldToWorldVoxelCoords(float p) {
    p = (round(p*10)*0.1)*VOXELS_PER_METER;

    return p;
}

float getTerrainHeight(float worldX, float worldZ) {
    #if SIMPLE_TERRAIN_HEIGHT
    float a = SimplexNoise_fractal_2d(16, worldX, worldZ, 0.01);
    a = mapSimplexNoiseTo01(a);

    float b = SimplexNoise_fractal_2d(16, worldX, worldZ, 1);
    b = mapSimplexNoiseTo01(a);
    float maxHeight = lerp(2, 5, make_lerpTValue(b));
    
    float terrainHeight = maxHeight*a;
    #else
    float a = SimplexNoise_fractal_2d(16, worldX, worldZ, 0.00522);
    a = mapSimplexNoiseTo01(a);

    float b = SimplexNoise_fractal_2d(16, worldX, worldZ, 0.007143);
    b = mapSimplexNoiseTo01(b);

    float c = SimplexNoise_fractal_2d(16, 0.1f*worldX, 0.1f*worldZ, 0.000043);
    c = mapSimplexNoiseTo01(c);

    float maxTerrainValue = lerp(100, 200, make_lerpTValue(c));

    float terrainAmplitude = lerp(10, maxTerrainValue, make_lerpTValue(b));
    float terrainHeight = a*terrainAmplitude; 

    #endif

    return terrainHeight;
}


bool isTreeLocation(int worldX, int worldZ) {
    float t = perlin2d(worldX, worldZ, 0.8f, 16);
    return t < 0.25f;
}

bool isBushLocation(int worldX, int worldZ) {
    float t = perlin2d(worldX, worldZ, 0.7f, 16);
    return t < 0.4f;
}

bool isBigBush(int worldX, int worldZ) {
    float t = perlin2d(worldX, worldZ, 0.9f, 16);
    return t < 0.5f;
}

bool isTallBuildingLocation(int worldX, int worldZ, VoxelModel *model) {
    bool result = false;

    int marginInVoxels = 0;
    int totalMarginForBothSides = marginInVoxels*2;
    
    float3 cellSize = plus_float3(model->maxBounds, make_float3(totalMarginForBothSides, totalMarginForBothSides, totalMarginForBothSides));
    float3 worldVoxelP = make_float3(convertRealWorldToWorldVoxelCoords(worldX), 0, convertRealWorldToWorldVoxelCoords(worldZ));

    int cellX = ((int)worldVoxelP.x / (int)cellSize.x);
    int cellZ = ((int)worldVoxelP.z / (int)cellSize.z);

    float t0 = SimplexNoise_fractal_1d(16, cellX, 10);
    t0 = mapSimplexNoiseTo11(t0);
    int xOffset = (int)lerp(-0.5f*marginInVoxels, 0.5f*marginInVoxels, make_lerpTValue(t0));

    float t1 = SimplexNoise_fractal_1d(16, cellZ, 10);
    t1 = mapSimplexNoiseTo11(t1);
    int zOffset = (int)lerp(-0.5f*marginInVoxels, 0.5f*marginInVoxels, make_lerpTValue(t1));

    float3 cellTargetP = make_float3(xOffset, 0, zOffset);
    int remainderX_centerBased = (worldVoxelP.x - (cellX * cellSize.x)) - (int)(0.5f*cellSize.x);
    int remainderZ_centerBased = (worldVoxelP.z - (cellZ * cellSize.z)) - (int)(0.5f*cellSize.z);

    if(remainderX_centerBased == (int)cellTargetP.x && remainderZ_centerBased == (int)cellTargetP.z) {
        result = true;
    }
    // t0 = mapSimplexNoiseTo01(t0);
    // return t0 < 0.5f;

    return result;
}

bool isIronLocation(int worldX, int worldY, int worldZ) {
    //NOTE: Bigger frequency noise overlay
    float t0 = SimplexNoise_fractal_3d(16, worldX, worldY, worldZ, 0.007143);
    t0 = mapSimplexNoiseTo01(t0);
    
    //NOTE: Smaller frequency for individual iron blocks 
    float t = SimplexNoise_fractal_3d(16, worldX, worldY, worldZ, 0.128572);
    t = mapSimplexNoiseTo01(t);

    t = t * t0;
    return t < 0.135714;
}

bool isCoalLocation(int worldX, int worldY, int worldZ) {
    int coalOffset = 64;
    //NOTE: Bigger frequency noise overlay
    float t0 = SimplexNoise_fractal_3d(16, worldX + coalOffset, worldY + coalOffset, worldZ + coalOffset, 0.007143);
    t0 = mapSimplexNoiseTo01(t0);
    
    //NOTE: Smaller frequency for individual iron blocks 
    float t = SimplexNoise_fractal_3d(16, worldX, worldY, worldZ, 0.128572);
    t = mapSimplexNoiseTo01(t);

    t = t * t0;
    return t < 0.135714;
}