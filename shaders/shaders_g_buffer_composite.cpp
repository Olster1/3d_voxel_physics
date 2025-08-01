
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
"uniform sampler2D linearDepth;"
"uniform sampler2D hyperbolicDepth;"
"out vec4 color;"
"vec3 sunAngle = vec3(0.17364817766, 0.98480775301, 0);"
"void main() {"

    "float depth = texture(hyperbolicDepth, uv_frag).r;"
    "depth = pow(depth, 20.0); "
    "vec4 albedo = texture(diffuse, uv_frag);"
    "vec3 worldNormal = texture(normal, uv_frag).xyz;"
    "float mixValue = max(dot(worldNormal, sunAngle), 0.4);"
    "vec3 mappedColor = (worldNormal.xyz + vec3(1.0)) * 0.5;"
    // "color = vec4(mappedColor, 1);"
    // "color = vec4(depth, depth, depth, 1);"
    "color = vec4(mixValue*albedo.xyz, 1);"
"}";
