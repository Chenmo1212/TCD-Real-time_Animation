// Windows includes (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.
#include <list>

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLM/gtx/euler_angles.hpp>
#include <glm/gtx/spline.hpp>
#include <glm/gtx/vector_angle.hpp>

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
	std::vector<vec3> mTangents;
	std::vector<vec3> mBitangents;

} ModelData;
#pragma endregion SimpleTypes

using namespace std;

int width = 900;
int height = 900;

glm::vec3 lightPos, lightColor, objectColor, viewPos, cameraPos;
GLuint loc1, loc2, loc3, loc4, loc5;
GLfloat rotate_y = 0.0f;
GLfloat prop_rotate_y = 0.0f;
bool isCameraRotate = false;
GLfloat cameraRotateY = 0.0f;

glm::mat4 view, proj, model;

GLuint normalMappingProgramID, textShaderProgram, skyboxShaderProgramID;

const GLuint i = 15;
GLuint VAO[i], VBO[i * 2], VTO[i];
GLuint textureMode = 7;

GLuint angleIndex = 1;
char* modelType = "simple";
bool displayMode = true;
bool isWithinDis = true;

std::vector < ModelData > meshData;
std::vector < const char* > dataArray;
ModelData mesh_data;

bool isFirstView = false;
bool isModelRotate = false;
bool isCCDSolved = false;
float camera_dis = 4.5f;

float x_mouse = .0, y_mouse = 1.0;

glm::vec3 lastTarget;
bool isTargetSame;

GLfloat a_z[6];
float delta;

// For debugging
float offsetX = 0.0f;
float offsetY = 0.0f;
float offsetZ = 0.0f;


class Node {
public:
	string m_nodeName;  // The name of the node.

	Node* m_pParent;  // A pointer to the parent node.
	vector<Node*> m_lChildren;  // A vector of pointers to the child nodes.

	glm::mat4 m_transform;  // The local transform matrix of the node.
	glm::vec3 m_end_pos;  // The end position of the node in world space.
	glm::vec3 m_direction = glm::vec3(0, 1, 0);  // The direction vector of the node.

	GLuint m_type;  // The type of the node.
	float m_length = 0.75f;  // The length of the node (used for calculating the end position).

	// Constructor that takes the node name, type and parent node as input.
	Node(const std::string& nodeName = "", GLuint type = 0, Node* pParent = nullptr)
		: m_nodeName(nodeName), m_type(type), m_pParent(pParent) {
		m_transform = glm::mat4(1.0f);  // Initialize the transform matrix to the identity matrix.
	}

	// Adds a child node to the current node.
	void addChild(Node* pChild) {
		m_lChildren.push_back(pChild);  // Add the child node to the vector.
		pChild->m_pParent = this;  // Set the parent of the child node to the current node.
	}

	// Returns the type of the node.
	GLuint getMType() const {
		return m_type;
	}

	// Returns the world transform matrix of the node.
	glm::mat4 getWorldTransform() const {
		if (m_pParent) {
			return m_pParent->getWorldTransform() * m_transform;  // If the node has a parent, concatenate the parent's world transform with the node's local transform.
		}
		else {
			return m_transform;  // If the node doesn't have a parent, return the local transform as the world transform.
		}
	}

	// Sets the translation of the node.
	void setTranslation(float x, float y, float z) {
		m_transform = glm::translate(m_transform, glm::vec3(x, y, z));
	}

	// Sets the scale of the node.
	void setScale(float scale) {
		m_transform = glm::scale(m_transform, glm::vec3(scale));
	}

	// Sets the rotation of the node.
	void setRotation(float angle, float x = 0, float y = 0, float z = 1) {
		m_transform = glm::rotate(m_transform, glm::radians(angle), glm::vec3(x, y, z));
	}

	// Calculates and returns the end position of the node in world space.
	glm::vec3 getEndPosition() {
		glm::vec4 end_pos_local = glm::vec4(0, m_length, 0, 1);  // Calculate the end position in local space.
		glm::vec4 end_pos_world = getWorldTransform() * end_pos_local;  // Convert the end position to world space using the node's world transform.
		m_end_pos = glm::vec3(end_pos_world.x, end_pos_world.y, end_pos_world.z);  // Store the end position in a member variable for later use.
		return m_end_pos;
	}
};


class SimpleJointIK {
public:
	Node* m_start;
	Node* m_end;

	glm::vec3 m_target;
	glm::vec3 m_startPos;
	glm::vec3 m_endPos;

	float m_totalLength;
	float m_armLength = 1;  // Length of arm. And each arm's length is the same.
	float m_jointAngles[5];

	GLuint m_depth = 3;  // Depth of joints
	GLuint m_index = 0;  // Depth of joints

	SimpleJointIK(Node* startNode, Node* endNode, float length) {
		m_start = startNode;
		m_end = endNode;

		// Calculate total length
		m_totalLength = length * m_armLength;
		m_startPos = glm::vec3(0.3, 0.9, 0);    // Get position from transform matrix

		// Initialize joint angles to 0
		m_jointAngles[0] = 0.0f;
		m_jointAngles[1] = 0.0f;
		m_jointAngles[2] = 0.0f;
		m_jointAngles[3] = 0.0f;
		m_jointAngles[4] = 0.0f;
	}

	void setTargetPosition(const glm::vec3& target) {
		m_target = target - m_startPos;
	}

	void setJointsRotation(Node* node) {
		// Do something with the current node
		node->setRotation(m_jointAngles[m_index++]);

		// Recursively traverse all child nodes
		for (auto& child : node->m_lChildren) {
			setJointsRotation(child);
		}
	}

	bool checkDistance() {
		// Calculate distance to target
		float distanceToTarget = glm::length(m_target);

		float x = m_target.x;
		float y = m_target.y;

		if (distanceToTarget >= m_totalLength) {
			float angle1 = glm::degrees(acos(x / sqrt(x * x + y * y)));
			if (y < 0) angle1 = -angle1;

			if (m_start->m_nodeName == "arm4") angle1 += 180;

			m_jointAngles[0] = angle1;  // Target is out of reach, so the angle of each joint is the same.
			m_jointAngles[1] = 0.0f;

			// Set joint angles
			setJointsRotation(m_start);

			// Show text
			isWithinDis = false;

			return true;
		}
		isWithinDis = true;
		return false;
	}

	void solveAnalyticalSolution() {
		float x = m_target.x;
		float y = m_target.y;

		// Check if distance of arm and target are out of reach.
		if (checkDistance()) return;

		// Distance is in the range, solve it using analytical solution (formulas from slides)
		float a = m_armLength;
		float b = a;

		// Calculate the angle of joint2 using cosine law
		float angleTheta = acos((a * a + b * b - x * x - y * y) / (2 * a * b));
		float angle2 = 180 - glm::degrees(angleTheta);

		// Calculate the angle of joint1 using cosine law
		float acos1 = acos((a * a + (x * x + y * y) - b * b) / (2 * a * sqrt(x * x + y * y)));
		float atanT = atan(y / x);
		float angle1 = glm::degrees(atanT - acos1);
		if (x < 0) angle1 = 180 + angle1;

		// Store the angles
		m_jointAngles[0] = angle1;
		m_jointAngles[1] = angle2;

		// Set joint angles
		setJointsRotation(m_start);
	}

	void animate(float deltaTime) {
		if (checkDistance()) return;  // Check if distance of arm and target out of reach

		// Declare variables for tracking the current animation frame and the total elapsed time.
		static float elapsedTime = 0.0f; // record elapsed time
		static int currentFrame = 0; // Record the current frame number

		// Define constants for the animation duration and the number of frames in the animation.
		const float animationDuration = 1.0f; // animation duration in seconds
		const int numFrames = 100; // The total number of frames of the animation
		const float timePerFrame = animationDuration / numFrames; // The time spent per frame, in seconds

		// Check if the target position has changed since the last frame.
		if (!isTargetSame) {
			// If it has changed, reset the elapsed time and current frame number to start a new animation.
			elapsedTime = 0.0f;
			currentFrame = 0;
		}

		// Calculate which frame should be updated based on the current elapsed time.
		elapsedTime += deltaTime;
		int targetFrame = (int)(elapsedTime / timePerFrame);
		targetFrame = glm::clamp(targetFrame, 0, numFrames - 1);

		// Create a set of control points to use for the spline curve that the arm will follow during the animation.
		std::vector<glm::vec3> controlPoints;

		// Get the end position of the IK chain and set it as the start position of the Catmull-Rom spline
		glm::vec3 startPos = m_end->getEndPosition();
		controlPoints.push_back(startPos);

		// Add the current target position as a control point
		controlPoints.push_back(m_target);

		// Calculate two additional control points that are a quarter and three-quarters of the way between the start position and the target position
		glm::vec3 quarterPoint = startPos + (m_target - startPos) * 0.25f;
		glm::vec3 threeQuarterPoint = startPos + (m_target - startPos) * 0.75f;

		// Add the calculated control points to the vector of control points
		controlPoints.push_back(quarterPoint);
		controlPoints.push_back(threeQuarterPoint);

		// Generate a set of spline points that the arm will follow during the animation.
		std::vector<glm::vec3> splinePoints;
		// Sample the spline by iterating over a range of values for t
		for (float t = 0.0f; t <= 1.0f; t += 0.01f) {
			// Evaluate the Catmull-Rom spline at the current value of t using the control points
			glm::vec3 point = glm::catmullRom(controlPoints[2], controlPoints[0], controlPoints[1], controlPoints[3], t);
			// Add the evaluated point to the vector of spline points
			splinePoints.push_back(point);
		}

		// Create a set of end positions to use for calculating the target position of the arm during the animation.
		std::vector<glm::vec3> endPositions;
		endPositions.push_back(m_end->getEndPosition());

		// Calculate the target position of the arm based on the current animation frame.
		glm::vec3 splinePoint = splinePoints[targetFrame];
		glm::vec3 targetPos = (1.0f - targetFrame / (float)numFrames) * endPositions[0] + (targetFrame / (float)numFrames) * splinePoint;

		// Update the IK animation to move the arm towards the target position.
		solveCCDSolution(10, 0.05f, targetPos);

		// If the current animation frame has changed, update the currentFrame variable.
		if (targetFrame != currentFrame) {
			currentFrame = targetFrame;
		}

		// Add the current end position of the arm to the set of end positions.
		endPositions.push_back(m_end->getEndPosition());
	}

	void solveCCDSolution(int maxIterations, float deltaThreshold, glm::vec3 targetPos) {

		// Start iteration
		for (int iteration = 0; iteration < maxIterations; iteration++) {
			m_endPos = m_end->getEndPosition();

			// Check if the end effector is close enough to the target
			glm::vec3 delta = targetPos - m_endPos;
			if (glm::length(delta) <= deltaThreshold) {
				return;
			}

			// Get current joint
			Node* curr = m_end;

			// Iterate over the joints in reverse order
			while (curr->m_pParent) {  // joint node instead of root node

				// Get current joint start position
				glm::vec3 startPos = curr->getWorldTransform()[3];

				// Compute the vector from the current joint to the end effector
				glm::vec3 jointToEnd = m_endPos - startPos;

				// Compute the vector from the current joint to the target
				glm::vec3 jointToTarget = targetPos - startPos;

				// Compute the angle between the two vectors
				float angle = atan2(glm::length(glm::cross(jointToEnd, jointToTarget)), glm::dot(jointToEnd, jointToTarget));
				glm::vec3 crossProduct = glm::cross(jointToEnd, jointToTarget);
				if (crossProduct.z < 0) angle = -angle;

				// Set the rotation around the z-axis
				curr->setRotation(glm::degrees(angle), 0, 0, 1);

				// Apply the current node's transform to endPos
				m_endPos = m_end->getEndPosition();

				// Check if the end effector is close enough to the target
				delta = targetPos - m_endPos;
				if (glm::length(delta) <= deltaThreshold) {
					return;
				}

				// Update current joint
				curr = curr->m_pParent;
			}
		}
	}

};



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
		aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_PreTransformVertices | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace
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
				const aiVector3D* vta = &(mesh->mTangents[v_i]);
				modelData.mTangents.push_back(vec3(vta->x, vta->y, vta->z));

				const aiVector3D* vbt = &(mesh->mBitangents[v_i]);
				modelData.mBitangents.push_back(vec3(vbt->x, vbt->y, vbt->z));
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
void generateObjectBufferMesh(std::vector < const char* > dataArray) {
	int width, height, nrChannels;
	unsigned char* data;
	int counter = 0;
	int texCounter = 0;

	loc1 = glGetAttribLocation(normalMappingProgramID, "vertex_position");
	loc2 = glGetAttribLocation(normalMappingProgramID, "vertex_normal");
	loc3 = glGetAttribLocation(normalMappingProgramID, "vertex_texture");
	loc4 = glGetAttribLocation(normalMappingProgramID, "aTangent");
	loc5 = glGetAttribLocation(normalMappingProgramID, "aBitangent");

	for (int i = 0; i < dataArray.size(); i++) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, VTO[texCounter]);
		glUniform1i(glGetUniformLocation(normalMappingProgramID, "diffuseMap"), 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, VTO[texCounter + 1]);
		glUniform1i(glGetUniformLocation(normalMappingProgramID, "normalMap"), 1);
		mesh_data = load_mesh(dataArray[i]);
		meshData.push_back(mesh_data);

		glGenBuffers(1, &VBO[counter]);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter]);
		glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mVertices[0], GL_STATIC_DRAW);

		glGenBuffers(1, &VBO[counter + 1]);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter + 1]);
		glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mNormals[0], GL_STATIC_DRAW);

		glGenBuffers(1, &VBO[counter + 2]);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter + 2]);
		glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec2), &mesh_data.mTextureCoords[0], GL_STATIC_DRAW);

		glGenBuffers(1, &VBO[counter + 3]);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter + 3]);
		glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mTangents[0], GL_STATIC_DRAW);

		glGenBuffers(1, &VBO[counter + 4]);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter + 4]);
		glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mBitangents[0], GL_STATIC_DRAW);

		glGenVertexArrays(1, &VAO[i]);
		glBindVertexArray(VAO[i]);

		glEnableVertexAttribArray(loc1);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter]);
		glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		glEnableVertexAttribArray(loc2);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter + 1]);
		glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		glEnableVertexAttribArray(loc3);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter + 2]);
		glVertexAttribPointer(loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);

		glEnableVertexAttribArray(loc4);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter + 3]);
		glVertexAttribPointer(loc4, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		glEnableVertexAttribArray(loc5);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter + 4]);
		glVertexAttribPointer(loc5, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		counter += 5;
		texCounter += 2;
	}
}

#pragma region TEXTURE_FUNCTIONS
unsigned int loadTexture(const char* texture, int i) {
	glGenTextures(1, &VTO[i]);
	int width, height, nrComponents;
	unsigned char* data = stbi_load(texture, &width, &height, &nrComponents, 0);

	GLenum format;

	cout << i << ":" << nrComponents << endl;
	if (nrComponents == 1)
		format = GL_RED;
	else if (nrComponents == 3)
		format = GL_RGB;
	else if (nrComponents == 4)
		format = GL_RGBA;

	glBindTexture(GL_TEXTURE_2D, VTO[i]);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(data);

	return VTO[i];
}

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

void drawText() {
	// Show Project Title
	drawText("Hierarchical Creatures", 3, glm::vec3(5.4, 5.5, 0));

	char array[30];
	// Draw display mode text: Forward Kinematics or Inverse Kinematics
	snprintf(array, sizeof(array), displayMode ? "Forward Kinematics" : "Inverse Kinematics");
	drawText(array, 2, glm::vec3(5.4f, -3.5f, 0.0f), glm::vec4(237.0 / 255, 208.0 / 255, 2.0 / 255, 1));

	// Draw out of touch tip£º Within distance, Not within distance. Only IK can show this text
	if (!displayMode) {
		snprintf(array, sizeof(array), isWithinDis ? "Within distance" : "Not within distance");
		drawText(array, 2, glm::vec3(5.4f, -2.5f, 0.0f), isWithinDis ? glm::vec4(.325, .8, .4, 1) : glm::vec4(1, .4, .4, 1));
	}
	else {
		if (modelType == "simple") {
			snprintf(array, sizeof(array), "Arm one: %1.1f", a_z[0]);
			drawText(array, 2, glm::vec3(3.4f, -4.5f, 0.0f));
			snprintf(array, sizeof(array), "Arm two: %1.1f", a_z[1]);
			drawText(array, 2, glm::vec3(7.4f, -4.5f, 0.0f));
		}
		else {
			snprintf(array, sizeof(array), "Left1: %1.1f", a_z[3]);
			drawText(array, 2, glm::vec3(2.4f, -4.5f, 0.0f));
			snprintf(array, sizeof(array), "Left2: %1.1f", a_z[4]);
			drawText(array, 2, glm::vec3(5.4f, -4.5f, 0.0f));
			snprintf(array, sizeof(array), "Left3: %1.1f", a_z[5]);
			drawText(array, 2, glm::vec3(8.4f, -4.5f, 0.0f));

			snprintf(array, sizeof(array), "Right1: %1.1f", a_z[0]);
			drawText(array, 2, glm::vec3(2.4f, -5.5f, 0.0f));
			snprintf(array, sizeof(array), "Right2: %1.1f", a_z[1]);
			drawText(array, 2, glm::vec3(5.4f, -5.5f, 0.0f));
			snprintf(array, sizeof(array), "Right3: %1.1f", a_z[2]);
			drawText(array, 2, glm::vec3(8.4f, -5.5f, 0.0f));
		}
	}
}

void drawSingleModel(int model_index, glm::mat4 s_model) {
	glUniformMatrix4fv(glGetUniformLocation(normalMappingProgramID, "proj"), 1, GL_FALSE, glm::value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(normalMappingProgramID, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(normalMappingProgramID, "model"), 1, GL_FALSE, glm::value_ptr(s_model));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, VTO[0]);
	glUniform1i(glGetUniformLocation(normalMappingProgramID, "diffuseMap"), 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, VTO[1]);
	glUniform1i(glGetUniformLocation(normalMappingProgramID, "normalMap"), 1);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, VTO[2]);
	glUniform1i(glGetUniformLocation(normalMappingProgramID, "depthMap"), 2);
	//glUniform1i(glGetUniformLocation(normalMappingProgramID, "mappingMode"), boneMode);

	glUniform1i(glGetUniformLocation(normalMappingProgramID, "uvScalar"), 1);

	glBindVertexArray(VAO[model_index]);
	glDrawArrays(GL_TRIANGLES, 0, meshData[model_index].mPointCount);
}

void drawNode(Node* node) {

	// Draw node here using the calculated model_matrix
	drawSingleModel(node->getMType(), node->getWorldTransform());

	for (Node* child : node->m_lChildren) {
		drawNode(child);
	}
}

void drawModels() {
	glUseProgram(normalMappingProgramID);

	// Camera 
	glUniform3fv(glGetUniformLocation(normalMappingProgramID, "lightPos"), 1, &lightPos[0]);
	glUniform3fv(glGetUniformLocation(normalMappingProgramID, "viewPos"), 1, &cameraPos[0]);

	float same_z = 0.0f;

	// ----------------------------------- Create nodes ----------------------------------------
	Node* root = new Node("root", 0);
	root->setTranslation(0.0f, -1.0f, same_z);

	Node* arm1 = new Node("arm1", 1);
	arm1->setTranslation(0.3f + offsetX, 0.9f + offsetY, same_z);
	arm1->setRotation(-90.0);
	root->addChild(arm1);

	Node* arm2 = new Node("arm2", 1);
	arm2->setTranslation(0.0f, .75f, same_z);
	arm1->addChild(arm2);

	Node* arm3 = new Node("arm3", 1);
	arm3->setTranslation(0.0f, 1.0f, same_z);

	Node* arm4 = new Node("arm4", 1);
	arm4->setTranslation(-1.0f, 3.8f, same_z);
	arm4->setRotation(90.0);

	Node* arm5 = new Node("arm5", 1);
	arm5->setTranslation(0.0f, 1.0f, same_z);

	Node* arm6 = new Node("arm6", 1);
	arm6->setTranslation(0.0f, 1.0f, same_z);

	if (modelType == "complex") {
		arm2->addChild(arm3);
		root->addChild(arm4);
		arm4->addChild(arm5);
		arm5->addChild(arm6);
	}

	// ------------------------------------- Set Target ------------------------------------------
	GLfloat target[2];
	bool isTouched = false;  // For FK to show text
	//target[0] = x_mouse, target[1] = y_mouse;
	target[0] = 1.0, target[1] = 1.0;

	// Draw the target model using its own global transform
	glm::mat4 target_model = glm::translate(model, glm::vec3(target[0], target[1], same_z));
	drawSingleModel(2, target_model);

	if (lastTarget == glm::vec3(target[0], target[1], same_z)) {
		isTargetSame = true;
	}
	else {
		isTargetSame = false;
		lastTarget = glm::vec3(target[0], target[1], same_z);
	}

	// ------------------------------------- Set Display Mode ------------------------------------
	if (displayMode) { // FK
		arm1->setRotation(a_z[0]);
		arm2->setRotation(a_z[1]);

		// Calculate if the end node has touched target point.
		float dis = glm::length(glm::vec3(target[0], target[1], same_z) - arm2->getEndPosition());
		isTouched = dis <= 0.1;

		if (modelType == "complex") {
			arm3->setRotation(a_z[2]);
			arm4->setRotation(a_z[3]);
			arm5->setRotation(a_z[4]);
			arm6->setRotation(a_z[5]);

			// Calculate if the end node has touched target point.
			float dis1 = glm::length(glm::vec3(target[0], target[1], same_z) - arm3->getEndPosition());
			float dis2 = glm::length(glm::vec3(target[0], target[1], same_z) - arm6->getEndPosition());
			isTouched = dis1 <= 0.1 && dis2 <= 0.1;
		}


	}
	else {  // IK
		// Solve IK
		SimpleJointIK ikSolver(arm1, arm3, 3);
		if (modelType == "simple") { // Analytical solution
			// Solve IK
			SimpleJointIK ikSolver(arm1, arm2, 2);

			ikSolver.setTargetPosition(glm::vec3(target[0], target[1], same_z));

			// Analytical solution only support 2 arms.
			 ikSolver.solveAnalyticalSolution();
		}
		else {
			// Solve IK
			SimpleJointIK ikSolver(arm1, arm3, 3);
			//SimpleJointIK ikSolver2(arm4, arm6, 3);

			ikSolver.setTargetPosition(glm::vec3(target[0], target[1], same_z));
			//ikSolver2.setTargetPosition(glm::vec3(target[0], target[1], same_z));

			// Solve IK with CCD solution and using spline animation
			ikSolver.animate(delta);
			//ikSolver2.animate(delta);
		}
	}

	// Draw scene
	drawNode(root); // Start with identity matrix as the parent model matrix

	// Draw isTouched text only in FK mode
	if (displayMode) drawText(isTouched ? "Touched!" : "", 3, glm::vec3(5.4f, -2.5f, 0.0f), glm::vec4(.325, .8, .4, 1));
}

void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -4.5f + cameraZ));
	view = glm::mat4(1.0f);
	float radian = glm::radians(-cameraRotateY * 3);
	view = glm::lookAt(glm::vec3(camera_dis * sin(radian), 0.0f, 5.4f + camera_dis * cos(radian)),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f));

	proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);
	model = glm::mat4(1.0f);

	cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
	lightPos = glm::vec3(0.0f, 0.0f, 2.0f);

	// ------------------------------------------------- SKYBOX -----------------------------------------------
	drawSkybox();

	// ------------------------------------------------- TEXT ------------------------------------------------- 
	drawText();

	// ------------------------------------------------- MODEL ------------------------------------------------ 
	drawModels();


	glutSwapBuffers();
}


void updateScene() {

	static DWORD last_time = 0;
	DWORD curr_time = timeGetTime();
	if (last_time == 0)
		last_time = curr_time;
	delta = (curr_time - last_time) * 0.001;
	last_time = curr_time;

	if (isModelRotate) {
		rotate_y += 10.0f * delta;
		rotate_y = fmodf(rotate_y, 360.0f);
	}

	if (isCameraRotate) {
		cameraRotateY += 10.0f * delta;
		cameraRotateY = fmodf(cameraRotateY, 360.0f);
	}

	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	// Set up the shaders
	normalMappingProgramID = CompileShaders("./shaders/mappingVertexShader.txt", "./shaders/mappingFragmentShader.txt");
	textShaderProgram = CompileShaders("./shaders/textVertexShader.txt", "./shaders/textFragmentShader.txt");
	skyboxShaderProgramID = CompileShaders("./shaders/skyboxVertexShader.txt", "./shaders/skyboxFragmentShader.txt");

	// Skybox
	generateSkybox();
	cubemapTexture = loadCubemap(faces);

	// Textures
	unsigned int brickWallDiffuseMap = loadTexture("./Textures/brick_basecolor.jpg", 0);

	// load mesh into a vertex buffer array
	dataArray.push_back("./models/body5.dae");
	dataArray.push_back("./models/arm1.dae");
	dataArray.push_back("./models/target.dae");

	//dataArray.push_back(TEAPOT_MESH);
	generateObjectBufferMesh(dataArray);
}

void keyPress(unsigned char key, int xmouse, int ymouse) {
	switch (key) {
		case('w'):
			//a_z[angleIndex - 1] += 5.0f;
			//boneMode == 1 ?
			//a_z[0] = 0.01f;
			offsetY += .1f;
			break;
		case('s'):
			//a_z[angleIndex - 1] -= 5.0f;
			offsetY -= .1f;
			break;
		case('a'):
			offsetX += .1f;
			break;
		case('d'):
			offsetX -= .1f;
			break;
		case('q'):
			offsetZ -= .1f;
			break;
		case('e'):
			offsetZ += .1f;
			break;
		case('r'):
			a_z[0] = 0.0f;
			a_z[1] = 0.0f;
			a_z[2] = 0.0f;
			a_z[3] = 0.0f;
			a_z[4] = 0.0f;
			a_z[5] = 0.0f;
			offsetY = 0.0f;
			offsetZ = 0.0f;
			rotate_y = 0.0f;
			cameraRotateY = 0.0f;
			camera_dis = 4.5f;
			break;
		case('v'):
			displayMode = !displayMode;
			break;
		case('t'):
			modelType = modelType == "simple" ? "complex" : "simple";
			break;
		case('m'):
			isModelRotate = !isModelRotate;
			break;
		case('c'):
			isCameraRotate = !isCameraRotate;
			break;
		case('1'):
			angleIndex = 1;
			break;
		case('2'):
			angleIndex = 2;
			break;
		case('3'):
			angleIndex = 3;
			break;
		case('4'):
			angleIndex = 4;
			break;
		case('5'):
			angleIndex = 5;
			break;
		case('6'):
			angleIndex = 6;
			break;
		}

	cout << offsetX << " " << offsetY << endl;
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
	x_mouse = (float)(x - width / 2) * 0.0091;
	y_mouse = (float)-(y - height / 2) * 0.0091;
};

void mouseWheel(int key, int wheeldir, int x, int y) {
	if (wheeldir == 1)
	{
		camera_dis -= 0.2f;
	}
	else if (wheeldir == -1) {
		camera_dis += 0.2f;
	}

	cout << "camera_dis: " << camera_dis << endl;
};

int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("Hierarchical Creatures");

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
