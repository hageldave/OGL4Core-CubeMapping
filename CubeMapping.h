#include "RenderPlugin.h"
#include "glm/glm.hpp"
#include "GL/gl3w.h"
#include "FramebufferObject.h"
#include "GLShader.h"
#include "VertexArray.h"

#define GLM_FORCE_RADIANS 1

#ifndef uint
#define uint unsigned int
#endif
#ifndef ushort
#define ushort unsigned short
#endif
#ifndef uchar
#define uchar unsigned char
#endif

/** The Object struct stores state information of scene objects
 * as well as handles to vertex arrays and shaders
 */
typedef struct Object_t {

	/** id of the object used for picking and as index in the objects
	 * array (idx = id-1)
	 */
	uint id;
	glm::mat4    modelMX;     //!< model matrix
	VertexArray* va;          //!< vertex array
	GLenum       elementType; //!< element type (GL_TRIANGLES)
	uint         numElements; //!< number of vertices to be drawn
	GLShader*    shader;      //!< shader for the object
	GLuint       texID;       //!< texture handle (cubemap texture)
	bool useTexture;          //!< flag wether to use texture
	bool renderAsSphere;      //!< flag wether to render as speher or as cube
	int warpFN;               //!< warping function to be used (0,1,2)

	Object_t() {
		id = 0;
		modelMX = glm::mat4(1.0f);
		va = nullptr;
		elementType = GL_POINTS;
		numElements = 0;
		shader = nullptr;
		texID = 0;
		useTexture = false;
		renderAsSphere = false;
		warpFN = 0;
	}
	Object_t( uint id, glm::mat4 modelMX, VertexArray* va, GLenum elementType, uint numElements, GLShader* shader, GLuint texID ) {
		this->id = id;
		this->modelMX = modelMX;
		this->va = va;
		this->elementType = elementType;
		this->numElements = numElements;
		this->shader = shader;
		this->texID = texID;
		useTexture = false;
		renderAsSphere = false;
		warpFN = 0;
	}
} Object;

/**
 * CubeMapping RenderPlugin - a demo of cubemapping applications 
 */
class OGL4COREPLUGIN_API CubeMapping : public RenderPlugin {
public:
	CubeMapping(COGL4CoreAPI *Api);
	~CubeMapping(void);

	virtual bool Activate(void);
	virtual bool Deactivate(void);
	virtual bool Init(void);
	virtual bool Render(void);
	virtual bool Resize(int w, int h);
	virtual bool Keyboard(unsigned char key, int x, int y);	// linux
	virtual bool Keyboard(int key, int action, int mods, int x, int y);	// windows
	virtual bool Motion(int x, int y);
	virtual bool Mouse(int button, int state, int x, int y);	// linux
	virtual bool Mouse(int button, int state, int mods, int x, int y);	// windows

private:
	glm::mat4 viewMX; //!< view matrix of the camera
	int camHandle;    //!< handle to the manipulator controlling the cam
	int wWidth;       //!< window width
	int wHeight;      //!< window height
	float aspect;     //!< aspect ratio

	GLint maxGeomTotalOutComp; //!< current context's max number of output comps in geometry shaders
	GLint maxGeomOutVerts;     //!< current context's max number of output verts in geometry shaders
	uint cubeGeomMaxVerts;     //!< resulting maximum number of verts for the cube geometry shader

	glm::mat4x4 modelMX_sky; //!< skybox model matrix (scaling only)

	APIVar<CubeMapping, FloatVarPolicy> fovY;   //!< camera's vertical field of view
	APIVar<CubeMapping, FloatVarPolicy>  zNear; //!< near clipping plane
	APIVar<CubeMapping, FloatVarPolicy>  zFar;  //!< far clipping plane
	EnumVar<CubeMapping> skyboxTexturing;       //!< selection for skybox texturing

	APIVar<CubeMapping, IntVarPolicy> pickedIDVar;          //!< shows/selects picked id
	APIVar<CubeMapping, FloatVarPolicy> picked_x;           //!< picked object's x coord
	APIVar<CubeMapping, FloatVarPolicy> picked_y;           //!< picked object's y coord
	APIVar<CubeMapping, FloatVarPolicy> picked_z;           //!< picked object's z coord
	APIVar<CubeMapping, BoolVarPolicy> cube_sphere_switch;  //!< switch for cube/sphere rendering
	APIVar<CubeMapping, BoolVarPolicy> cube_texture_switch; //!< switch for cubemap texture / checkerboard
	EnumVar<CubeMapping> cube_warpFN;                       //!< selector for warping function

	APIVar<CubeMapping, IntVarPolicy> subDivLevel;          //!< subdivision level for cube face 
	APIVar<CubeMapping, FloatVarPolicy> parallaxCorrection; //!< parallax correction factor (0 = none)

	VertexArray vaQuad;             //!< vertex array for a quad
	std::string quadVertShaderName; //!< quad vertex shader filename 
	std::string quadFragShaderName; //!< quad fragment shader filename
	GLShader shaderQuad;            //!< quad shader
	
	std::string layers2cubeVertShaderName; //!< layers2cube vertex shader filename
	std::string layers2cubeGeomShaderName; //!< layers2cube geometry shader filename
	std::string layers2cubeFragShaderName; //!< layers2cube fragment shader filename
	GLShader shaderLayers2Cube;            //!< layers2cube shader

	VertexArray vaSkybox;             //!< vertex array for skybox
	std::string skyboxVertShaderName; //!< skybox vertex shader filename
	std::string skyboxGeomShaderName; //!< skybox geometry shader filename
	std::string skyboxFragShaderName; //!< skybox fragment shader filename
	GLShader shaderSkybox;            //!< skybox shader

	VertexArray vaCube;                   //!< vertex array for cube
	std::string cubeVertShaderName;       //!< cube vertex shader filename
	std::string cubeGeomShaderName;       //!< cube geometry shader filename
	std::string cubeFragShaderName;       //!< cube fragment shader filename
	GLShader shaderCube;                  //!< cube shader
	std::string mirrorcubeGeomShaderName; //!< mirror cube geometry shader filename
	std::string mirrorcubeFragShaderName; //!< mirror cube fragment shader filename
	GLShader shaderMirrorcube;            //!< mirror cube shader

	VertexArray vaBox;             //!< vertex array for box 
	std::string boxVertShaderName; //!< box vertex shader filename
	std::string boxFragShaderName; //!< box fragment shader filename
	GLShader shaderBox;            //!< box shader

	GLuint texSky1;   //!< skybox cubemap texture 1
	GLuint texSky2;   //!< skybox cubemap texture 2
	GLuint texSky3;   //!< skybox cubemap texture 3
	GLuint texEarth;  //!< cubemap texture earth
	glm::vec3 light1; //!< light direction for skybox 1
	glm::vec3 light2; //!< light direction for skybox 2
	glm::vec3 light3; //!< light direction for skybox 3
	glm::vec3 lightE; //!< light direction for earth cubemap (when used as skybox)

	GLuint fbo;                  //!< handle for FBO
	GLuint texArrayColor;        //!< handle for 'standard' color attachment (1+6 layers)
	GLuint texArrayPicking;      //!< handle for picking color attachment (1 layer)
	GLuint texArrayDepth;        //!< handle for depth attachment (1+6 layers)
	GLuint fbo_layers2Cube;      //!< handle for the cubemap FBO (used to transfer contents from layers to cubemap)
	GLuint texReflectionCubeMap; //!< handle for the cubemap where the reflection is rendered to

	Object cube1;                 //!< 1st scene object
	Object cube2;                 //!< 2nd scene object
	Object cube3;                 //!< 3rd scene object
	std::vector<Object*> objects; //!< all scene objects (for lookup from picking using id-1)

	// picking things
	uint pickedID;              //!< currently picked object's id (0=none picked)
	bool pickingEnabled;        //!< wether picking is enabled or not
	bool ignoreObjectVarUpdate; //!< flag for ignoring updates posted to the methods pickedObjectModeChanged(..) and pickedObjectMoved(..)

private:
	void createShaders();
	void deleteShaders();
	void drawToFBO();
	void transferArrayTexture2CubeMap();
	void initFBO();
	GLuint loadCubeMapTexture(std::string directory, glm::vec3& light_location);
	glm::vec3 idToColor( uint id );
	uint colorToId( uchar buf[3] );
	void objectPicked(APIVar<CubeMapping, IntVarPolicy> &id);
	void pickedObjectMoved(APIVar<CubeMapping, FloatVarPolicy> &var);
	void pickedObjectModeChanged(APIVar<CubeMapping, BoolVarPolicy> &var);
	void pickedObjectModeChanged(EnumVar<CubeMapping> &var);
};

extern "C" OGL4COREPLUGIN_API RenderPlugin* OGL4COREPLUGIN_CALL CreateInstance(COGL4CoreAPI *Api) {
	return new CubeMapping(Api);
}
