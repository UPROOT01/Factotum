#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPos;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform UniformBufferObject {
   mat4 gVP;
   vec3 camWorldPos;
   float gridSize;
} ubo;

vec4 gridColorThick = vec4(1.0, 1.0, 1.0, 0.);

void main() {
  float zMod = mod(fragPos.z + ubo.gridSize/2., ubo.gridSize);
  float xMod = mod(fragPos.x + ubo.gridSize/2., ubo.gridSize);

  vec4 color = gridColorThick;
  if(zMod <= .01 || xMod <= .01){
    color.a = 1.0;
  }

  outColor = color;
}
