#version 330

layout(location = 0) out vec4 frag_color;

uniform bool useTexture;
uniform samplerCube tex;

in vec2 faceCoords;
in vec3 texCoords;

/* Truncates the components of the vec to their integer values. */
vec2 truncateVec(vec2 v) {
        return vec2(int(v.x), int(v.y));
}

/* Fragment shader for drawing a cube for a skybox.
 * This either textures the cubefaces using a cubemap texture
 * or a procedural checkerboard pattern.
 */
void main() {
   vec3 color = vec3(0,0,0);
   if(useTexture){
      color = texture(tex, texCoords).rgb;
   } else {
      vec2 checker = truncateVec(10 * faceCoords);
      if(int(checker.x + checker.y) % 2 == 0) {
         color = vec3(0.3, 0.3, 0.3);
      } else {
         color = 1 - vec3(0.3, 0.3, 0.3);
      }
   }
   frag_color = vec4(color,1);
}
