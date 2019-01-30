#version 330
#extension GL_ARB_gpu_shader5 : require

layout(triangles, invocations = 7) in;
layout(triangle_strip,max_vertices=3) out;

uniform mat4 viewMX;
uniform mat4 projMX;
uniform mat4 boxProjMX;

out vec2 faceCoords;
out vec3 texCoords;

in vec2 faceCoords_[];
in vec3 texCoords_[];

/* view matrices for pointing the camera to each of a cube's faces (from the center of the cube)
 */
const mat4 boxViewMX[6] = mat4[6](
    mat4(vec4( 0, 0,-1,0),vec4(0, 1, 0,0),vec4( 1, 0, 0,0),vec4(0,0,0,1)), // y+90 (POSX)
    mat4(vec4( 0, 0, 1,0),vec4(0, 1, 0,0),vec4(-1, 0, 0,0),vec4(0,0,0,1)), // y-90 (NEGX)
    mat4(vec4(-1, 0, 0,0),vec4(0, 0,-1,0),vec4( 0,-1, 0,0),vec4(0,0,0,1)), // x-90 (POSY)
    mat4(vec4(-1, 0, 0,0),vec4(0, 0, 1,0),vec4( 0, 1, 0,0),vec4(0,0,0,1)), // x+90 (NEGY)
    mat4(vec4(-1, 0, 0,0),vec4(0, 1, 0,0),vec4( 0, 0,-1,0),vec4(0,0,0,1)), // y180 (POSZ)
    mat4(vec4( 1, 0, 0,0),vec4(0, 1, 0,0),vec4( 0, 0, 1,0),vec4(0,0,0,1))  // 0    (NEGZ)
);

/* Geometry shader for drawing a cube for a skybox.
 * This stage performs the view transformation and projection and is instanced
 * 7 times. The first instantiation is for the actual rendering from perspective
 * of the camera. The following 6 instances are for rendering from the perspective
 * of the reflecting object onto each of the 6 cube faces.
 * This realizes a layered rendering approach.
 */
void main() {
    mat4 vpMX;
    if(gl_InvocationID == 0){
		// camera transform and screen view frustum projection
        vpMX = projMX*viewMX;
    } else {
		// cubemap transform 90° view frustum projection onto cube face
        vpMX = boxProjMX*boxViewMX[gl_InvocationID-1];
    }
    // for each vertex of triangle perform projection and emit a vertex
    for(int i = 0; i < 3; i++){
        texCoords = texCoords_[i];
        faceCoords = faceCoords_[i];
        gl_Position = vpMX*gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}
