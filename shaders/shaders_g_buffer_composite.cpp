
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
"uniform sampler2D normalMap;"
"out vec4 color;"
"void main() {"
    "vec4 diffSample = texture(diffuse, uv_frag);"
    "if(diffSample.w == 0) {"
        "discard;"
    "}"
    "color = diffSample*color_frag;"
"}";
