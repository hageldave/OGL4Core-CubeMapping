#version 330

layout(location = 0) in vec2  in_position;
layout(location = 1) in vec3  pmRowX;
layout(location = 2) in vec3  pmRowY;
layout(location = 3) in vec3  pmRowZ;

uniform mat4 modelMX;

out vec2 faceCoords_;
out vec3 texCoords_;

/* Transforms the current 2D face coordinate to 3D cube coordinate.
 * (multiplication with permutation matrix)
 */
vec3 get3DPos() {
	vec3 coord = vec3(in_position,1);
	return vec3( 
		dot(pmRowX,coord),
		dot(pmRowY,coord),
		dot(pmRowZ,coord)
	);
}

/* vertex shader for drawing a cube for a skybox.
 * The input vertices are 2D face coordinates ( in [-0.5,0.5]^2 )
 * which are then transformed to their actual cube position using
 * the corresponding permutation matrix (split up into row vectors as input).
 */
void main() {
	vec3 pos = get3DPos();
	gl_Position = modelMX * vec4(pos,1);
	faceCoords_ = in_position+vec2(.5,.5);
	texCoords_ = pos;
}
