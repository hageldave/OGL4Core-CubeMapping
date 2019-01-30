#version 330

layout(location = 0) in vec4  in_position;

uniform mat4 modelMX;
uniform mat4 viewMX;
uniform mat4 projMX;

/* Shader for drawing a wireframe box.
 * The left back bottom vertex of the inputs is at (0,0,0)
 * the right front top vertex of the inputs is at (1,1,1)
 */
void main() {
    gl_Position = projMX * viewMX * modelMX * (in_position-vec4(0.5,0.5,0.5,0));
}
