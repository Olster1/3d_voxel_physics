
static char *compositeTextureVertexShader = 
"#version 330\n"
//per vertex variables
"in vec3 vertex;"
"in vec3 normal;"
"in vec2 texUV;	"

//per instanced variables
"in mat4 M;"
"in vec4 uvAtlas;"
"in vec4 color;"

//outgoing variables
"out vec4 color_frag;"
"out vec2 uv_frag;"

"void main() {"
    "gl_Position = M * vec4((vertex), 1);"
    "color_frag = color;"
    "uv_frag = vec2(texUV.x, -1*texUV.y);"
"}";

static char *compositeTextureFragShader = 
"#version 330\n"
"in vec4 color_frag;" 
"in vec2 uv_frag; "
"uniform sampler2D diffuse;"
"uniform sampler2D normal;"
"uniform sampler2D material;"
"uniform sampler2D motion;"
"uniform sampler2D worldPosition;"
"uniform sampler2D hyperbolicDepth;"
"out vec4 color;"
"vec3 sunAngle = " TOSTR(SUN_DIRECTION) ";" // Your normalized light direction
"uniform usampler3D voxels;"
"uniform vec3 AABB_min_metres;" 
"uniform vec3 AABB_max_metres;"

"float sampleShadowVoxels(vec3 ro, vec3 rd, vec3 normal) {"
    "int mipLevel = 0;" 
    "ivec3 size = textureSize(voxels, mipLevel);"
    "int maxSearchDist = 256;"
    "float normalBias = 0.05;" // Lift up away from the geometry skin
    
    "vec3 voxels_per_unit = vec3(size) / (AABB_max_metres - AABB_min_metres);"

    // 1. First lift it vertically off the surface, then push it slightly along the sun ray
    "vec3 currentWorldP = ro + (normalize(normal) * normalBias) + (rd);"
    
    // Scale the ray direction so one step in world space equals roughly one voxel unit step
    // This stops the ray from stepping too fast or slow depending on voxel density
    "float min_voxel_size = 1.0 / max(voxels_per_unit.x, max(voxels_per_unit.y, voxels_per_unit.z));"
    "vec3 stepVector = rd * min_voxel_size;"

    "for(int i = 0; i < maxSearchDist; ++i) {"
        // Convert current world position to voxel texture coordinates
        "vec3 voxelSpaceP = (currentWorldP - AABB_min_metres) * voxels_per_unit;"
        "ivec3 pos = ivec3(floor(voxelSpaceP));"

        // Out of bounds check against the voxel grid dimensions
        "if(pos.x < 0 || pos.x >= size.x || pos.y < 0 || pos.y >= size.y || pos.z < 0 || pos.z >= size.z) {"
            "return 1.0;" // Exited voxel bounds, sky is visible -> light
        "}"

        // Use texelFetch for perfect pixel-accurate integer voxel lookup
        "uint intVal = texelFetch(voxels, pos, 0).r;"
        "if(intVal > 0u) {"
            "return 0.4;" // Hit a voxel -> shadow
        "}"
        
        // Step forward along the sun ray
        "currentWorldP += stepVector;"
    "}"
    
    "return 1.0;"
"}"

"void main() {"
    "float depth = texture(hyperbolicDepth, uv_frag).r;"
    "vec3 worldP = texture(worldPosition, uv_frag).rgb;"
    "depth = pow(depth, 20.0); "
    "vec4 albedo = texture(diffuse, uv_frag);"
    "vec3 worldNormal = texture(normal, uv_frag).xyz;"
    
    "float shadowValue = sampleShadowVoxels(worldP, sunAngle, worldNormal);"

    "float mixValue = max(dot(normalize(worldNormal), normalize(sunAngle)), 0.4);"
    
    "color = vec4(mixValue * shadowValue * albedo.xyz, 1);"
"}";