#version 330

layout(location = 0) out vec4 frag_color;

uniform int useTexture;
uniform sampler2DArray tex;

in vec2 texCoords;

/* Quad fragment shader for drawing window filling quad that
 * displays rendered testure from FBO.
 * The texture is a texture array with the first layer containing
 * the image rendered from the camera, other 6 layers contain the
 * images rendered from each of the cubefaces of the reflecting object.
 */
void main() {
   if(useTexture != 0){
      vec4 texColor = texture(tex, vec3(texCoords,0));
      frag_color = texColor;
   } else {
      frag_color = vec4(texCoords,0,1);
   }
}

