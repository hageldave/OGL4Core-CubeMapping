#version 330

#define M_PI 3.1415

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 picking_color;

uniform mat4 invViewMX;
uniform bool useTexture;
uniform samplerCube tex;
uniform vec3 pickColor;
uniform vec3 lightDir;
uniform float k_exp;
uniform vec3 cubeCenterWorldCoords;
uniform float parallaxCorrectionFactor;
uniform int warpFN;

in vec3 worldCoords;
in vec2 faceCoords;
in vec3 normal;
flat in uint permMXidx;

mat3 permMX;
vec3 texCoords;

const float k_amb  = 0.6;
const float k_spec = 0.15;
const float k_diff = 0.25;
const vec3 ambient = vec3(1,1,1);
const vec3 diffuse = vec3(1,1,1);
const vec3 specular= vec3(1,1,1);
const float theta = 0.8687;
const mat3 permMatrices[6] = mat3[6](
            mat3( 1,0,0,   0,1,0,    0,  0, .5 ), // front
            mat3(-1,0,0,   0,1,0,    0,  0,-.5 ), // back
            mat3( 0,0,-1,  0,1,0,   .5,  0,  0 ), // left
            mat3( 0,0,1,   0,1,0,  -.5,  0,  0 ), // right
            mat3( 1,0,0,   0,0,-1,   0, .5,  0 ), // top
            mat3( 1,0,0,   0,0,1,    0,-.5,  0 )  // bottom
            );

vec2 truncateVec(vec2 v) {
        return vec2(int(v.x), int(v.y));
}

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

vec2 warp(vec2 pos){
	switch(warpFN){
		case 1:{
			float divByTheta = 1/theta;
			float tanTheata = tan(theta);
			float u = divByTheta*atan(pos.x*tanTheata);
			float v = divByTheta*atan(pos.y*tanTheata);
			return vec2(u,v);
		}
		case 2:{
			float u = cobe(pos.x,pos.y);
			float v = cobe(pos.y,pos.x);
			return vec2(u,v);
		}
		case 0:
		default:
			return pos;
	}
}

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

void main() {
	vec2 uv = 0.5*warp(2*faceCoords);
	permMX = permMatrices[permMXidx];
	texCoords = permMX*vec3(uv,1);
	
   // calculate lighting
   vec4 camPos = invViewMX * vec4(0, 0, 0, 1);
   vec3 observerDir = normalize(camPos.xyz - worldCoords);
   vec3 phong = blinnPhong(normalize(normal), -lightDir, observerDir);
   // calculate color (reflection)
   vec3 color = vec3(0,0,0);
   if(useTexture){
      vec3 R = reflect(-observerDir, normalize(normal));
      vec3 center2surfpoint = worldCoords - cubeCenterWorldCoords;
      R = R + parallaxCorrectionFactor*center2surfpoint;
      vec4 texColor = texture(tex, R);
      color = texColor.rgb;
   } else {
      vec2 checker = truncateVec(10 * (uv+vec2(.5,.5)));
      if(int(checker.x + checker.y) % 2 == 0) {
         color = vec3(0.3, 0.3, 0.3);
      } else {
         color = 1 - vec3(0.3, 0.3, 0.3);
      }
   }
   frag_color = vec4(color*phong,1);
   picking_color = vec4(pickColor,1);
}