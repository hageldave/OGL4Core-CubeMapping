#version 330
#extension GL_ARB_gpu_shader5 : require

layout(triangles, invocations = 6) in;
layout(triangle_strip, max_vertices = 3) out;

in vec2 texCoords_[];
out vec2 texCoords;
flat out uint texLayer;

/* geometry shader for drawing a quad which is used to transfer the
 * contents of a 2D texture array to a cubemap FBO.
 * This GS will be instanced 6 times for each of the cube faces.
 * The layers to be transferred start at the second layer 
 * (first layer does not correspond to the cubemap)
 */
void main() {
	for(int i = 0; i < 3; i++){
		gl_Position = gl_in[i].gl_Position;
		gl_Layer = gl_InvocationID;
		texLayer = gl_InvocationID+1; // starting at second layer of tex array
		texCoords = texCoords_[i];
		EmitVertex();
	}
	EndPrimitive();
}

