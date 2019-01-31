#version 330
#extension GL_ARB_gpu_shader5 : require

layout(points, invocations = 7) in;
/* we have a total of 14 (including gl_position and gl_Layer) components per vertex
 * this means that the minimum number of vertices that can be generated
 * is 73 (with respect to the specification).
 * GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS is at least 1024
 * from which follows: max_vertices = 1024/14 = 73.1
 * of course it may be larger when the hardware has a higher limit
 * on GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS
 */
layout(triangle_strip,max_vertices=73) out;

uniform bool doSphereProjection;
uniform int subDivisionLevel;
uniform float totalQuadSize;
uniform mat4 modelMX;
uniform mat4 viewMX;
uniform mat4 projMX;
uniform mat4 boxProjMX;
uniform mat4 boxTransMX;

flat out uint permMXidx;
out vec3 worldCoords;
out vec2 faceCoords;
out vec3 normal;

in uint permMXidx_[];

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
			
/* Permutation matrices for transforming a 2D cube face vertex to its actual 3D position of the cube.
 * Each matrix transforms to another face of the cube. 
 */
const mat3 permMatrices[6] = mat3[6](
			mat3( 1,0,0,   0,1,0,    0,  0, .5 ), // front
			mat3(-1,0,0,   0,1,0,    0,  0,-.5 ), // back
			mat3( 0,0,-1,  0,1,0,   .5,  0,  0 ), // left
			mat3( 0,0,1,   0,1,0,  -.5,  0,  0 ), // right
			mat3( 1,0,0,   0,0,-1,   0, .5,  0 ), // top
			mat3( 1,0,0,   0,0,1,    0,-.5,  0 )  // bottom
			);
			
mat3 permMX;

/* Creates a vertex ready to be emmited from the specified 2D position, 
 * the specified viewprojection matrix and the currently set permutation matrix.
 */
void makeVertex(vec2 facePos, mat4 vpMX) {
	// transform to 3D cube position
	vec3 pos = permMX*vec3(facePos,1);
	faceCoords = facePos;
	if(doSphereProjection){
		normal = normalize(pos);
		worldCoords = (modelMX * vec4(normal*0.69,1)).xyz;
	} else {
		normal = permMX*vec3(0,0,1);
		worldCoords = (modelMX * vec4(pos,1)).xyz;
	}
	gl_Position = vpMX * vec4(worldCoords,1);
	gl_Layer = gl_InvocationID;
	permMXidx = permMXidx_[0];
}


/* Geometry shader for rendering a cube or sphere in the scene.
 * This GS is instanced 7 times, the first instance rendering from the
 * perspective of the camera, the other 6 rendering from the perspective of
 * the cube center in direction of each of its faces.
 * This realizes a layered rendering approach where each instance
 * sends the primitives to a different layer.
 * 
 * Also this GS introduces new vertices to refine the geometry of the
 * quads defined by the inputs. The input position defines the 2D coordinate
 * of the bottom left corner of the current quad and the uniform totalQuadSize
 * determines the width and height of the quad originating from that point.
 * The quad spanned by this parameters will be covered with columns of
 * triangle strips to equally sample the area.
 * The uniform subDivisionLevel determines how many triangle strip columns
 * are created and how many "small" quads a trianglestrip contains.
 */
void main() {
	// set the permutation matrix corresponding to the current input's permutation matrix index
	permMX = permMatrices[permMXidx_[0]];

	mat4 vpMX;
	if(gl_InvocationID == 0){
		// camera transform and screen view frustum projection
		vpMX = projMX*viewMX;
	} else {
		// cubemap transform 90° view frustum projection onto cube face
		vpMX = boxProjMX*boxViewMX[gl_InvocationID-1]*boxTransMX;
	}

	// create the triangle strip
	float subQuadSize = totalQuadSize/subDivisionLevel;
	for(int ix = 0; ix < subDivisionLevel; ix++){
		// define the coords of the first quad
		float x0 = ix*subQuadSize + gl_in[0].gl_Position.x;
		float x1 = x0+subQuadSize;
		float y0 = 0*subQuadSize + gl_in[0].gl_Position.y;
		float y1 = y0+subQuadSize;
		// bottom left
		makeVertex(vec2(x0,y0), vpMX);
		EmitVertex();
		// bottom_right
		makeVertex(vec2(x1,y0), vpMX);
		EmitVertex();
		// top_left
		makeVertex(vec2(x0,y1), vpMX);
		EmitVertex();
		// top_right
		makeVertex(vec2(x1,y1), vpMX);
		EmitVertex();
		// append more vertices to form a strip
		for(int iy = 1; iy < subDivisionLevel; iy++){
			y0 = iy*subQuadSize + gl_in[0].gl_Position.y;
			y1 = y0+subQuadSize;
			// top_left
			makeVertex(vec2(x0,y1), vpMX);
			EmitVertex();
			// top_right
			makeVertex(vec2(x1,y1), vpMX);
			EmitVertex();
		}
		EndPrimitive();
	}

}
