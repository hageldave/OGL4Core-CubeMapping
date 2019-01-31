#version 330

layout(location = 0) in vec2  cornerVert;
layout(location = 1) in uint permMXidx;

out uint permMXidx_;

/* Veretex shader for rendering a cube or sphere in the scene.
 * This simply forwards the inputs to the geometry shader.
 */
void main() {
	gl_Position = vec4(cornerVert,0,0);
	permMXidx_ = permMXidx;
}
