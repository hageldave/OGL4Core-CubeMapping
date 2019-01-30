#version 330

layout(points) in;
/* we have a total of 14 (including gl_position and gl_Layer) components per vertex
 * this means that the minimum number of vertices that can be generated
 * is 73 (with respect to the specification).
 * GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS is at least 1024
 * from which follows: max_vertices = 1024/14 = 73.1
 * of course it may be larger when the hardware has a higher limit
 * on GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS
 */
layout(triangle_strip,max_vertices=73) out;

uniform mat4 modelMX;
uniform mat4 viewMX;
uniform mat4 projMX;
uniform bool doSphereProjection;
uniform int subDivisionLevel;
uniform float totalQuadSize;

flat out uint permMXidx;
out vec3 worldCoords;
out vec2 faceCoords;
out vec3 normal;


in uint permMXidx_[];

const mat3 permMatrices[6] = mat3[6](
            mat3( 1,0,0,   0,1,0,    0,  0, .5 ), // front
            mat3(-1,0,0,   0,1,0,    0,  0,-.5 ), // back
            mat3( 0,0,-1,  0,1,0,   .5,  0,  0 ), // left
            mat3( 0,0,1,   0,1,0,  -.5,  0,  0 ), // right
            mat3( 1,0,0,   0,0,-1,   0, .5,  0 ), // top
            mat3( 1,0,0,   0,0,1,    0,-.5,  0 )  // bottom
            );
            
mat3 permMX;


void makeVertex(vec2 facePos, mat4 vpMX) {
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
    gl_Layer = 0;
    permMXidx = permMXidx_[0];
}

void main() {
    vec3 pos = vec3(0);
    vec2 facePos = vec2(0);
    permMX = permMatrices[permMXidx_[0]];

    mat4 vpMX = projMX*viewMX;

    float subQuadSize = totalQuadSize/subDivisionLevel;
    for(int ix = 0; ix < subDivisionLevel; ix++){
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
