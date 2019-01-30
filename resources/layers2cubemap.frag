#version 330

layout(location = 0) out vec4 frag_color;

uniform sampler2DArray tex;
in vec2 texCoords;
flat in uint texLayer;

/* fragment shader for drawing a quad which is used to transfer the
 * contents of a 2D texture array to a cubemap FBO.
 * This copies the content of the texture array at specified layer.
 */
void main() {
	// need to transfer image upside down because y-axis is flipped in FBO (compared to texture)
	frag_color = texture(tex, vec3(1-texCoords,texLayer));
}

