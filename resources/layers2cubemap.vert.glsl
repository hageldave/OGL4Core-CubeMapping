#version 330

layout(location = 0) in vec2  in_position;

uniform mat4 projMX;

out vec2 texCoords_;

/* vertex shader for drawing a quad which is used to transfer the
 * contents of a 2D texture array to a cubemap FBO.
 */
void main() {
	gl_Position = projMX * vec4(in_position,0,1);
	texCoords_ = in_position;
}

