// CubeMapping.cpp 
//

#include "CubeMapping.h"
#include "stdafx.h"
#include "GLHelpers.h"
#include "glm/gtc/constants.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "PngBitmapCodec.h"
#include "Defs.h"

// key codes
#ifdef __linux__
#define KEY_L 0x6C
#define KEY_S 0x73
#define KEY_R 0x72
#define KEY_G 0x67
#define KEY_B 0x62
#define KEY_A 0x61
#define KEY_F 0x66
#define KEY_X 0x78
#elif _WIN32
#define KEY_L 0x4C
#define KEY_S 0x53
#define KEY_R 0x52
#define KEY_G 0x47
#define KEY_B 0x42
#define KEY_A 0x41
#define KEY_F 0x46
#define KEY_X 0x58
#endif
#define KEY_0 0x30
#define KEY_1 0x31

/**
 * CubeMapping constructor
 */
CubeMapping::CubeMapping(COGL4CoreAPI *Api) : RenderPlugin(Api) {
	this->myName = "CubeMapping";
	this->myDescription = "cubemapping demo";
	camHandle = 0;
	wWidth = 300;
	wHeight = 300;
	aspect = 1.0;
	camHandle = 0;

	maxGeomOutVerts = 256;
	maxGeomTotalOutComp = 1024;
	cubeGeomMaxVerts = 73;

	texArrayColor = texArrayDepth = texArrayPicking = texReflectionCubeMap = texSky1 = texSky2 = texSky3 = texEarth = 0;
	fbo = fbo_layers2Cube = 0;

	pickedID = 0;
	pickingEnabled = false;
	ignoreObjectVarUpdate = false;
}

/**
 *  CubeMapping destructor
 */
CubeMapping::~CubeMapping() {
	Deactivate();
}

/**
 * is called when OGL4Core starts and decides whether a plugin
 * is available or not.
 * @return true when plugin will work, otherwise false
 */
bool CubeMapping::Init(void) {
	if (gl3wInit()) {
		std::fprintf(stderr, "Error: Failed to initialize gl3w.\n");
		return false;
	}
	return true;
}

/**
 * Called when selecting the plugin, sets up everything.
 */
bool CubeMapping::Activate(void) {
	// get path of plugin
	std::string pathName = this->GetCurrentPluginPath();
	// lets find out what geometry shader limitations we have before creating shaders and apivars
	glGetIntegerv(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS, &maxGeomTotalOutComp);
	glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES, &maxGeomOutVerts);
	cubeGeomMaxVerts = std::min(maxGeomTotalOutComp/14, maxGeomOutVerts);
	std::cout	<< "GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS=" << maxGeomTotalOutComp << std::endl
				<< "GL_MAX_GEOMETRY_OUTPUT_VERTICES=" << maxGeomOutVerts << std::endl;
	
	//--------------------//
	// interaction things //
	//--------------------//
	camHandle = this->AddManipulator("camera", &this->viewMX, Manipulator::MANIPULATOR_ORBIT_VIEW_3D);
	this->SelectCurrentManipulator(camHandle);
	this->SetManipulatorRotation(camHandle, glm::vec3(1,0,0), 50.0f);
	this->SetManipulatorDolly(camHandle, -10.0f );

	fovY.Set(this, "FoVy");
	fovY.Register();
	fovY.SetMinMax(5.0f, 120.0f);
	fovY = 45.0f;

	zNear.Set(this, "zNear");
	zNear.Register();
	zNear = 0.01f;

	zFar.Set(this,"zFar");
	zFar.Register();
	zFar  = 200.0f;

	EnumPair skyboxSelection[] = {{0,"checkerboard"},{1,"bridge"},{2,"space"},{3,"clouds"}};
	skyboxTexturing.Set(this,"skybox",skyboxSelection, 4);
	skyboxTexturing.Register();
	skyboxTexturing = 0;

	pickedIDVar.Set(this,"picked_obj", &CubeMapping::objectPicked);
	pickedIDVar.Register();
	pickedIDVar.SetMinMax(0,3);

	picked_x.Set(this,"x", &CubeMapping::pickedObjectMoved);
	picked_y.Set(this,"y", &CubeMapping::pickedObjectMoved);
	picked_z.Set(this,"z", &CubeMapping::pickedObjectMoved);
	picked_x.Register();
	picked_y.Register();
	picked_z.Register();
	picked_x.SetStep(0.01);
	picked_y.SetStep(0.01);
	picked_z.SetStep(0.01);

	cube_sphere_switch.Set(this,"sphere", &CubeMapping::pickedObjectModeChanged);
	cube_sphere_switch.Register();
	cube_sphere_switch = false;

	cube_texture_switch.Set(this, "texture", &CubeMapping::pickedObjectModeChanged);
	cube_texture_switch.Register();
	cube_texture_switch = false;

	EnumPair warpSelection[] = { { 0,"identity" },{ 1,"tangent" },{ 2,"COBE" } };
	cube_warpFN.Set(this, "warpFN", warpSelection, 3, &CubeMapping::pickedObjectModeChanged);
	cube_warpFN.Register();
	cube_warpFN = 0;

	pickedIDVar = 0;
	pickedIDVar.SetVisible(pickingEnabled);
	picked_x.SetVisible(pickingEnabled);
	picked_y.SetVisible(pickingEnabled);
	picked_z.SetVisible(pickingEnabled);
	cube_sphere_switch.SetVisible(pickingEnabled);
	cube_texture_switch.SetVisible(pickingEnabled);
	cube_warpFN.SetVisible(pickingEnabled);

	subDivLevel.Set(this, "subDivLvl");
	subDivLevel.Register();
	// set maximum sub division level, which is bounded by the
	// max geometry output vertices limit.
	// we use rows of triangle strips, thus each row has
	// 2+subdivlevel*2 vertices = 2*(1+subdivLevel)
	// we also have subdivlevel number of rows so all in all
	// numVerts = 2*(subDivLevel+subDivLevel^2)
	// how many subdivlevels can we have given max numer of vertices?
	// max/2 = x^2+x --> x^2+x-max/2 = 0 --> discr = 1+2*max (b^2-4ac)
	// x = (-1 + sqrt(discr))/2
	subDivLevel.SetMinMax(1, (std::sqrt(cubeGeomMaxVerts*2.0+1)-1)/2);
	subDivLevel = 1;

	parallaxCorrection.Set(this, "prllxCorr");
	parallaxCorrection.Register();
	parallaxCorrection.SetStep(0.001);
	parallaxCorrection = 0;

	//---------------//
	// vertex arrays //
	// --------------//
	vaQuad.Create(4);
	vaQuad.SetArrayBuffer(0, GL_FLOAT, 2, ogl4_2dQuadVerts);

	// create vertex array for cube faces with quad coordinates and permutation matrices
	vaSkybox.Create(4*6);
	float quadVerts6X[8*6] = {
		-.5,-.5,  .5,-.5,  -.5,.5,  .5,.5,
		-.5,-.5,  .5,-.5,  -.5,.5,  .5,.5,
		-.5,-.5,  .5,-.5,  -.5,.5,  .5,.5,
		-.5,-.5,  .5,-.5,  -.5,.5,  .5,.5,
		-.5,-.5,  .5,-.5,  -.5,.5,  .5,.5,
		-.5,-.5,  .5,-.5,  -.5,.5,  .5,.5
	};
	vaSkybox.SetArrayBuffer(0, GL_FLOAT, 2, quadVerts6X);
	uint cubeFaces[6*6] = {
		0+0*4, 1+0*4, 2+0*4 ,   3+0*4, 2+0*4, 1+0*4,
		0+1*4, 1+1*4, 2+1*4 ,   3+1*4, 2+1*4, 1+1*4,
		0+2*4, 1+2*4, 2+2*4 ,   3+2*4, 2+2*4, 1+2*4,
		0+3*4, 1+3*4, 2+3*4 ,   3+3*4, 2+3*4, 1+3*4,
		0+4*4, 1+4*4, 2+4*4 ,   3+4*4, 2+4*4, 1+4*4,
		0+5*4, 1+5*4, 2+5*4 ,   3+5*4, 2+5*4, 1+5*4
	};
	vaSkybox.SetElementBuffer(0, 6*6, cubeFaces);
	// create the permutation matrices
	// (written down in row major, so we need to transpose the matrices 
	//  later or think about the columns as rows)
	glm::mat3x3 permutationMatrices[6] = {
		glm::mat3x3(  1,  0,  0,     0,  1,  0,     0,  0, .5),   // front
		glm::mat3x3( -1,  0,  0,     0,  1,  0,     0,  0,-.5),   // back
		glm::mat3x3(  0,  0, .5,     0,  1,  0,    -1,  0,  0),   // left
		glm::mat3x3(  0,  0,-.5,     0,  1,  0,     1,  0,  0),   // right
		glm::mat3x3(  1,  0,  0,     0,  0, .5,     0, -1,  0),   // top
		glm::mat3x3(  1,  0,  0,     0,  0,-.5,     0,  1,  0)    // bottom
	};
	// create the array buffers for the matrix rows
	float permRowX_[6*3];
	float permRowY_[6*3];
	float permRowZ_[6*3];
	float permRowX[4*6*3];
	float permRowY[4*6*3];
	float permRowZ[4*6*3];
	for(uint face = 0; face < 6; face++){
		uint f = face;
		// copy the matrix entries per row vector
		permRowX_[face*3 + 0] = permutationMatrices[f][0].x;
		permRowX_[face*3 + 1] = permutationMatrices[f][0].y;
		permRowX_[face*3 + 2] = permutationMatrices[f][0].z;

		permRowY_[face*3 + 0] = permutationMatrices[f][1].x;
		permRowY_[face*3 + 1] = permutationMatrices[f][1].y;
		permRowY_[face*3 + 2] = permutationMatrices[f][1].z;

		permRowZ_[face*3 + 0] = permutationMatrices[f][2].x;
		permRowZ_[face*3 + 1] = permutationMatrices[f][2].y;
		permRowZ_[face*3 + 2] = permutationMatrices[f][2].z;
		// qadruple the entries for the 4 vertices per face
		for(uint vert = 0; vert < 4; vert++){
			for(uint i =  0; i < 3; i++){
				permRowX[face*4*3 + vert*3 + i] = permRowX_[face*3 + i];
				permRowY[face*4*3 + vert*3 + i] = permRowY_[face*3 + i];
				permRowZ[face*4*3 + vert*3 + i] = permRowZ_[face*3 + i];
			}
		}
	}
	vaSkybox.SetArrayBuffer(1,GL_FLOAT,3,permRowX);
	vaSkybox.SetElementBuffer(1, 6*6, cubeFaces);
	vaSkybox.SetArrayBuffer(2,GL_FLOAT,3,permRowY);
	vaSkybox.SetElementBuffer(2, 6*6, cubeFaces);
	vaSkybox.SetArrayBuffer(3,GL_FLOAT,3,permRowZ);
	vaSkybox.SetElementBuffer(3, 6*6, cubeFaces);

	vaCube.Create(6*4);
	float subQuadCornerVerts6X[8*6] = {
		-.5,-.5,  0,-.5,  -.5,0,  0,0,
		-.5,-.5,  0,-.5,  -.5,0,  0,0,
		-.5,-.5,  0,-.5,  -.5,0,  0,0,
		-.5,-.5,  0,-.5,  -.5,0,  0,0,
		-.5,-.5,  0,-.5,  -.5,0,  0,0,
		-.5,-.5,  0,-.5,  -.5,0,  0,0
	};
	uint indices[24] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23};
	vaCube.SetArrayBuffer(0,GL_FLOAT,2,subQuadCornerVerts6X);
	vaCube.SetElementBuffer(0,6*4,indices);
	uint permMXindices[24] = {
		0,0,0,0, 1,1,1,1, 2,2,2,2,
		3,3,3,3, 4,4,4,4, 5,5,5,5
	};
	vaCube.SetArrayBuffer(1,GL_UNSIGNED_INT, 1, permMXindices);
	vaCube.SetElementBuffer(1,6*4,indices);

	vaBox.Create(8);
	vaBox.SetArrayBuffer(0, GL_FLOAT, 4, ogl4_4dBoxVerts);
	vaBox.SetElementBuffer(0, ogl4_numBoxEdges*2, ogl4_BoxEdges);

	//---------//
	// shaders //
	//---------//
	
	quadVertShaderName = pathName + std::string("/resources/quad.vert.glsl");
	quadFragShaderName = pathName + std::string("/resources/quad.frag.glsl");
	skyboxVertShaderName = pathName + std::string("/resources/skybox.vert.glsl");
	skyboxGeomShaderName = pathName + std::string("/resources/skybox.geom.glsl");
	skyboxFragShaderName = pathName + std::string("/resources/skybox.frag.glsl");
	cubeVertShaderName =  pathName + std::string("/resources/cube.vert.glsl");
	cubeGeomShaderName =  pathName + std::string("/resources/cube.geom.glsl");
	cubeFragShaderName =  pathName + std::string("/resources/cube.frag.glsl");
	mirrorcubeGeomShaderName =  pathName + std::string("/resources/mirrorcube.geom.glsl");
	mirrorcubeFragShaderName =  pathName + std::string("/resources/mirrorcube.frag.glsl");
	boxVertShaderName = pathName + std::string("/resources/box.vert.glsl");
	boxFragShaderName = pathName + std::string("/resources/box.frag.glsl");
	layers2cubeVertShaderName = pathName + std::string("/resources/layers2cubemap.vert.glsl");
	layers2cubeGeomShaderName = pathName + std::string("/resources/layers2cubemap.geom.glsl");
	layers2cubeFragShaderName = pathName + std::string("/resources/layers2cubemap.frag.glsl");
	createShaders();

	//----------//
	// textures //
	//----------//

	texSky1 = loadCubeMapTexture(pathName + std::string("/resources/skyboxes/bridge"), light1);
	texSky2 = loadCubeMapTexture(pathName + std::string("/resources/skyboxes/space"),  light2);
	texSky3 = loadCubeMapTexture(pathName + std::string("/resources/skyboxes/clouds"), light3);
	texEarth= loadCubeMapTexture(pathName + std::string("/resources/skyboxes/earth"),  lightE);

	//----------------------//
	// Frame Buffer Objects //
	//----------------------//
	initFBO();

	//-------//
	// scene //
	//-------//
	modelMX_sky = glm::scale(glm::mat4x4(1), glm::vec3(100));

	cube1 = Object(1, glm::translate(glm::mat4x4(1), glm::vec3( 0)),&vaCube,GL_POINTS,4*6,&shaderMirrorcube,0);
	objects.push_back(&cube1);
	cube2 = Object(2, glm::translate(glm::mat4x4(1), glm::vec3( 2)),&vaCube,GL_POINTS,4*6,&shaderCube,0);
	objects.push_back(&cube2);
	cube3 = Object(3, glm::translate(glm::mat4x4(1), glm::vec3(-2)),&vaCube,GL_POINTS,4*6,&shaderCube,0);
	objects.push_back(&cube3);

	cube1.texID = texReflectionCubeMap;
	cube2.texID = texSky1;
	cube3.texID = texEarth;
	cube1.useTexture = true;

	//------//
	// misc //
	//------//
	glEnable(GL_DEPTH_TEST);

	return true;
}

/**
 * reads the geometry shader source from file, changes the max_vertices declaration
 * (which is a compile time constant) and returns the source as string.
 */
std::string readCubeGeometryShaderAndSetupMaxVerts(std::string shaderFileName, uint maxVerts){
	std::ifstream ifs(shaderFileName.c_str());
	std::string content( (std::istreambuf_iterator<char>(ifs) ),
											 (std::istreambuf_iterator<char>()    ) );
	std::string toReplace("max_vertices=73");
	size_t idx = content.find(toReplace);
	std::string replacement = std::string("max_vertices=") + std::to_string(maxVerts);
	content.replace(idx, toReplace.length(), replacement);
	return content;
}

/**
 * Creates all the shader programs from the previously set source file names
 */
void CubeMapping::createShaders(){
	shaderQuad.CreateProgramFromFile( quadVertShaderName.c_str(), quadFragShaderName.c_str() );
	shaderSkybox.CreateProgramFromFile( skyboxVertShaderName.c_str(), skyboxGeomShaderName.c_str(), skyboxFragShaderName.c_str() );
	shaderBox.CreateProgramFromFile(boxVertShaderName.c_str(), boxFragShaderName.c_str());
	shaderLayers2Cube.CreateProgramFromFile(layers2cubeVertShaderName.c_str(), layers2cubeGeomShaderName.c_str(), layers2cubeFragShaderName.c_str());

	// want to alter geometry shaders max_vertices declaration, so we cannot directly load it from file
	shaderCube.CreateEmptyProgram();
	shaderCube.AttachShaderFromFile(cubeVertShaderName.c_str(), GL_VERTEX_SHADER);
	shaderCube.AttachShaderFromFile(cubeFragShaderName.c_str(), GL_FRAGMENT_SHADER);
	std::string cubeGeomSrc = readCubeGeometryShaderAndSetupMaxVerts(cubeGeomShaderName, cubeGeomMaxVerts);
	shaderCube.AttachShaderFromString(cubeGeomSrc.c_str(), cubeGeomSrc.length(), GL_GEOMETRY_SHADER);
	shaderCube.Link();

	shaderMirrorcube.CreateEmptyProgram();
	shaderMirrorcube.AttachShaderFromFile(cubeVertShaderName.c_str(), GL_VERTEX_SHADER);
	shaderMirrorcube.AttachShaderFromFile(mirrorcubeFragShaderName.c_str(), GL_FRAGMENT_SHADER);
	std::string mirrorcubeGeomSrc = readCubeGeometryShaderAndSetupMaxVerts(mirrorcubeGeomShaderName, cubeGeomMaxVerts);
	shaderMirrorcube.AttachShaderFromString(mirrorcubeGeomSrc.c_str(), mirrorcubeGeomSrc.length(), GL_GEOMETRY_SHADER);
	shaderMirrorcube.Link();
}

/**
 * Releases and deletes all shader programs
 */
void CubeMapping::deleteShaders(){
	shaderQuad.Release();
	shaderSkybox.Release();
	shaderCube.Release();
	shaderMirrorcube.Release();
	shaderBox.Release();
	shaderLayers2Cube.Release();

	shaderQuad.RemoveAllShaders();
	shaderSkybox.RemoveAllShaders();
	shaderCube.RemoveAllShaders();
	shaderMirrorcube.RemoveAllShaders();
	shaderBox.RemoveAllShaders();
	shaderLayers2Cube.RemoveAllShaders();
}

/** tries to read a uint from the specified file */
uint readResolutionFromFile(std::string filepath) {
	std::ifstream in;
	in.open(filepath);
	if (!in.is_open()) {
		std::fprintf(stderr, "could not load file, so sorry [%s]\n", filepath.c_str());
		return false;
	}
	uint resolution;
	in >> resolution;
	in.close();
	return resolution;
}

/** tries to read a vec3 from the specified file */
void readLightLocationFromFile(std::string filepath, glm::vec3 &lightLoc) {
	std::ifstream in;
	in.open(filepath);
	if (!in.is_open()) {
		std::fprintf(stderr, "could not load file, so sorry [%s]\n", filepath.c_str());
		return;
	}
	in >> lightLoc.x;
	in >> lightLoc.y;
	in >> lightLoc.z;
	in.close();
}

/**
 * Creates a cubemap texture and loads the individual images for the faces into it.
 * The cubemap images are expected to be in a directory that contains the files
 * negx.png, posx.png, negy.png, posy.png, negz.png, posz.png.
 * It also contains the text file resolution.txt which contains a single integer 
 * which states the width and height of the cubemap face images.
 * It also contains the text file lightloc.txt which contains 3 floats forming
 * a vec3 which defines the light direction associated with the cubemap (skybox) 
 */
GLuint CubeMapping::loadCubeMapTexture(std::string directory, glm::vec3& light_location)
{
	std::string faceFileNames[6] = {
		"negx.png","posx.png",
		"negy.png","posy.png",
		"negz.png","posz.png"
	};
	GLenum targets[6] = {
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_TEXTURE_CUBE_MAP_POSITIVE_Z
	};
	uint resolution = readResolutionFromFile(directory + std::string("/resolution.txt"));
	// generate texture object
	GLuint tex;
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
	// load faces into texture from files
	for(uint i = 0; i < 6; i++){
		std::string file = directory + std::string("/") + faceFileNames[i];
		std::cout << "loading file " << file.c_str() << std::endl;
		PngBitmapCodec codec;
		BitmapImage image(resolution,resolution,3,BitmapImage::ChannelType::CHANNELTYPE_BYTE);
		codec.Image() = &image;
		codec.LoadFromFile(file.c_str());
		glTexImage2D(targets[i], 0, GL_RGB, resolution,resolution,0, GL_RGB, GL_UNSIGNED_BYTE, image.PeekDataAs<unsigned char>());
	}
	// set texture params
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	// read location of light source
	readLightLocationFromFile(directory + std::string("/lightloc.txt"),light_location);
	return tex;
}

/**
 * Creates a cubemap texture of specified resolution (width=height=resolution)
 * internal format, format and datatype.
 * It also sets the min and mag filter properties to the specified filter
 * and wrapping properties to GL_CLAMP_TO_EDGE.
 */
void createCubeMapTexture(
		GLuint &outID, const GLenum internalFormat,
		const GLenum format, const GLenum type,
		GLint filter, int resolution)
{
	GLenum targets[6] = {
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_TEXTURE_CUBE_MAP_POSITIVE_Z
	};
	glGenTextures(1,&outID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, outID);
	for(uint i = 0; i < 6; i++){
		glTexImage2D(
				targets[i],     // target
				0,              // level
				internalFormat, // internal format
				resolution,     // w
				resolution,     // h
				0,              // border
				format,         // format
				type,           // type
				0               // data
		);
	}
	// set texture params
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

/**
 * Creates a TEXTURE_2D_ARRAY of specified width, height, internal format and
 * number of layers.
 * It also sets the min and mag filter properties to the specified filter
 * and wrapping properties to GL_CLAMP_TO_EDGE.
 */
void createTextureArray( GLuint &outID, const GLenum internalFormat,
							 GLint filter, int width, int height , int numLayers) {
	glGenTextures(1, &outID);
	glBindTexture(GL_TEXTURE_2D_ARRAY, outID);
	glTexStorage3D(
				GL_TEXTURE_2D_ARRAY, // target
				1, // num mip levels
				internalFormat,
				width,
				height,
				numLayers
				);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

/** checks the status of the currently bound frame buffer object */
void checkFBOStatus() {
	GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	switch (status) {
	case GL_FRAMEBUFFER_UNDEFINED: {
		fprintf(stderr,"FBO: undefined.\n");
		break;
	}
	case GL_FRAMEBUFFER_COMPLETE: {
		fprintf(stderr,"FBO: complete.\n");
		break;
	}
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: {
		fprintf(stderr,"FBO: incomplete attachment.\n");
		break;
	}
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: {
		fprintf(stderr,"FBO: no buffers are attached to the FBO.\n");
		break;
	}
	case GL_FRAMEBUFFER_UNSUPPORTED: {
		fprintf(stderr,"FBO: combination of internal buffer formats is not supported.\n");
		break;
	}
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: {
		fprintf(stderr,"FBO: number of samples or the value for ... does not match.\n");
		break;
	}
	}
}

/**
 * Initialize the framebuffer objects
 */
void CubeMapping::initFBO() {
	if (wWidth<=0 || wHeight<=0) {
		return;
	}
	if(fbo){
		glDeleteFramebuffers(1, &fbo);
		GLuint texturesToDelete[] = {texArrayColor, texArrayDepth, texArrayPicking};
		glDeleteTextures(3, texturesToDelete);
		texArrayColor = texArrayDepth = texArrayPicking = fbo = 0;
	}

	createTextureArray(texArrayColor, GL_RGB8, GL_LINEAR,
										 wWidth, wHeight, 1+6);
	createTextureArray(texArrayPicking, GL_RGB8, GL_LINEAR,
										 wWidth, wHeight, 1+6);
	createTextureArray(texArrayDepth, GL_DEPTH_COMPONENT32, GL_LINEAR,
										 wWidth, wHeight, 1+6);


	// generate fbo and attach textures
	glGenFramebuffers(1,&fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture(
				GL_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT0,
				texArrayColor,
				0
				);
	glFramebufferTexture(
				GL_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT1,
				texArrayPicking,
				0
				);
	glFramebufferTexture(
				GL_FRAMEBUFFER,
				GL_DEPTH_ATTACHMENT,
				texArrayDepth,
				0
				);
	checkFBOStatus();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// create the fbo for texture array to cubemap transfer (dont need to reinitialize on resize)
	if(!fbo_layers2Cube){
		createCubeMapTexture(texReflectionCubeMap, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, GL_LINEAR, 1024);
		glGenFramebuffers(1, &fbo_layers2Cube);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_layers2Cube);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texReflectionCubeMap, 0);
		checkFBOStatus();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

}

/**
 * Convert object ID to unique color
 * @param id   object ID.
 * @return color
 */
glm::vec3 CubeMapping::idToColor( unsigned int id ) {
	id = id*111;
	unsigned int r = (id>>16)& 0xff;
	unsigned int g = (id>>8 )& 0xff;
	unsigned int b = (id    )& 0xff;
	float toFloat = 1.0f/255;
	glm::vec3 col = glm::vec3(r*toFloat,g*toFloat,b*toFloat);
	return col;
}

/**
 * Convert color to object ID.
 * @param buf  color defined as 3-array [red,green,blue]
 * @return object ID
 */
unsigned int CubeMapping::colorToId( unsigned char buf[3] ) {
	unsigned int num = 0;
	num = num | buf[0];
	num = num << 8;
	num = num | buf[1];
	num = num << 8;
	num = num | buf[2];
	return num/111;
}

/**
 * Callback function for the event of changing the picked object apivar.
 * This in turn sets several apivars which display the sate of the picked object
 */
void CubeMapping::objectPicked(APIVar<CubeMapping, IntVarPolicy> &id){
	pickedID = static_cast<uint>(id);
	ignoreObjectVarUpdate = true;
	if(pickedID){
		Object o = *objects[pickedID-1];
		glm::vec4 pos = o.modelMX[3];
		picked_x = pos.x;
		picked_y = pos.y;
		picked_z = pos.z;
		cube_sphere_switch = o.renderAsSphere;
		cube_texture_switch = o.useTexture;
		cube_warpFN = o.warpFN;
		picked_x.SetReadonly(false);
		picked_y.SetReadonly(false);
		picked_z.SetReadonly(false);
		cube_sphere_switch.SetReadonly(false);
		cube_texture_switch.SetReadonly(false);
		cube_warpFN.SetReadonly(false);
	} else {
		picked_x = 0;
		picked_y = 0;
		picked_z = 0;
		cube_sphere_switch = false;
		cube_texture_switch = false;
		cube_warpFN = 0;
		picked_x.SetReadonly(true);
		picked_y.SetReadonly(true);
		picked_z.SetReadonly(true);
		cube_sphere_switch.SetReadonly(true);
		cube_texture_switch.SetReadonly(true);
		cube_warpFN.SetReadonly(true);
	}
	ignoreObjectVarUpdate = false;
}

/**
 * Callback function for the event of one of the coordinate apivars picked_x/y/z being changed.
 * This alters the picked object's translation.
 */
void CubeMapping::pickedObjectMoved(APIVar<CubeMapping, FloatVarPolicy> &var){
	if(ignoreObjectVarUpdate){
		return;
	}
	if(pickedID){
		Object& o = *objects[pickedID-1];
		glm::vec4& pos = o.modelMX[3];
		pos.x = picked_x.GetValue();
		pos.y = picked_y.GetValue();
		pos.z = picked_z.GetValue();
	}
}

/**
 * Callback function for the event of an enumvar (e.g. cube_warpFN) is changed.
 * This calls the other pickedObjectModeChanged callback function.
 */
void CubeMapping::pickedObjectModeChanged(EnumVar<CubeMapping> &var) {
	// just call the other function, argument does not matter
	pickedObjectModeChanged(cube_sphere_switch);
}

/**
 * Callback function for the event of a switch var (e.g. cube_sphere_switch) is changed.
 * This updates the properties of the picked object except for coordinates.
 */
void CubeMapping::pickedObjectModeChanged(APIVar<CubeMapping, BoolVarPolicy> &var){
	if(ignoreObjectVarUpdate){
		return;
	}
	if(pickedID){
		Object& o = *objects[pickedID-1];
		o.renderAsSphere = cube_sphere_switch.GetValue();
		o.useTexture = cube_texture_switch.GetValue();
		o.warpFN = cube_warpFN.GetValue();
	}
}

/**
 * Called when changing from this plugin to another in OGL4Core.
 * Should reset all introduced GL states and delete GL objects created by this plugin.
 */
bool CubeMapping::Deactivate(void) {
	// clean up GL things
	// shaders
	deleteShaders();
	// framebuffers
	glDeleteFramebuffers(1, &fbo);
	glDeleteFramebuffers(1, &fbo_layers2Cube);
	fbo = fbo_layers2Cube = 0;
	// textures
	GLuint textures[] = {
		texSky1,texSky2,texSky3,texEarth,
		texArrayColor,texArrayDepth,texArrayPicking,
		texReflectionCubeMap
	};
	glDeleteTextures(7, textures);
	texSky1=texSky2=texSky3=texEarth=texArrayColor=texArrayDepth=texArrayPicking=texReflectionCubeMap = 0;
	// vertex arrays
	vaBox.Delete();
	vaCube.Delete();
	vaQuad.Delete();
	vaSkybox.Delete();

	glDisable(GL_DEPTH_TEST);
	return true;
}

/**
 * Sets framebuffer, drawbuffers and corresponding viewport dimensions.
 */
void setRenderTargets(GLuint fbo, uint numBuffers, const GLenum* buffers, int vWidth, int vHeight){
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	if(numBuffers > 1){
		glDrawBuffers(numBuffers, buffers);
	}else{
		glDrawBuffer(buffers[0]);
	}
	glViewport(0,0,vWidth, vHeight);
}

/**
 * renders the last 6 layers of the texture array onto the cubemap texture
 * (transfering from array texture to cubemap)
 * this changes framebuffer and viewport
 */
void CubeMapping::transferArrayTexture2CubeMap(){
	GLenum drawbuffer = GL_COLOR_ATTACHMENT0;
	setRenderTargets(fbo_layers2Cube, 1, &drawbuffer, 1024, 1024);
	// clear
	glClearColor( 0.0, 0.0, 0.0, 1.0 );
	glClear( GL_COLOR_BUFFER_BIT );
	// draw quad
	glm::mat4 pmx = glm::ortho(0.0f,1.0f,0.0f,1.0f);
	shaderLayers2Cube.Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texArrayColor);
	glUniform1i( shaderLayers2Cube.GetUniformLocation("tex"), 0);
	glUniformMatrix4fv( shaderLayers2Cube.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(pmx) );
	vaQuad.Bind();
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	vaQuad.Release();
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	shaderLayers2Cube.Release();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
 * main rendering routine, renders everything into FBO which will later
 * be drawn to q quad.
 */
void CubeMapping::drawToFBO() {
	// set fbo and drawbuffers
	GLenum buffersColAndPick[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
	GLenum buffersColOnly[] = {GL_COLOR_ATTACHMENT0};
	setRenderTargets(fbo, 2, buffersColAndPick, wWidth, wHeight);
	// clear
	glClearColor( 0.0, 0.0, 0.0, 1.0 );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	// matrices
	glm::mat4x4 projMX = glm::perspective(
				glm::radians(static_cast<float>(fovY)),
				aspect,
				static_cast<float>(zNear),
				static_cast<float>(zFar)
	);
	glm::mat4x4 invViewMX = glm::inverse(viewMX);
	glm::mat4x4 boxProjMX = glm::perspective(
				glm::radians(90.0f),
				1.0f,
				static_cast<float>(zNear),
				static_cast<float>(zFar)
	);
	glm::vec4 boxTransl = objects[0]->modelMX*glm::vec4(0,0,0,1);
	glm::mat4x4 boxTranslMX = glm::translate(glm::mat4x4(1), -glm::vec3(boxTransl));

	// light direction (depending on used skybox)
	glm::vec3 lights[] = {glm::vec3(1,0,0),light1,light2,light3};
	uint skyboxSelection = static_cast<uint>(skyboxTexturing);
	glm::vec3 lightDir = -lights[skyboxSelection];

	setRenderTargets(fbo, 1, buffersColOnly, wWidth, wHeight);
	// draw skybox
	{
		shaderSkybox.Bind();
		glActiveTexture(GL_TEXTURE0);
		GLuint skyboxTextures[] = {0, texSky1, texSky2, texSky3};
		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTextures[skyboxSelection]);
		glUniform1i( shaderSkybox.GetUniformLocation("tex"), 0);
		glUniform1i( shaderSkybox.GetUniformLocation("useTexture"), skyboxSelection != 0);
		glUniformMatrix4fv( shaderSkybox.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(projMX) );
		glUniformMatrix4fv( shaderSkybox.GetUniformLocation("boxProjMX"), 1, GL_FALSE, glm::value_ptr(boxProjMX) );
		// use untranslated camera for skybox so that it is centered
		glm::mat4 skyboxViewMX = viewMX; skyboxViewMX[3] = glm::vec4(0,0,0,1);
		glUniformMatrix4fv( shaderSkybox.GetUniformLocation("viewMX"), 1, GL_FALSE, glm::value_ptr(skyboxViewMX) );
		glUniformMatrix4fv( shaderSkybox.GetUniformLocation("modelMX"), 1, GL_FALSE, glm::value_ptr(modelMX_sky) );
		vaSkybox.Bind();
		glDrawElements(GL_TRIANGLES, 6*6, GL_UNSIGNED_INT, 0);
		vaSkybox.Release();
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		shaderSkybox.Release();
	}
	setRenderTargets(fbo, 2, buffersColAndPick, wWidth, wHeight);
	// draw objects (not the first which is the mirror object)
	for(uint i=1; i < objects.size(); i++){
		Object obj = *objects[i];
		obj.shader->Bind();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, obj.texID);
		glUniform1i( obj.shader->GetUniformLocation("tex"), 0 );
		glUniform1i( obj.shader->GetUniformLocation("useTexture"), obj.useTexture );
		glUniform1i( obj.shader->GetUniformLocation("doSphereProjection"), obj.renderAsSphere );
		glUniform1i( obj.shader->GetUniformLocation("warpFN"), obj.warpFN );
		glUniform1i( obj.shader->GetUniformLocation("subDivisionLevel"), static_cast<int>(subDivLevel));
		glUniform1f( obj.shader->GetUniformLocation("k_exp"), 10.0f );
		glUniform1f( obj.shader->GetUniformLocation("totalQuadSize"), 0.5f );
		glUniform3fv( obj.shader->GetUniformLocation("pickColor"),1, glm::value_ptr(idToColor(obj.id)) );
		glUniform3fv( obj.shader->GetUniformLocation("lightDir"), 1, glm::value_ptr(lightDir) );
		glUniformMatrix4fv( obj.shader->GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(projMX) );
		glUniformMatrix4fv( obj.shader->GetUniformLocation("viewMX"), 1, GL_FALSE, glm::value_ptr(viewMX) );
		glUniformMatrix4fv( obj.shader->GetUniformLocation("modelMX"), 1, GL_FALSE, glm::value_ptr(obj.modelMX) );
		glUniformMatrix4fv( obj.shader->GetUniformLocation("invViewMX"), 1, GL_FALSE, glm::value_ptr(invViewMX) );
		glUniformMatrix4fv( obj.shader->GetUniformLocation("boxProjMX"), 1, GL_FALSE, glm::value_ptr(boxProjMX) );
		glUniformMatrix4fv( obj.shader->GetUniformLocation("boxTransMX"), 1, GL_FALSE, glm::value_ptr(boxTranslMX) );

		obj.va->Bind();
		glDrawElements(obj.elementType, obj.numElements, GL_UNSIGNED_INT, 0);
		obj.va->Release();
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		obj.shader->Release();
	}

	// transfer the rendered faces to the cubemap texture
	transferArrayTexture2CubeMap();
	setRenderTargets(fbo, 2, buffersColAndPick, wWidth, wHeight);

	// draw the mirror object
	{
		Object obj = *objects[0];
		obj.shader->Bind();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, obj.texID);
		glUniform1i( obj.shader->GetUniformLocation("tex"), 0 );
		glUniform1i( obj.shader->GetUniformLocation("useTexture"), obj.useTexture );
		glUniform1i( obj.shader->GetUniformLocation("doSphereProjection"), obj.renderAsSphere );
		glUniform1i( obj.shader->GetUniformLocation("warpFN"), obj.warpFN );
		glUniform1i( obj.shader->GetUniformLocation("subDivisionLevel"), static_cast<int>(subDivLevel));
		glUniform1f( obj.shader->GetUniformLocation("k_exp"), 10.0f );
		glUniform1f( obj.shader->GetUniformLocation("totalQuadSize"), 0.5f );
		glUniform1f( obj.shader->GetUniformLocation("parallaxCorrectionFactor"), parallaxCorrection.GetValue() );
		glUniform3fv( obj.shader->GetUniformLocation("pickColor"),1, glm::value_ptr(idToColor(obj.id)) );
		glUniform3fv( obj.shader->GetUniformLocation("lightDir"), 1, glm::value_ptr(lightDir) );
		glUniform3fv( obj.shader->GetUniformLocation("cubeCenterWorldCoords"), 1, glm::value_ptr(glm::vec3(obj.modelMX*glm::vec4(0,0,0,1))) );
		glUniformMatrix4fv( obj.shader->GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(projMX) );
		glUniformMatrix4fv( obj.shader->GetUniformLocation("viewMX"), 1, GL_FALSE, glm::value_ptr(viewMX) );
		glUniformMatrix4fv( obj.shader->GetUniformLocation("modelMX"), 1, GL_FALSE, glm::value_ptr(obj.modelMX) );
		glUniformMatrix4fv( obj.shader->GetUniformLocation("invViewMX"), 1, GL_FALSE, glm::value_ptr(invViewMX) );

		obj.va->Bind();
		glDrawElements(obj.elementType, obj.numElements, GL_UNSIGNED_INT, 0);
		obj.va->Release();
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		obj.shader->Release();
	}

	setRenderTargets(fbo, 1, buffersColOnly, wWidth, wHeight);
	if(pickingEnabled && pickedID){
		// draw box around picked object
		Object& obj = *objects[pickedID-1];
		glm::mat4x4 scaleMX = glm::scale(glm::mat4(1),glm::vec3(1.1f, 1.1f, 1.1f));
		shaderBox.Bind();
		glUniformMatrix4fv( shaderBox.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(projMX) );
		glUniformMatrix4fv( shaderBox.GetUniformLocation("viewMX"), 1, GL_FALSE, glm::value_ptr(viewMX) );
		glUniformMatrix4fv( shaderBox.GetUniformLocation("modelMX"), 1, GL_FALSE, glm::value_ptr(obj.modelMX*scaleMX) );
		vaBox.Bind();
		glDrawElements(GL_LINES, ogl4_numBoxEdges*2, GL_UNSIGNED_INT, 0);
		vaBox.Release();
		shaderBox.Release();
	}

	GLenum windowBuffer = GL_BACK;
	setRenderTargets(0, 1, &windowBuffer, wWidth, wHeight);
}

/**
 * Render method (is called by OGL4Core)
 */
bool CubeMapping::Render(void) {
	drawToFBO();
	glClearColor( 0.0, 0.0, 0.0, 1.0 );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	// draw quad
	glm::mat4 pmx = glm::ortho(0.0f,1.0f,0.0f,1.0f);
	shaderQuad.Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texArrayColor);
	glUniform1i( shaderQuad.GetUniformLocation("tex"), 0);
	glUniformMatrix4fv( shaderQuad.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(pmx) );
	glUniform1i( shaderQuad.GetUniformLocation("useTexture"), true );
	vaQuad.Bind();
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	vaQuad.Release();
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	shaderQuad.Release();
	return false;
}

/**
 * Callback function for the event of window resizing.
 * The new viewport dimensions are passed as arguments.
 */
bool CubeMapping::Resize(int w, int h){
	wWidth = w;
	wHeight = h;
	aspect = wWidth*1.0f/wHeight;
	initFBO();
	return false;
}

/** keyboard event callback function (outdated OGL4Core version for linux) */
bool CubeMapping::Keyboard(uchar key, int x, int y) { return this->Keyboard(key, -1, -1, x, y);}

/** keyboard event callback function */
bool CubeMapping::Keyboard(int key, int action, int mods, int x, int y) {
	std::fprintf(stdout, "key:%X action:%d mods:%X\n",key,action,mods);
	std::flush(std::cout);
	switch (key) {
		case KEY_R:
			if(action*action == 1){
				// reload shaders
				deleteShaders();
				createShaders();
				std::cout << "reloaded shaders" << std::endl;
				PostRedisplay();
			}
			break;
		case KEY_S:
			// en/disable picking
			if(action*action == 1){
				pickingEnabled = !pickingEnabled;
				// show/hide api vars for picking
				pickedIDVar.SetVisible(pickingEnabled);
				picked_x.SetVisible(pickingEnabled);
				picked_y.SetVisible(pickingEnabled);
				picked_z.SetVisible(pickingEnabled);
				cube_sphere_switch.SetVisible(pickingEnabled);
				cube_texture_switch.SetVisible(pickingEnabled);
				cube_warpFN.SetVisible(pickingEnabled);
				if(pickingEnabled){
					DisableManipulator(camHandle);
				} else {
					EnableManipulator(camHandle);
				}
			}
			break;
		default:
			break;
	}
	PostRedisplay();
	return false;
}

/** mouse button event callback function (outdated OGL4Core version for linux) */
bool CubeMapping::Mouse(int button, int state, int x, int y) { return this->Mouse(button, state, -1, x, y);}

/** mouse button event callback function */
bool CubeMapping::Mouse(int button, int state, int mods, int x, int y) {
	std::fprintf(stdout, "mouse: %d %d btn:%d stt:%d\n", x,y, button,state);
	std::flush(std::cout);
	if(pickingEnabled){
		// on left click, poll pixel color to find out which object was picked
		if(button==0 && state == 0){
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
			glReadBuffer(GL_COLOR_ATTACHMENT1);
			unsigned char pickedColor[3];
			glReadPixels(x,wHeight-1-y,1,1,GL_RGB, GL_UNSIGNED_BYTE, pickedColor);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			uint id = colorToId(pickedColor);
			pickedIDVar = id;
		}
	}
	PostRedisplay();
	return false;
}

/** mouse movement callback function */
bool CubeMapping::Motion(int x, int y) {
	if(pickingEnabled && pickedID){
		glm::ivec2 last = GetLastMousePos();
		int dx = x-last.x;
		int dy = y-last.y;

		if(IsRightButtonPressed() || IsMidButtonPressed()){
			// projection matrix
			glm::mat4x4 projMX = glm::perspective(
						glm::radians(static_cast<float>(fovY)),
						aspect,
						static_cast<float>(zNear),
						static_cast<float>(zFar)
			);
			// obtain point translation from model transformation (dont want the scaling and rotation part)
			Object& obj = *objects[pickedID-1];
			glm::mat4x4 modelMX = obj.modelMX;
			glm::vec4 modelTransl = modelMX*glm::vec4(0,0,0,1);
			glm::mat4 translMX = glm::translate(glm::mat4(1.0f),glm::vec3(modelTransl));
			// obtain obj position in screen space
			glm::mat4 forward = projMX*viewMX*translMX;
			glm::vec4 objpos = forward*glm::vec4(0,0,0,1);
			std::cout << "objpos:" << objpos.x << " " << objpos.y << " " << objpos.z << " " << objpos.w << std::endl;
			// translate either in x-y or z
			if(IsRightButtonPressed()){
				// projection to image plane
				float w = objpos.w;
				objpos = (1.0f/w)*objpos;
				// translate obj in screen space according to mouse movement (x-y-plane)
				objpos = objpos + glm::vec4(dx*2.0f/wWidth,-dy*2.0f/wHeight,0,0);
				// un-projection
				objpos = w*objpos;
			} else if(IsMidButtonPressed()){
				// translate obj in (pre-)screen space according to mouse-y movement (z direction)
				objpos = objpos + glm::vec4(0,0,-dy*0.02f/wHeight,0);
			}
			// do backwards transformation to obtain translation in object space
			glm::vec4 translation = glm::inverse(forward)*objpos;
			std::cout << "transl:" << translation.x << " " << translation.y << " " << translation.z << " " << translation.w << std::endl;
			// translate picked object (this is the actual translation)
			// ignore first two coordinate updates, so only the last will trigger updating the objects position
			ignoreObjectVarUpdate = true;
			picked_x = static_cast<float>(picked_x) + translation.x;
			picked_y = static_cast<float>(picked_y) + translation.y;
			ignoreObjectVarUpdate = false;
			picked_z = static_cast<float>(picked_z) + translation.z;
		}
	}
	return false;
}

