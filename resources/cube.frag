#version 330

#define M_PI 3.1415

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 picking_color;

uniform bool useTexture;
uniform int warpFN;
uniform float k_exp;
uniform samplerCube tex;
uniform vec3 pickColor;
uniform vec3 lightDir;
uniform mat4 invViewMX;

in vec3 worldCoords;
in vec2 faceCoords;
in vec3 normal;
flat in uint permMXidx;

/* lighting constants */
const float k_amb  = 0.6;
const float k_spec = 0.15;
const float k_diff = 0.25;
const vec3 ambient = vec3(1,1,1);
const vec3 diffuse = vec3(1,1,1);
const vec3 specular= vec3(1,1,1);

/* angle constant for tangent warp function */
const float theta = 0.8687;

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


/* Truncates the components of the vec to their integer values. */
vec2 truncateVec(vec2 v) {
   return vec2(int(v.x), int(v.y));
}

/* polynomial of COBE warp */
float cobe(float a, float b){
   float lambda  =  1.3774;
   float gamma01 = -0.2129;
   float gamma10 = -0.1178;
   float gamma20 =  0.0694;
   float gamma02 =  0.0108;
   float gamma11 =  0.0941;
   
   float a2 = a*a;
   float b2 = b*b;
   
   // begin calculation
   float sum = 0;
   sum += lambda*a;
   sum += (1-lambda)*a2*a;
   float f = (1-a2)*a;
   sum += f*gamma01*b2;
   sum += f*gamma10*a2;
   sum += f*gamma11*a2*b2;
   sum += f*gamma02*b2*b2;
   sum += f*gamma20*a2*a2;
   return sum;
}

/* warps 2D vector ( in [-1,1]^2 ) according to selected function */
vec2 warp(vec2 pos){
   switch(warpFN){
      case 1:{ // tangent warp function
         float divByTheta = 1/theta;
         float tanTheata = tan(theta);
         float u = divByTheta*atan(pos.x*tanTheata);
         float v = divByTheta*atan(pos.y*tanTheata);
         return vec2(u,v);
      }
      case 2:{ // COBE warp function (from Cosmic Background Explorer)
         float u = cobe(pos.x,pos.y);
         float v = cobe(pos.y,pos.x);
         return vec2(u,v);
      }
      case 0:  // fall through
      default: // identity warp function
         return pos;
   }
}

/* the usual blinn phong shading depending on surface nornal n, 
 * direction to light source l and observer direction v.
 */
vec3 blinnPhong(vec3 n, vec3 l, vec3 v) {
   float diffuseReflect = max(0.0, dot(n, l));
   vec3 h = normalize(v + l);
   float specularReflect = (k_exp + 2)* pow(max(0.0, dot(h, n)), k_exp) / (2 * M_PI);
   vec3 color =
        (ambient * k_amb)
      + (diffuse * k_diff * diffuseReflect)
      + (specular* k_spec * specularReflect);
   return color;
}

/* Fragment shader for rendering a cube or sphere in the scene.
 * This FS warps the texture coordinates, textures the fragment, 
 * calculates blinn phong shding and sets the picking color.
 */
void main() {
   // warp face coordinates before using them
   vec2 uv = 0.5*warp(2*faceCoords);
	
	// next up: determine color from texturing
   vec3 color = vec3(0,0,0);
   if(useTexture){
		// set the permutation matrix corresponding to the current permMXidx
		mat3 permMX = permMatrices[permMXidx];
		// get 3D cubemap texture coordinates
      vec3 texCoords = permMX*vec3(uv,1);
      color = texture(tex, texCoords).rgb;
   } else {
      vec2 checker = truncateVec(10 * (uv+vec2(.5,.5)));
      if(int(checker.x + checker.y) % 2 == 0) {
         color = vec3(0.3, 0.3, 0.3);
      } else {
         color = 1 - vec3(0.3, 0.3, 0.3);
      }
   }

   // next up: calculate lighting
   vec4 camPos = invViewMX * vec4(0, 0, 0, 1);
   vec3 observerDir = normalize(camPos.xyz - worldCoords);
   vec3 phong = blinnPhong(normalize(normal), -lightDir, observerDir);
   
   // set fragment color and object's picking id color
   frag_color = vec4(color*phong,1);
   picking_color = vec4(pickColor,1);
}
