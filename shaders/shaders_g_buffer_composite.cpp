
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
"vec3 sunAngle = vec3(0.17364817766, 0.98480775301, 0);"
"uniform usampler3D voxels;"
"uniform vec3 AABB_min_metres;" //NOTE: Size of the map in meteres
"uniform vec3 AABB_max_metres;"

"float sampleShadowVoxels(vec3 ro, vec3 rd) {"
    "int mipLevel = 0;" 
    "ivec3 size = textureSize(voxels, mipLevel);"
    "int maxSearchDist = 128;"
    "float nudge = 0.0001;"
    "vec3 voxels_per_unit = size / (AABB_max_metres - AABB_min_metres);"

    "vec3 entryP = ((ro + nudge*rd) - AABB_min_metres) * voxels_per_unit;"
    "vec3 at = entryP;"
    "for(int i = 0; i < maxSearchDist; ++i) {"
        "ivec3 pos = clamp(ivec3(floor(entryP)), ivec3(0, 0, 0), size);"
        "vec3 texCoord = vec3(pos) / vec3(size);"
        "int intVal = int(texture(voxels, texCoord).r);"
        "if(intVal > 0) {"
            "return 0.4;"
        "}"
        "entryP += rd;"
        "if(entryP.x < 0 || entryP.x >= size.x) {"
            "return 1;"
        "}"
        "if(entryP.y < 0 || entryP.y >= size.y) {"
            "return 1;"
        "}"
        "if(entryP.z < 0 || entryP.z >= size.z) {"
            "return 1;"
        "}"
        
    "}"
    
    "return 1;"
"}"

"void main() {"
    "float depth = texture(hyperbolicDepth, uv_frag).r;"
    "vec3 worldP = texture(worldPosition, uv_frag).rgb;"
    "depth = pow(depth, 20.0); "
    "vec4 albedo = texture(diffuse, uv_frag);"
    "vec3 worldNormal = texture(normal, uv_frag).xyz;"
    "float mixValue = max(dot(worldNormal, sunAngle), 0.4);"
    "vec3 mappedColor = (worldNormal.xyz + vec3(1.0)) * 0.5;"
    // "color = vec4(mappedColor, 1);"
    // "color = vec4(depth, depth, depth, 1);"
    // "color = vec4(worldP, 1);"
    "float shadowValue = sampleShadowVoxels(worldP, worldNormal);"
    "color = vec4(shadowValue*mixValue*albedo.xyz, 1);"
"}";
