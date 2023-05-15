// Windows includes (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLM/gtx/euler_angles.hpp>

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

// Project includes
// Loading photos
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "maths_funcs.h"
#define GLT_IMPLEMENTATION
#include "gltext.h"

#pragma region SimpleTypes
typedef struct
{
	size_t mPointCount = 0;
	std::vector<vec3> mVertices;
	std::vector<vec3> mNormals;
	std::vector<vec2> mTextureCoords;
} ModelData;
#pragma endregion SimpleTypes

using namespace std;

int width = 900;
int height = 900;

glm::vec3 lightPos, lightColor, objectColor, viewPos;
GLuint loc1, loc2, loc3;
GLfloat rotate_y = 0.0f;
GLfloat prop_rotate_y = 0.0f;

// Define the plane's pitch, roll, and yaw rotations 
float pitch = 0.0f;
float roll = 0.0f;
float yaw = 0.0f;

//mouse/Key movement
float x_mouse;
float y_mouse;
float z_mouse = 1.0f;
float x_pos, y_pos, z_pos = 0.0f;

glm::mat4 view, proj, model;

GLuint airplaneShaderProgram, textShaderProgram, skyboxShaderProgramID;
ModelData airplane_data, prop_data;
GLuint airplane_vao = 0, airplane_vp_vbo = 0, airplane_vn_vbo = 0;
GLuint prop_vao = 0, prop_vp_vbo = 0, prop_vn_vbo = 0;

int displayMode = 0;
bool isFirstView = false;

// For debugging
float offsetX = 0.0f;
float offsetY = 0.0f;

#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

ModelData load_mesh(const char* file_name) {
	ModelData modelData;

	/* Use assimp to read the model file, forcing it to be read as    */
	/* triangles. The second flag (aiProcess_PreTransformVertices) is */
	/* relevant if there are multiple meshes in the model file that   */
	/* are offset from the origin. This is pre-transform them so      */
	/* they're in the right position.                                 */
	const aiScene* scene = aiImportFile(
		file_name, 
		aiProcess_Triangulate | aiProcess_PreTransformVertices
	); 

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", file_name);
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		modelData.mPointCount += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelData.mVertices.push_back(vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelData.mNormals.push_back(vec3(vn->x, vn->y, vn->z));
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				modelData.mTextureCoords.push_back(vec2(vt->x, vt->y));
			}
			if (mesh->HasTangentsAndBitangents()) {
				/* You can extract tangents and bitangents here              */
				/* Note that you might need to make Assimp generate this     */
				/* data for you. Take a look at the flags that aiImportFile  */
				/* can take.                                                 */
			}
		}
	}

	aiReleaseImport(scene);
	return modelData;
}

#pragma endregion MESH LOADING

// ------------ SKYBOX ------------
unsigned int skyboxVAO, skyboxVBO;
unsigned int cubemapTexture;
vector<std::string> faces
{
	"./skybox/right.jpg",
	"./skybox/left.jpg",
	"./skybox/top.jpg",
	"./skybox/bottom.jpg",
	"./skybox/front.jpg",
	"./skybox/back.jpg"
};

float skyboxVertices[] = {
	-200.0f,  200.0f, -200.0f,
	-200.0f, -200.0f, -200.0f,
	 200.0f, -200.0f, -200.0f,
	 200.0f, -200.0f, -200.0f,
	 200.0f,  200.0f, -200.0f,
	-200.0f,  200.0f, -200.0f,

	-200.0f, -200.0f,  200.0f,
	-200.0f, -200.0f, -200.0f,
	-200.0f,  200.0f, -200.0f,
	-200.0f,  200.0f, -200.0f,
	-200.0f,  200.0f,  200.0f,
	-200.0f, -200.0f,  200.0f,

	 200.0f, -200.0f, -200.0f,
	 200.0f, -200.0f,  200.0f,
	 200.0f,  200.0f,  200.0f,
	 200.0f,  200.0f,  200.0f,
	 200.0f,  200.0f, -200.0f,
	 200.0f, -200.0f, -200.0f,

	-200.0f, -200.0f,  200.0f,
	-200.0f,  200.0f,  200.0f,
	 200.0f,  200.0f,  200.0f,
	 200.0f,  200.0f,  200.0f,
	 200.0f, -200.0f,  200.0f,
	-200.0f, -200.0f,  200.0f,

	-200.0f,  200.0f, -200.0f,
	 200.0f,  200.0f, -200.0f,
	 200.0f,  200.0f,  200.0f,
	 200.0f,  200.0f,  200.0f,
	-200.0f,  200.0f,  200.0f,
	-200.0f,  200.0f, -200.0f,

	-200.0f, -200.0f, -200.0f,
	-200.0f, -200.0f,  200.0f,
	 200.0f, -200.0f, -200.0f,
	 200.0f, -200.0f, -200.0f,
	-200.0f, -200.0f,  200.0f,
	 200.0f, -200.0f,  200.0f
};

// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS
char* readShaderSource(const char* shaderFile) {
	FILE* fp;
	fopen_s(&fp, shaderFile, "rb");

	if (fp == NULL) { return NULL; }

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}

unsigned int loadCubemap(vector<std::string> faces)
{
	unsigned int skyboxTextureID;
	glGenTextures(1, &skyboxTextureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTextureID);

	int width, height, nrChannels;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return skyboxTextureID;
}

void generateSkybox() {
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
}

static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		std::cerr << "Error creating shader..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	// check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024] = { '\0' };
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		std::cerr << "Error compiling "
			<< (ShaderType == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< " shader program: " << InfoLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders(const char* vertexShaderName, const char* fragmentShaderName)
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	GLuint shaderProgramID = glCreateProgram();
	if (shaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgramID, vertexShaderName, GL_VERTEX_SHADER);
	AddShader(shaderProgramID, fragmentShaderName, GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { '\0' };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgramID);
	return shaderProgramID;
}
#pragma endregion SHADER_FUNCTIONS

// VBO Functions - click on + to expand
#pragma region VBO_FUNCTIONS
void generateObjectBufferMesh(GLuint shaderProgramID, ModelData mesh_data, GLuint vao, GLuint vp_vbo, GLuint vn_vbo) {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	//Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
	//Might be an idea to do a check for that before generating and binding the buffer.

	loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
	loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
	loc3 = glGetAttribLocation(shaderProgramID, "vertex_texture");

	glGenBuffers(1, &vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mVertices[0], GL_STATIC_DRAW);
	glGenBuffers(1, &vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mNormals[0], GL_STATIC_DRAW);

	glBindVertexArray(vao);

	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
}
#pragma endregion VBO_FUNCTIONS

void drawText(const char* str, GLfloat size, glm::vec3 pos, glm::vec4 color=glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);
	glUseProgram(textShaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(textShaderProgram, "proj"), 1, GL_FALSE, glm::value_ptr(projection));

	// Initialize glText
	gltInit();

	// Creating text
	GLTtext* text = gltCreateText();
	gltSetText(text, str);

	// Begin text drawing (this for instance calls glUseProgram)
	gltBeginDraw();

	// Draw any amount of text between begin and end
	gltColor(color.x, color.y, color.z, color.w);
	gltDrawText2DAligned(text, 70 * (pos.x + 1), 450 - pos.y * 70, size, GLT_CENTER, GLT_CENTER);

	// Finish drawing text
	gltEndDraw();

	// Deleting text
	gltDeleteText(text);

	// Destroy glText
	gltTerminate();
	glDisable(GL_BLEND);
}

float range_angle_360(float angle) {
	if (angle > 360) {
		angle = 360;
		drawText("Angle Limit Exceeded!", 2, glm::vec3(5.4f, -2.5f, 0.0f), glm::vec4(237 / 255.0, 108 / 255.0, 2 / 255.0, 1.0));
	}
	else if (angle < -360) {
		angle = -360;
		drawText("Angle Limit Exceeded!", 2, glm::vec3(5.4f, -2.5f, 0.0f), glm::vec4(237 / 255.0, 108 / 255.0, 2 / 255.0, 1.0));
	}
	return angle;
}

void drawSkybox() {
	glDepthFunc(GL_LEQUAL);
	glUseProgram(skyboxShaderProgramID);

	glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgramID, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(skyboxShaderProgramID, "proj"), 1, GL_FALSE, glm::value_ptr(proj));

	glBindVertexArray(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
}

void drawTipText() {
	// Show Project Title
	drawText("Plane Rotation", 3, glm::vec3(5.4, 5.5, 0));

	// Show Params Value
	yaw = range_angle_360(yaw);
	pitch = range_angle_360(pitch);
	roll = range_angle_360(roll);

	char array[20];
	snprintf(array, sizeof(array), "Yaw: %1.1f", yaw);
	drawText(array, 2, glm::vec3(2.4f, -3.5f, 0.0f));
	snprintf(array, sizeof(array), "Pitch: %1.1f", pitch);
	drawText(array, 2, glm::vec3(5.4f, -3.5f, 0.0f));
	snprintf(array, sizeof(array), "Roll: %1.1f", roll);
	drawText(array, 2, glm::vec3(8.6f, -3.5f, 0.0f));

	// Check for Gimbal Lock 
	if (glm::abs(pitch) == 90 && displayMode == 1)
	{
		std::cout << "Gimbal lock detected!" << std::endl;
		drawText("Gimbal lock detected!", 2, glm::vec3(5.4f, -2.5f, 0.0f), glm::vec4(237 / 255.0, 108 / 255.0, 2 / 255.0, 1.0));
	}

	// Show Display Mode Text
	char* displayModeStr = "";
	if (displayMode == 0) {
		displayModeStr = "Display Mode: Matrix";
	}
	else if (displayMode == 1) {
		displayModeStr = "Display Mode: Euler - (yaw, pitch, roll)";
	}
	else if (displayMode == 2) {
		displayModeStr = "Display Mode: Quaternion";
	}
	drawText(displayModeStr, 2, glm::vec3(5.4f, -4.5f, 0.0f), glm::vec4(237 / 255.0, 108 / 255.0, 2 / 255.0, 1.0));
}

void drawModels() {
	glUseProgram(airplaneShaderProgram);

	glBindVertexArray(airplane_vao);
	glUniform3fv(glGetUniformLocation(airplaneShaderProgram, "objectColor"), 1, &objectColor[0]);
	glUniform3fv(glGetUniformLocation(airplaneShaderProgram, "lightColor"), 1, &lightColor[0]);
	glUniform3fv(glGetUniformLocation(airplaneShaderProgram, "lightPos"), 1, &lightPos[0]);
	glUniform3fv(glGetUniformLocation(airplaneShaderProgram, "viewPos"), 1, &viewPos[0]);

	// root - plane
	glm::mat4 modelPlane = glm::mat4(1.0f);
	modelPlane = model * modelPlane;

	// mouse animation
	//modelPlane = glm::translate(modelPlane, glm::vec3(-x_mouse + x_pos, y_mouse + y_pos, z_mouse + z_pos));

	if (displayMode == 0) {  // 4x4 transformation matrix
		modelPlane = glm::rotate(modelPlane, glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
		modelPlane = glm::rotate(modelPlane, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
		modelPlane = glm::rotate(modelPlane, glm::radians(roll), glm::vec3(0.0f, 0.0f, 1.0f));
		modelPlane = glm::translate(modelPlane, glm::vec3(0.0f, 0.0f, 0.0f));
	}
	else if (displayMode == 1) {  // Euler: Yaw, Pitch, Roll
		glm::mat4 R = glm::yawPitchRoll(glm::radians(yaw), glm::radians(pitch), glm::radians(roll)); // Yaw, Pitch, Roll
		modelPlane = modelPlane * R;
	}
	else if (displayMode == 2) {  // Quaternion
	// Convert Euler angle to Quaternion
	//glm::quat q;
	//q.w = cos(yaw / 2) * cos(pitch / 2) * cos(roll / 2) + sin(yaw / 2) * sin(pitch / 2) * sin(roll / 2);
	//q.x = sin(yaw / 2) * cos(pitch / 2) * cos(roll / 2) - cos(yaw / 2) * sin(pitch / 2) * sin(roll / 2);
	//q.y = cos(yaw / 2) * sin(pitch / 2) * cos(roll / 2) + sin(yaw / 2) * cos(pitch / 2) * sin(roll / 2);
	//q.z = cos(yaw / 2) * cos(pitch / 2) * sin(roll / 2) - sin(yaw / 2) * sin(pitch / 2) * cos(roll / 2);
	//// Convert quaternion to 4*4 matrix
	//glm::mat4 R;
	//R[0][0] = 1 - 2 * q.y * q.y - 2 * q.z * q.z;
	//R[0][1] = 2 * q.x * q.y - 2 * q.w * q.z;
	//R[0][2] = 2 * q.x * q.z + 2 * q.w * q.y;
	//R[1][0] = 2 * q.x * q.y + 2 * q.w * q.z;
	//R[1][1] = 1 - 2 * q.x * q.x - 2 * q.z * q.z;
	//R[1][2] = 2 * q.y * q.z - 2 * q.w * q.x;
	//R[2][0] = 2 * q.x * q.z - 2 * q.w * q.y;
	//R[2][1] = 2 * q.y * q.z + 2 * q.w * q.x;
	//R[2][2] = 1 - 2 * q.x * q.x - 2 * q.y * q.y;

	// Or use the glm function directly
	 glm::quat q = glm::quat(glm::vec3(glm::radians(pitch), glm::radians(yaw), glm::radians(roll)));  // Convert Euler angle to Quaternion
	 glm::mat4 R = glm::mat4_cast(q);  // Convert quaternion to 4*4 matrix

	modelPlane = modelPlane * R;  // Apply the quaternion result
	}

	glUniformMatrix4fv(glGetUniformLocation(airplaneShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(airplaneShaderProgram, "proj"), 1, GL_FALSE, glm::value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(airplaneShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelPlane));

	glDrawArrays(GL_TRIANGLES, 0, airplane_data.mPointCount);

	// child - prop
	glBindVertexArray(prop_vao);

	glm::mat4 modelProp = glm::mat4(1.0f);
	modelProp = glm::translate(modelProp, glm::vec3(0.0f, 0.0f, 2.4f));
	modelProp = glm::rotate(modelProp, glm::radians(prop_rotate_y), glm::vec3(0.0f, 0.0f, 1.0f));

	// Apply the root matrix to the child matrix
	modelProp = modelPlane * modelProp;

	glUniformMatrix4fv(glGetUniformLocation(airplaneShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(airplaneShaderProgram, "proj"), 1, GL_FALSE, glm::value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(airplaneShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelProp));

	glDrawArrays(GL_TRIANGLES, 0, prop_data.mPointCount);
}

void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (isFirstView) {
		glm::vec3 eye = glm::vec3(0.0f, 0.8f, 0.8f);
		glm::vec3 center = glm::vec3(0.0f, 0.0f, 20.0f);
		glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
		rotation = glm::rotate(rotation, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
		rotation = glm::rotate(rotation, glm::radians(roll), glm::vec3(0.0f, 0.0f, 1.0f));
		glm::vec3 eye_rotated = glm::vec3(rotation * glm::vec4(eye, 1.0f));
		center = glm::vec3(rotation * glm::vec4(center, 1.0f));
		view = glm::lookAt(eye_rotated, center, glm::vec3(rotation * glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)));
	}
	else {
		view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, -18.0f));
	}

	proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);
	model = glm::mat4(1.0f);

	// ------------------------------------------------- SKYBOX ------------------------------------------------- 
	drawSkybox();

	// ------------------------------------------------- TEXT ------------------------------------------------- 
	drawTipText();

	// ------------------------------------------------- MODEL ------------------------------------------------- 
	drawModels();

	glutSwapBuffers();
}


void updateScene() {

	static DWORD last_time = 0;
	DWORD curr_time = timeGetTime();
	if (last_time == 0)
		last_time = curr_time;
	float delta = (curr_time - last_time) * 0.001f;
	last_time = curr_time;

	prop_rotate_y += 60.0f * delta;
	prop_rotate_y = fmodf(prop_rotate_y, 360.0f);

	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	// Set up the shaders
	airplaneShaderProgram = CompileShaders("./shaders/simpleVertexShader.txt", "./shaders/simpleFragmentShader.txt");
	textShaderProgram = CompileShaders("./shaders/textVertexShader.txt", "./shaders/textFragmentShader.txt");
	skyboxShaderProgramID = CompileShaders("./shaders/skyboxVertexShader.txt", "./shaders/skyboxFragmentShader.txt");
	
	airplane_data = load_mesh("./models/plane_no_prop.dae"); // airplane
	prop_data = load_mesh("./models/prop.dae"); // prop

	// Skybox
	generateSkybox();
	cubemapTexture = loadCubemap(faces);

	glGenVertexArrays(1, &airplane_vao);
	glBindVertexArray(airplane_vao);
	glGenBuffers(1, &airplane_vp_vbo);
	glGenBuffers(1, &airplane_vn_vbo);
	generateObjectBufferMesh(airplaneShaderProgram, airplane_data, airplane_vao, airplane_vp_vbo, airplane_vn_vbo);

	glGenVertexArrays(1, &prop_vao);
	glBindVertexArray(prop_vao);
	glGenBuffers(1, &prop_vp_vbo);
	glGenBuffers(1, &prop_vn_vbo);
	generateObjectBufferMesh(airplaneShaderProgram, prop_data, prop_vao, prop_vp_vbo, prop_vn_vbo);
}

void keyPress(unsigned char key, int xmouse, int ymouse) {
	switch (key) {
		case('w'):
			pitch += 5.0f;
			break;
		case('s'):
			pitch -= 5.0f;
			break;
		case('a'):
			yaw += 5.0f;
			break;
		case('d'):
			yaw -= 5.0f;
			break;
		case('q'):
			roll -= 5.0f;
			break;
		case('e'):
			roll += 5.0f;
			break;
		case('r'):
			pitch = 0.0f;
			roll = 0.0f;
			yaw = 0.0f;
			break;
		case('v'):
			isFirstView = !isFirstView;
			break;
		case('1'):
			displayMode = 0;
			break;
		case('2'):
			displayMode = 1;
			break;
		case('3'):
			displayMode = 2;
			break;
		}
};

void specialKeypress(int key, int x, int y) {
	switch (key) {
		// Specular Lighting settings, up for up, down for down
	case(GLUT_KEY_UP):
		offsetY += 0.2;
		break;
	case(GLUT_KEY_DOWN):
		offsetY -= 0.2;
		break;
	case(GLUT_KEY_LEFT):
		offsetX -= 0.2;
		break;
	case(GLUT_KEY_RIGHT):
		offsetX += 0.2;
		break;
	}
	cout << key << ": " << offsetX << ",   " << offsetY << ",   " << endl;
}

void mouseMove(int x, int y) {
	// Fix x_mouse coordinates calculation to only take the first viewport
	x_mouse = (float)-(x - width / 2) / (width / 2);
	y_mouse = (float)-(y - height / 2) / (height / 2);
};

void mouseWheel(int key, int wheeldir, int x, int y) {
	if (wheeldir == 1)
	{
		z_mouse -= 0.2f;
	}
	else if (wheeldir == -1) {
		z_mouse += 0.2f;
	}
};

int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("Lab 2: Plane Rotation");

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutIdleFunc(updateScene);
	glutKeyboardFunc(keyPress);
	glutSpecialFunc(specialKeypress);
	glutPassiveMotionFunc(mouseMove);
	glutMouseWheelFunc(mouseWheel);

	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}
	// Set up your objects and shaders
	init();
	// Begin infinite event loop
	glutMainLoop();
	return 0;
}
