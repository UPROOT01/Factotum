#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0, set = 0) uniform UniformBufferObject {
   mat4 gVP;
   vec3 camWorldPos;
   float gridSize;
} ubo;


const vec3 Pos[4] = vec3[4](
    vec3(-100.0, .5, -100.0),
    vec3(100.0, .5, -100.0),
    vec3(100.0, .5, 100.0),
    vec3(-100.0, .5, 100.0)
);


const int Indices[6] = int[6](0, 2, 1, 2, 0, 3);

layout(location = 0) out vec3 fragPos;

void main() {
   int Index = Indices[gl_VertexIndex];
   vec4 vPos = vec4(Pos[Index], 1.0);

   vPos.x += ubo.camWorldPos.x;
   vPos.z += ubo.camWorldPos.z;

   gl_Position = ubo.gVP * vPos;
   fragPos = vPos.xyz;
}
