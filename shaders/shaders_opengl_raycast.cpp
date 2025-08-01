
static char *voxelEntityRaycastVertexShader = 
"#version 330\n"
//per vertex variables
"in vec3 vertex;"
"in vec3 normal;"
"in vec2 texUV;"

//per instanced variables
"in vec4 color;"
"in mat4 M;"

//uniform variables
"uniform mat4 V;"
"uniform mat4 projection;"

//outgoing variables
"out vec4 color_frag;"
"out vec3 sunAngle;"
"out mat4 inverseModelT;"
"out mat4 modelT;"
"out mat4 invViewMatrix;"
"out mat4 projectionMatrix;"
"out mat4 cameraT;"
"out mat4 PVM;"

"mat4 removeScale(mat4 m) {"
    "vec3 scale;"
    "scale.x = length(m[0].xyz);"
    "scale.y = length(m[1].xyz);"
    "scale.z = length(m[2].xyz);"

    "mat4 result = m;"
    "result[0].xyz /= scale.x;"
    "result[1].xyz /= scale.y;"
    "result[2].xyz /= scale.z;"

    "return result;"
"}"

"void main() {"
    "gl_Position = projection * V * M * vec4((vertex), 1);"
    "color_frag = color;"
    "sunAngle = mat3(V) * vec3(0.17364817766, 0.98480775301, 0);"
    "mat4 scaleRemovedM = removeScale(M);"
    "inverseModelT = inverse(scaleRemovedM);"
    "invViewMatrix = inverse(V);"
    "modelT = scaleRemovedM;"
    "PVM = projection * V * scaleRemovedM;"
    "cameraT = V;"
    "projectionMatrix = projection;"
"}";

static char *voxelEntityRaycastFragShader = 
"#version 330\n"
"in vec4 color_frag;" 
"in vec2 uv_frag; "
"in vec3 sunAngle;"
"in mat4 inverseModelT;"
"in mat4 modelT;"
"in mat4 cameraT;"
"in mat4 PVM;"
"uniform sampler2D diffuse;"
"uniform vec3 lookingAxis;"
"uniform int numPalettes;"
"uniform usampler3D voxels;"
"uniform int palleteIndex;"
"uniform vec3 cameraP;"

"uniform vec2 projectionPlaneSize;" // Size of the projection plane 1metre away from the camera
"in mat4 invViewMatrix;"        // Inverse of your camera's view matrix
"uniform vec2 viewportSize;"         // (width, height)
"in mat4 projectionMatrix;" 

"uniform vec3 AABB_min_metres;"
"uniform vec3 AABB_max_metres;"

"layout(location = 0) out vec4 color;"
"layout(location = 1) out vec4 normalOut;"
"layout(location = 2) out vec4 materialOut;"
"layout(location = 3) out vec4 motionOut;"
"layout(location = 4) out vec4 linearDepthOut;"

"struct Ray {"
    "vec3 origin;"
    "vec3 direction;"
"};"

"Ray getAABBModelRay() {"
    "vec2 projectionPlanePoint = ((gl_FragCoord.xy / viewportSize) - vec2(0.5))*projectionPlaneSize;"

    "vec3 rayInCameraSpace = vec3(projectionPlanePoint, 1);"

    "vec3 rayDirWorld = normalize((invViewMatrix * vec4(rayInCameraSpace.xyz, 0.0)).xyz);"

    "vec3 rayDirModel = normalize((inverseModelT * vec4(rayDirWorld, 0.0)).xyz);"

    "vec3 rayOriginModel = (inverseModelT * vec4(cameraP, 1.0)).xyz;"

    "Ray ray;"
    "ray.origin = rayOriginModel;"
    "ray.direction = rayDirModel;"
    "return ray;"
"}"

"float safeDivideFloat(float a, float v) {"
    "return v != 0.0 ? a / v : 1e30;"
"}"

"struct AABBResult {"
    "float tMin;"
    "int axis;"
"};"

"AABBResult ray_aabb(vec3 minBoundingCorner, vec3 maxBoundingCorner, vec3 ro, vec3 rd) {"
    "float tmin = 0;"
    "float tmax = 1e30f;"
    "AABBResult result;"
    "result.tMin = 1e30f;"

    "for (int axis = 0; axis < 3; ++axis) {"
        "float t1 = safeDivideFloat((minBoundingCorner[axis] - ro[axis]), rd[axis]);"
        "float t2 = safeDivideFloat((maxBoundingCorner[axis] - ro[axis]), rd[axis]);"

        "float dmin = min(t1, t2);"
        "float dmax = max(t1, t2);"

        "if(dmin > tmin) {"
            "result.axis = axis;"
            "tmin = dmin;"
        "}"
        "tmax = min(dmax, tmax);"
    "}"

    "if (tmax >= tmin) {"
        "result.tMin = tmin;"
        "return result;"
    "}"
    "return result;" //NOTE: Miss
"}"

"int getSignOfFloat(float value) {"
    "if(value < 0) {"
        "return -1;"
    "}"
    "if(value > 0) {"
        "return 1;"
    "}"
    "return 0;"
"}"

"vec3 safeInverse(vec3 v) {"
    "return vec3("
        "v.x != 0.0 ? 1.0 / v.x : 1e30,"
        "v.y != 0.0 ? 1.0 / v.y : 1e30,"
        "v.z != 0.0 ? 1.0 / v.z : 1e30"
    ");"
"}"

"vec3 safeDivide(vec3 a, vec3 v) {"
    "return vec3("
        "v.x != 0.0 ? a.x / v.x : 1e30,"
        "v.y != 0.0 ? a.y / v.y : 1e30,"
        "v.z != 0.0 ? a.z / v.z : 1e30"
    ");"
"}"


"vec4 getColor(int index) {"
    "float x = (float(index) + 0.5) / 512.0;"
    "float y = (float(palleteIndex) + 0.5) / float(numPalettes);"
    "return texture(diffuse, vec2(x, y));"
"}"

"struct VoxelInfo {"
    "vec4 color;"
    "vec3 normal;"
    "vec4 material;"
"};"

"VoxelInfo rayCastVoxelGrid(vec3 AABB_min_metres, vec3 AABB_max_metres) {"
    "Ray ray = getAABBModelRay();"
    "VoxelInfo result;"
    "result.color = vec4(0, 0, 0, 1);"
    
    "AABBResult aabbResult = ray_aabb(AABB_min_metres, AABB_max_metres, ray.origin, ray.direction);"
    "if(aabbResult.tMin == 1e30f) {"
        "discard;"
    "}"

    "int mipLevel = 0;"
    "ivec3 size = textureSize(voxels, mipLevel);"
    "float nudge = 0.0001;"
    "vec3 voxels_per_unit = size / (AABB_max_metres - AABB_min_metres);"

    "vec3 entryP = ((ray.origin + (aabbResult.tMin + nudge)*ray.direction) - AABB_min_metres) * voxels_per_unit;"
    "ivec3 step = ivec3(getSignOfFloat(ray.direction.x), getSignOfFloat(ray.direction.y), getSignOfFloat(ray.direction.z));"
    "vec3 delta = abs(safeInverse(ray.direction));"
    "ivec3 pos = clamp(ivec3(floor(entryP)), ivec3(0, 0, 0), size);"
    "vec3 tmax = safeDivide((vec3(pos) + max(step, 0) -  entryP), ray.direction);"

    "int maxSteps = 128;"
    "int axis = aabbResult.axis;"
    "for(int i = 0; i < maxSteps; ++i) {"
        "vec3 texCoord = vec3(pos) / vec3(size);"
        "int intVal = int(texture(voxels, texCoord).r);"
        "if(intVal > 0) {"
            "vec4 color = getColor(intVal);"
            "result.color = color;"
            "vec4 normal = vec4(0);"
            "normal[axis] = -1*step[axis];"
            "result.normal =  (modelT * normalize(normal)).xyz;"
            "float finalHitTime = aabbResult.tMin;"
             "if(i > 0) {"
                "finalHitTime = aabbResult.tMin + ((tmax[axis] - delta[axis]) / voxels_per_unit[axis]);"
            "}"
            
            "vec3 hitPos_modelSpace = ray.origin + finalHitTime*ray.direction;"
           
            "vec4 hyperbolicDepth =  (PVM*vec4(hitPos_modelSpace, 1));"
            "hyperbolicDepth.z /= hyperbolicDepth.w;"
            "gl_FragDepth = hyperbolicDepth.z * 0.5 + 0.5;"
            "return result;"
        "}"

        "if (tmax.x < tmax.y) {"
            "if (tmax.x < tmax.z) {"
                "pos.x += step.x;"
                "if (pos.x < 0 || pos.x >= size.x) break;"
                "axis = 0;"
                "tmax.x += delta.x;"
            "} else {"
                "pos.z += step.z;"
                "if (pos.z < 0 || pos.z >= size.z) break;"
                "axis = 2;"
                "tmax.z += delta.z;"
            "}"
        "} else {"
            "if (tmax.y < tmax.z) {"
                "pos.y += step.y;"
                "if (pos.y < 0 || pos.y >= size.y) break;"
                "axis = 1;"
                "tmax.y += delta.y;"
            "} else {"
                "pos.z += step.z;"
                "if (pos.z < 0 || pos.z >= size.z) break;"
                "axis = 2;"
                "tmax.z += delta.z;"
            "}"
        "}"

    "}"
    "discard;"
  
"}"

"void main() {"
    "VoxelInfo voxelInfo = rayCastVoxelGrid(AABB_min_metres, AABB_max_metres);"
    "color = vec4(voxelInfo.color.xyz, 1);"
    "normalOut = vec4(voxelInfo.normal, 1);"
    "materialOut = voxelInfo.material;"
    "motionOut = vec4(1, 0, 0, 1);"
"}";


