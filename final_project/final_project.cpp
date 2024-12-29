#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

// GLTF model loader
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <render/shader.h>

#include <vector>
#include <iostream>
#define _USE_MATH_DEFINES
#include <math.h>

static GLFWwindow *window;
static int windowWidth = 1024;
static int windowHeight = 768;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
static void cursor_callback(GLFWwindow *window, double xpos, double ypos);

// OpenGL camera view parameters
static glm::vec3 eye_center(-278.0f, 350.0f, 800.0f);
static glm::vec3 lookat(-278.0f, 350.0f, 0.0f);
static glm::vec3 up(0.0f, 1.0f, 0.0f);
static float FoV = 65.0f;
static float zNear = 10.0f;
static float zFar = 10500.0f;

// Lighting control 
const glm::vec3 wave500(0.0f, 255.0f, 146.0f);
const glm::vec3 wave600(255.0f, 190.0f, 0.0f);
const glm::vec3 wave700(205.0f, 0.0f, 0.0f);
static glm::vec3 lightIntensity = 12000.0f * (8.0f * wave500 + 15.6f * wave600 + 18.4f * wave700);
static glm::vec3 lightPosition(-1000.0f, 1800.0f, -275.0f);
static glm::vec3 lightTarget(-1000.0f, 0.0f, -275.0f);;

// Shadow mapping
static glm::vec3 lightUp(0, 0, 1);
static int shadowMapWidth = 0;
static int shadowMapHeight = 0;

GLuint shadowFBO;
GLuint depthTexture;

// TODO: set these parameters 
static float depthFoV = 100.0f;
static float depthNear = 10.0f;
static float depthFar = 7500.0f; 

// Mouse control variables
static double lastX = windowWidth / 2.0, lastY = windowHeight / 2.0;
static float yaw = -90.0f; // Horizontal rotation
static float pitch = 0.0f; // Vertical rotation
static float mouseSensitivity = 0.1f;

// Helper flag and function to save depth maps for debugging
static bool saveDepth = true;

static GLuint LoadTextureTileBox(const char *texture_file_path) {
    int w, h, channels;
    uint8_t* img = stbi_load(texture_file_path, &w, &h, &channels, 3);
    GLuint texture;
    glGenTextures(1, &texture);  
    glBindTexture(GL_TEXTURE_2D, texture);  

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (img) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
        glGenerateMipmap(GL_TEXTURE_2D);
		std::cout << "Texture loaded successfully: " << texture_file_path << std::endl;

    } else {
        std::cout << "Failed to load texture " << texture_file_path << std::endl;
    }
    stbi_image_free(img);

    return texture;
}

// This function retrieves and stores the depth map of the default frame buffer 
// or a particular frame buffer (indicated by FBO ID) to a PNG image.
static void saveDepthTexture(GLuint fbo, std::string filename) {
    int width = shadowMapWidth;
    int height = shadowMapHeight;
	if (shadowMapWidth == 0 || shadowMapHeight == 0) {
		width = windowWidth;
		height = windowHeight;
	}
    int channels = 3; 
    
    std::vector<float> depth(width * height);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadBuffer(GL_DEPTH_COMPONENT);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::vector<unsigned char> img(width * height * 3);
    for (int i = 0; i < width * height; ++i) img[3*i] = img[3*i+1] = img[3*i+2] = depth[i] * 255;

    stbi_write_png(filename.c_str(), width, height, channels, img.data(), width * channels);
}

// Set initial mouse position and capture mode
void setupMouseControl()
{
    glfwSetCursorPos(window, lastX, lastY); // Center the mouse initially
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Disable cursor for FPS-style control
}

// Struct for a simple GLTF mesh representation
struct GLTFMesh {
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    GLsizei indexCount;
};

struct Ground {

	GLfloat vertex_buffer_data[264] = {
		// Ground and Background
	    -3500.0f, 0.0f, -3500.0f,  // Bottom-left
		3500.0f, 0.0f, -3500.0f,   // Bottom-right
     	3500.0f, 0.0f,  3500.0f,  // Top-right
		-3500.0f, 0.0f,  3500.0f,  // Top-left

		// Top face
		-3500.0f, 7000.0f, -3500.0f,  // Bottom-left
		-3500.0f, 7000.0f,  3500.0f,  // Top-left
		3500.0f, 7000.0f,  3500.0f,  // Top-right
		3500.0f, 7000.0f, -3500.0f,  // Bottom-right

		// Front face
		-3500.0f, 0.0f,  3500.0f,  // Bottom-left
		-3500.0f, 7000.0f,  3500.0f,  // Top-left
		3500.0f, 7000.0f,  3500.0f,  // Top-right
		3500.0f, 0.0f,  3500.0f,  // Bottom-right

		// Back face
		-3500.0f, 0.0f, -3500.0f,  // Bottom-left
		-3500.0f, 7000.0f, -3500.0f,  // Top-left
		3500.0f, 7000.0f, -3500.0f,  // Top-right
		3500.0f, 0.0f, -3500.0f,  // Bottom-right

		// Left face
		-3500.0f, 0.0f, -3500.0f,  // Bottom-left
		-3500.0f, 7000.0f, -3500.0f,  // Top-left
		-3500.0f, 7000.0f,  3500.0f,  // Top-right
		-3500.0f, 0.0f,  3500.0f,  // Bottom-right

		// Right face
		3500.0f, 0.0f, -3500.0f,  // Bottom-left
		3500.0f, 7000.0f, -3500.0f,  // Top-left
		3500.0f, 7000.0f,  3500.0f,  // Top-right
		3500.0f, 0.0f,  3500.0f,  // Bottom-right

		// Building 1
		// Top face
		-200.0f, 1600.0f, -500.0f,  // Bottom-left
		-200.0f, 1600.0f,  500.0f,  // Top-left
		800.0f, 1600.0f,  500.0f,  // Top-right
		800.0f, 1600.0f, -500.0f,  // Bottom-right

		// Front face
		-200.0f, 0.0f,  500.0f,  // Bottom-left
		-200.0f, 1600.0f,  500.0f,  // Top-left
		800.0f, 1600.0f,  500.0f,  // Top-right
		800.0f, 0.0f,  500.0f,  // Bottom-right

		// Back face
		-200.0f, 0.0f, -500.0f,  // Bottom-left
		-200.0f, 1600.0f, -500.0f,  // Top-left
		800.0f, 1600.0f, -500.0f,  // Top-right
		800.0f, 0.0f, -500.0f,  // Bottom-right

		// Left face
		-200.0f, 0.0f, -500.0f,  // Bottom-left
		-200.0f, 1600.0f, -500.0f,  // Top-left
		-200.0f, 1600.0f,  500.0f,  // Top-right
		-200.0f, 0.0f,  500.0f,  // Bottom-right

		// Right face
		800.0f, 0.0f, -500.0f,  // Bottom-left
		800.0f, 1600.0f, -500.0f,  // Top-left
		800.0f, 1600.0f,  500.0f,  // Top-right
		800.0f, 0.0f,  500.0f,  // Bottom-right

		// Building 2
		// Top face
		-1200.0f - 350.0f, 1900.0f, -350.0f,  // Bottom-left
		-1200.0f - 350.0f, 1900.0f,  350.0f,  // Top-left
		-1200.0f + 350.0f, 1900.0f,  350.0f,  // Top-right
		-1200.0f + 350.0f, 1900.0f, -350.0f,  // Bottom-right

		// Front face
		-1200.0f - 350.0f, 0.0f,  350.0f,  // Bottom-left
		-1200.0f - 350.0f, 1900.0f,  350.0f,  // Top-left
		-1200.0f + 350.0f, 1900.0f,  350.0f,  // Top-right
		-1200.0f + 350.0f, 0.0f,  350.0f,  // Bottom-right

		// Back face
		-1200.0f - 350.0f, 0.0f, -350.0f,  // Bottom-left
		-1200.0f - 350.0f, 1900.0f, -350.0f,  // Top-left
		-1200.0f + 350.0f, 1900.0f, -350.0f,  // Top-right
		-1200.0f + 350.0f, 0.0f, -350.0f,  // Bottom-right

		// Left face
		-1200.0f - 350.0f, 0.0f, -350.0f,  // Bottom-left
		-1200.0f - 350.0f, 1900.0f, -350.0f,  // Top-left
		-1200.0f - 350.0f, 1900.0f,  350.0f,  // Top-right
		-1200.0f - 350.0f, 0.0f,  350.0f,  // Bottom-right

		// Right face
		-1200.0f + 350.0f, 0.0f, -350.0f,  // Bottom-left
		-1200.0f + 350.0f, 1900.0f, -350.0f,  // Top-left
		-1200.0f + 350.0f, 1900.0f,  350.0f,  // Top-right
		-1200.0f + 350.0f, 0.0f,  350.0f,  // Bottom-right
		
	};
	
	GLfloat normal_buffer_data[264] = {
		// Ground and background
		// Floor 
		0.0, 1.0, 0.0,   
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,

		// Ceiling
		0.0f, -1.0f, 0.0f,  // Bottom-left
		0.0f, -1.0f, 0.0f,  // Top-left
		0.0f, -1.0f, 0.0f,  // Top-right
		0.0f, -1.0f, 0.0f,  // Bottom-right

		// Front face
		0.0f, 0.0f, -1.0f,   // Bottom-left
		0.0f, 0.0f, -1.0f,   // Top-left
		0.0f, 0.0f, -1.0f,   // Top-right
		0.0f, 0.0f, -1.0f,   // Bottom-right

		// Back face
		0.0f, 0.0f, 1.0f,  // Bottom-left
		0.0f, 0.0f, 1.0f,  // Top-left
		0.0f, 0.0f, 1.0f,  // Top-right
		0.0f, 0.0f, 1.0f,  // Bottom-right

		// Left face
		1.0f, 0.0f, 0.0f,  // Bottom-left
		1.0f, 0.0f, 0.0f,  // Top-left
		1.0f, 0.0f, 0.0f,  // Top-right
		1.0f, 0.0f, 0.0f,  // Bottom-right

		// Right face
		-1.0f, 0.0f, 0.0f,   // Bottom-left
		-1.0f, 0.0f, 0.0f,   // Top-left
		-1.0f, 0.0f, 0.0f,   // Top-right
		-1.0f, 0.0f, 0.0f,   // Bottom-right

		// Building 1
		// Ceiling
		0.0f, 1.0f, 0.0f,  // Bottom-left
		0.0f, 1.0f, 0.0f,  // Top-left
		0.0f, 1.0f, 0.0f,  // Top-right
		0.0f, 1.0f, 0.0f,  // Bottom-right

		// Front face
		0.0f, 0.0f, -1.0f,   // Bottom-left
		0.0f, 0.0f, -1.0f,   // Top-left
		0.0f, 0.0f, -1.0f,   // Top-right
		0.0f, 0.0f, -1.0f,   // Bottom-right

		// Back face
		0.0f, 0.0f, 1.0f,  // Bottom-left
		0.0f, 0.0f, 1.0f,  // Top-left
		0.0f, 0.0f, 1.0f,  // Top-right
		0.0f, 0.0f, 1.0f,  // Bottom-right

		// Left face
		-1.0f, 0.0f, 0.0f,  // Bottom-left
		-1.0f, 0.0f, 0.0f,  // Top-left
		-1.0f, 0.0f, 0.0f,  // Top-right
		-1.0f, 0.0f, 0.0f,  // Bottom-right

		// Right face
		-1.0f, 0.0f, 0.0f,   // Bottom-left
		-1.0f, 0.0f, 0.0f,   // Top-left
		-1.0f, 0.0f, 0.0f,   // Top-right
		-1.0f, 0.0f, 0.0f,   // Bottom-right

		// Building 2
		// Ceiling
		0.0f, 1.0f, 0.0f,  // Bottom-left
		0.0f, 1.0f, 0.0f,  // Top-left
		0.0f, 1.0f, 0.0f,  // Top-right
		0.0f, 1.0f, 0.0f,  // Bottom-right

		// Front face
		0.0f, 0.0f, -1.0f,   // Bottom-left
		0.0f, 0.0f, -1.0f,   // Top-left
		0.0f, 0.0f, -1.0f,   // Top-right
		0.0f, 0.0f, -1.0f,   // Bottom-right

		// Back face
		0.0f, 0.0f, 1.0f,  // Bottom-left
		0.0f, 0.0f, 1.0f,  // Top-left
		0.0f, 0.0f, 1.0f,  // Top-right
		0.0f, 0.0f, 1.0f,  // Bottom-right

		// Left face
		1.0f, 0.0f, 0.0f,  // Bottom-left
		1.0f, 0.0f, 0.0f,  // Top-left
		1.0f, 0.0f, 0.0f,  // Top-right
		1.0f, 0.0f, 0.0f,  // Bottom-right

		// Right face
		-1.0f, 0.0f, 0.0f,   // Bottom-left
		-1.0f, 0.0f, 0.0f,   // Top-left
		-1.0f, 0.0f, 0.0f,   // Top-right
		-1.0f, 0.0f, 0.0f,   // Bottom-right

	};

	GLfloat color_buffer_data[264] = {
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

	};

	GLuint index_buffer_data[96] = {
		// ground and background
		// Botton
		2, 1, 0, 	
		3, 2, 0,

		// Top face
		6, 5, 4,
		7, 6, 4,

		// Front face
		8, 9, 10,
		8, 10, 11,

		// Back face
		14, 13, 12,
		15, 14, 12,

		// Left face
		16, 17, 18,
		16, 18, 19,

		// Right face
		22, 21, 20,
		23, 22, 20,

		// Building 1
		24, 25, 26,
		24, 26, 27,

		30, 29, 28,
		31, 30, 28,

		32, 33, 34,
		32, 34, 35,

		38, 37, 36,
		39, 38, 36,

		40, 41, 42,
		40, 42, 43,

		// Building 2
		44, 45, 46,
		44, 46, 47,

		50, 49, 48,
		51, 50, 48,

		52, 53, 54,
		52, 54, 55,

		58, 57, 56,
		59, 58, 56,

		60, 61, 62,
		60, 62, 63,

	};	

	GLfloat uv_buffer_data[128] = {
		8.0f,  8.0f,    // Top-right
		0.0f,  8.0f,    // Top-left
		0.0f,  0.0f,    // Bottom-left
		8.0f,  0.0f,    // Bottom-right

		0.5f,  0.333f,  // Top-right
		0.25f, 0.333f,  // Top-left
		0.25f, 0.0f,    // Bottom-left
		0.5f,  0.0f,    // Bottom-right

		0.25f, 0.666f,  // Top-right
		0.25f, 0.333f,  // Bottom-right
		0.0f,  0.333f,  // Bottom-left
		0.0f,  0.666f,  // Top-left
		
		0.5f,  0.666f,  // Top-left
		0.5f,  0.333f,  // Bottom-left
		0.75f, 0.333f,  // Bottom-right
		0.75f, 0.666f,  // Top-right y

		0.5f,  0.666f,  // Top-right
		0.5f,  0.333f,  // Bottom-right
		0.25f, 0.333f,  // Bottom-left
		0.25f, 0.666f,  // Top-left y

		0.75f, 0.666f,  // Top-left
		0.75f, 0.333f,  // Bottom-left
		1.0f,  0.333f,  // Bottom-right
		1.0f,  0.666f,  // Top-right y

		// Building 1
		// Building top
		0.1f,  0.1f,  // Top-right
		0.1f,  0.0f,  // Bottom-right
		0.0f,  0.0f,  // Bottom-left
		0.0f,  0.1f,  // Top-left

		1.0f,  3.0f,  // Top-right
		1.0f,  0.0f,  // Bottom-right
		0.0f,  0.0f,  // Bottom-left
		0.0f,  3.0f,  // Top-left
		
		0.0f,  3.0f,  // Top-left
		0.0f,  0.0f,  // Bottom-left
		1.0f,  0.0f,  // Bottom-right
		1.0f,  3.0f,  // Top-right y

		0.0f,  0.0f,  // Bottom-left
		0.0f,  3.0f,  // Top-left y
		1.0f,  3.0f,  // Top-right
		1.0f,  0.0f,  // Bottom-right

		0.0f,  3.0f,  // Top-left
		0.0f,  0.0f,  // Bottom-left
		1.0f,  0.0f,  // Bottom-right
		1.0f,  3.0f,  // Top-right y

		// Building 2
		// Building top
		0.1f,  0.1f,  // Top-right
		0.1f,  0.0f,  // Bottom-right
		0.0f,  0.0f,  // Bottom-left
		0.0f,  0.1f,  // Top-left

		1.0f,  2.0f,  // Top-right
		1.0f,  0.0f,  // Bottom-right
		0.0f,  0.0f,  // Bottom-left
		0.0f,  2.0f,  // Top-left
		
		0.0f,  2.0f,  // Top-left
		0.0f,  0.0f,  // Bottom-left
		1.0f,  0.0f,  // Bottom-right
		1.0f,  2.0f,  // Top-right y

		0.0f,  0.0f,  // Bottom-left
		0.0f,  2.0f,  // Top-left y
		1.0f,  2.0f,  // Top-right
		1.0f,  0.0f,  // Bottom-right

		0.0f,  2.0f,  // Top-left
		0.0f,  0.0f,  // Bottom-left
		1.0f,  0.0f,  // Bottom-right
		1.0f,  2.0f,  // Top-right y
	};

	// OpenGL buffers
	GLuint vertexArrayID; 
	GLuint vertexBufferID; 
	GLuint indexBufferID; 
	GLuint colorBufferID;
	GLuint normalBufferID;
	GLuint uvBufferID;
	GLuint backgroundTextureID;
	GLuint groundTextureID;
	GLuint building1TextureID;
	GLuint building2TextureID;

	// Shader variable IDs
	GLuint mvpMatrixID;
	GLuint textureSamplerID;
	GLuint lightPositionID;
	GLuint lightIntensityID;
	GLuint programID;

	GLuint depthProgramID;
	GLuint depthMVPMatrixID;
	GLuint shadowMapID;
	GLuint lightSpaceMatrixID;

	// Robot meshes
    // std::vector<GLTFMesh> robotMeshes;

	void initialize() {
		for (int i = 0; i < 132; ++i) color_buffer_data[i] = 1.0f;
		// Create a vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create a vertex buffer object to store the vertex data		
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the color data
		glGenBuffers(1, &colorBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(color_buffer_data), color_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the vertex normals		
		glGenBuffers(1, &normalBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(normal_buffer_data), normal_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the UV data
		glGenBuffers(1, &uvBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data,
		GL_STATIC_DRAW);
		
		// Create an index buffer object to store the index data that defines triangle faces
		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

        glGenFramebuffers(1, &shadowFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);

        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowMapWidth, shadowMapHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromFile("/Users/selinawang/Downloads/Graphics Final Project/final_project/scene.vert", "/Users/selinawang/Downloads/Graphics Final Project/final_project/scene.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}

		std::string texturePath = "/Users/selinawang/Downloads/Graphics Final Project/final_project/texture/road.png";
		groundTextureID = LoadTextureTileBox(texturePath.c_str());

		texturePath = "/Users/selinawang/Downloads/Graphics Final Project/final_project/texture/star.png";
		backgroundTextureID = LoadTextureTileBox(texturePath.c_str());  // Convert string to C-style string

		texturePath = "/Users/selinawang/Downloads/Graphics Final Project/final_project/texture/building1.png";
		building1TextureID = LoadTextureTileBox(texturePath.c_str());

		texturePath = "/Users/selinawang/Downloads/Graphics Final Project/final_project/texture/building2.png";
		building2TextureID = LoadTextureTileBox(texturePath.c_str());

		textureSamplerID = glGetUniformLocation(programID,"textureSampler");

		// Get a handle for our "MVP" uniform
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		lightPositionID = glGetUniformLocation(programID, "lightPosition");
		lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
		shadowMapID = glGetUniformLocation(programID, "shadowMap");
		lightSpaceMatrixID = glGetUniformLocation(programID, "lightSpaceMatrix");

		// Create and compile GLSL program for depth rendering (shadow mapping)
		depthProgramID = LoadShadersFromFile("/Users/selinawang/Downloads/Graphics Final Project/final_project/depth.vert", "/Users/selinawang/Downloads/Graphics Final Project/final_project/depth.frag");
		if (depthProgramID == 0) {
			std::cerr << "Failed to load depth shaders." << std::endl;
		}
		depthMVPMatrixID = glGetUniformLocation(depthProgramID, "lightSpaceMatrix");
	}

	void render(glm::mat4 cameraMatrix, glm::mat4 lightSpaceMatrix) {
		glUseProgram(programID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		// Set model-view-projection matrix
		glm::mat4 mvp = cameraMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		glEnableVertexAttribArray(3);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 0, 0);

		// Set light data 
		glUniform3fv(lightPositionID, 1, &lightPosition[0]);
		glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);

		// Pass light-space matrix to shader
		glUniformMatrix4fv(lightSpaceMatrixID, 1, GL_FALSE, &lightSpaceMatrix[0][0]);

		// Bind depth texture to texture unit 0
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthTexture);
		glUniform1i(shadowMapID, 1); // Tell the shader that the shadow map is on texture unit 0

		// Set light data
		glUniform3fv(lightPositionID, 1, &lightPosition[0]);
		glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, groundTextureID);
		glUniform1i(textureSamplerID, 0); 
		glDrawElements(
			GL_TRIANGLES,      // mode
			6,    			   // number of indices
			GL_UNSIGNED_INT,   // type
			(void*)(0 * sizeof(GLuint))  // element array buffer offset
		);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, backgroundTextureID);
		glUniform1i(textureSamplerID, 0); 
		// Draw the box
		glDrawElements(
			GL_TRIANGLES,      // mode
			30,    			   // number of indices
			GL_UNSIGNED_INT,   // type
			(void*)(6 * sizeof(GLuint))   // element array buffer offset
		);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, building1TextureID);
		glUniform1i(textureSamplerID, 0); 
		// Draw the box
		glDrawElements(
			GL_TRIANGLES,      // mode
			30,    			   // number of indices
			GL_UNSIGNED_INT,   // type
			(void*)(36 * sizeof(GLuint))   // element array buffer offset
		);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, building2TextureID);
		glUniform1i(textureSamplerID, 0); 
		// Draw the box
		glDrawElements(
			GL_TRIANGLES,      // mode
			30,    			   // number of indices
			GL_UNSIGNED_INT,   // type
			(void*)(66 * sizeof(GLuint))   // element array buffer offset
		);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
		glDisableVertexAttribArray(3);
	}

	void renderDepth(glm::mat4 lightSpaceMatrix) {
		glUseProgram(depthProgramID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		// Set light-space matrix
		glUniformMatrix4fv(depthMVPMatrixID, 1, GL_FALSE, &lightSpaceMatrix[0][0]);

		glDrawElements(
			GL_TRIANGLES,
			90,
			GL_UNSIGNED_INT,
			(void*)0
		);

		glDisableVertexAttribArray(0);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &colorBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteBuffers(1, &normalBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		glDeleteBuffers(1, &uvBufferID);
		glDeleteTextures(1, &backgroundTextureID);
		glDeleteTextures(1, &groundTextureID);
		glDeleteTextures(1, &building1TextureID);
		glDeleteTextures(1, &building2TextureID);
		glDeleteProgram(programID);
		glDeleteProgram(depthProgramID);
	}
}; 


struct UFO {

	float rotationAngle = 0.0f;

	GLfloat vertex_buffer_data[72] = {
		// Front face
		2000.0f, 1400.0f,  200.0f,  // Bottom-left
		2400.0f, 1400.0f,  200.0f,  // Bottom-right
		2400.0f, 1800.0f, 200.0f,  // Top-right
		2000.0f, 1800.0f, 200.0f,  // Top-left

		// Back face
		2000.0f, 1400.0f, -200.0f,  // Bottom-left
		2400.0f, 1400.0f, -200.0f,  // Bottom-right
		2400.0f, 1800.0f, -200.0f, // Top-right
		2000.0f, 1800.0f, -200.0f, // Top-left

		// Left face
		2000.0f, 1400.0f, -200.0f,  // Bottom-left
		2000.0f, 1400.0f,  200.0f,  // Bottom-right
		2000.0f, 1800.0f, 200.0f,  // Top-right
		2000.0f, 1800.0f, -200.0f, // Top-left

		// Right face
		2400.0f, 1400.0f, -200.0f,  // Bottom-left
		2400.0f, 1400.0f,  200.0f,  // Bottom-right
		2400.0f, 1800.0f, 200.0f,  // Top-right
		2400.0f, 1800.0f, -200.0f, // Top-left

		// Top face
		2000.0f, 1800.0f, 200.0f,  // Bottom-left
		2400.0f, 1800.0f, 200.0f,  // Bottom-right
		2400.0f, 1800.0f, -200.0f, // Top-right
		2000.0f, 1800.0f, -200.0f, // Top-left

		// Bottom face
		2000.0f, 1400.0f, 200.0f,   // Bottom-left
		2400.0f, 1400.0f, 200.0f,   // Bottom-right
		2400.0f, 1400.0f, -200.0f,  // Top-right
		2000.0f, 1400.0f, -200.0f   // Top-left
	};
	
	GLfloat normal_buffer_data[72] = {    
		// Front face (facing +Z)
		0.0f, 0.0f,  -1.0f,  // Bottom-left
		0.0f, 0.0f,  -1.0f,  // Bottom-right
		0.0f, 0.0f,  -1.0f,  // Top-right
		0.0f, 0.0f,  -1.0f,  // Top-left

		// Back face (facing -Z)
		0.0f, 0.0f, -1.0f,  // Bottom-left
		0.0f, 0.0f, -1.0f,  // Bottom-right
		0.0f, 0.0f, -1.0f,  // Top-right
		0.0f, 0.0f, -1.0f,  // Top-left

		// Left face (facing -X)
		-1.0f, 0.0f, 0.0f,  // Bottom-left
		-1.0f, 0.0f, 0.0f,  // Bottom-right
		-1.0f, 0.0f, 0.0f,  // Top-right
		-1.0f, 0.0f, 0.0f,  // Top-left

		// Right face (facing +X)
		-1.0f, 0.0f, 0.0f,   // Bottom-left
		-1.0f, 0.0f, 0.0f,   // Bottom-right
		-1.0f, 0.0f, 0.0f,   // Top-right
		-1.0f, 0.0f, 0.0f,   // Top-left

		// Top face (facing +Y)
		0.0f, -1.0f, 0.0f,   // Bottom-left
		0.0f, -1.0f, 0.0f,   // Bottom-right
		0.0f, -1.0f, 0.0f,   // Top-right
		0.0f, -1.0f, 0.0f,   // Top-left

		// Bottom face (facing -Y)
		0.0f, 1.0f, 0.0f,  // Bottom-left
		0.0f, 1.0f, 0.0f,  // Bottom-right
		0.0f, 1.0f, 0.0f,  // Top-right
		0.0f, 1.0f, 0.0f   // Top-left
	};

	GLfloat color_buffer_data[72] = {
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f
	};

	GLuint index_buffer_data[96] = {
		// Front face (+Z)
		0, 1, 2,  // First triangle
		0, 2, 3,  // Second triangle

		// Back face (-Z)
		6, 5, 4,  // First triangle
		7, 6, 4,  // Second triangle y

		// Left face (-X)
		8, 9, 10,  // First triangle
		8, 10, 11, // Second triangle y

		// Right face (+X)
		14, 13, 12, // First triangle
		15, 14, 12, // Second triangle

		// Top face (+Y)
		16, 17, 18, // First triangle
		16, 18, 19, // Second triangle

		// Bottom face (-Y)
		22, 21, 20, // First triangle
		23, 22, 20  // Second triangle
	};

	GLfloat uv_buffer_data[128] = {
		// Front face (+Z)
		0.0f, 0.0f,  // Bottom-left
		1.0f, 0.0f,  // Bottom-right
		1.0f, 1.0f,  // Top-right
		0.0f, 1.0f,  // Top-left

		// Back face (-Z)
		0.0f, 0.0f,  // Bottom-left
		1.0f, 0.0f,  // Bottom-right
		1.0f, 1.0f,  // Top-right
		0.0f, 1.0f,  // Top-left

		// Left face (-X)
		0.0f, 0.0f,  // Bottom-left
		1.0f, 0.0f,  // Bottom-right
		1.0f, 1.0f,  // Top-right
		0.0f, 1.0f,  // Top-left

		// Right face (+X)
		0.0f, 0.0f,  // Bottom-left
		1.0f, 0.0f,  // Bottom-right
		1.0f, 1.0f,  // Top-right
		0.0f, 1.0f,  // Top-left

		// Top face (+Y)
		0.0f, 0.0f,  // Bottom-left
		0.1f, 0.0f,  // Bottom-right
		0.1f, 0.1f,  // Top-right
		0.0f, 0.1f,  // Top-left

		// Bottom face (-Y)
		0.0f, 0.0f,  // Bottom-left
		0.1f, 0.0f,  // Bottom-right
		0.1f, 0.1f,  // Top-right
		0.0f, 0.1f   // Top-left
	};

	// OpenGL buffers
	GLuint vertexArrayID; 
	GLuint vertexBufferID; 
	GLuint indexBufferID; 
	GLuint colorBufferID;
	GLuint normalBufferID;
	GLuint uvBufferID;
	GLuint textureID;

	// Shader variable IDs
	GLuint mvpMatrixID;
	GLuint textureSamplerID;
	GLuint lightPositionID;
	GLuint lightIntensityID;
	GLuint programID;

	GLuint lightSpaceMatrixID;

	void initialize() {
		for (int i = 0; i < 72; ++i) color_buffer_data[i] = 1.0f;
		// Create a vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create a vertex buffer object to store the vertex data		
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the color data
		glGenBuffers(1, &colorBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(color_buffer_data), color_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the vertex normals		
		glGenBuffers(1, &normalBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(normal_buffer_data), normal_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the UV data
		glGenBuffers(1, &uvBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data,
		GL_STATIC_DRAW);
		
		// Create an index buffer object to store the index data that defines triangle faces
		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromFile("/Users/selinawang/Downloads/Graphics Final Project/final_project/scene.vert", "/Users/selinawang/Downloads/Graphics Final Project/final_project/scene.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}

		std::string texturePath = "/Users/selinawang/Downloads/Graphics Final Project/final_project/texture/UFO.png";
		textureID = LoadTextureTileBox(texturePath.c_str());

		textureSamplerID = glGetUniformLocation(programID,"textureSampler");

		// Get a handle for our "MVP" uniform
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		lightPositionID = glGetUniformLocation(programID, "lightPosition");
		lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
		lightSpaceMatrixID = glGetUniformLocation(programID, "lightSpaceMatrix");
	}

	void render(glm::mat4 vpMatrix, glm::mat4 lightSpaceMatrix) {
		glUseProgram(programID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		// Set model-view-projection matrix
		
        glm::mat4 modelMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(rotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 mvp = vpMatrix * modelMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		glEnableVertexAttribArray(3);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 0, 0);

		// Set light data 
		glUniform3fv(lightPositionID, 1, &lightPosition[0]);
		glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);

		// Pass light-space matrix to shader
		glUniformMatrix4fv(lightSpaceMatrixID, 1, GL_FALSE, &lightSpaceMatrix[0][0]);

		// Set light data
		glUniform3fv(lightPositionID, 1, &lightPosition[0]);
		glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glUniform1i(textureSamplerID, 0); 
		glDrawElements(
			GL_TRIANGLES,      // mode
			36,    			   // number of indices
			GL_UNSIGNED_INT,   // type
			(void*)(0 * sizeof(GLuint))  // element array buffer offset
		);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
		glDisableVertexAttribArray(3);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &colorBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteBuffers(1, &normalBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		glDeleteBuffers(1, &uvBufferID);
		glDeleteTextures(1, &textureID);
		glDeleteProgram(programID);
	}
}; 

/*
// Function to load a GLTF model
std::vector<GLTFMesh> LoadGLTFModel(const std::string &path) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    if (!loader.LoadASCIIFromFile(&model, &err, &warn, path)) {
        std::cerr << "Failed to load GLTF: " << err << std::endl;
        return {};
    }

    if (!warn.empty()) {
        std::cerr << "GLTF Warning: " << warn << std::endl;
    }

    std::vector<GLTFMesh> meshes;

    // Iterate over meshes in the model
    for (const auto &mesh : model.meshes) {
        for (const auto &primitive : mesh.primitives) {
            GLTFMesh gltfMesh;

            // Extract vertex positions
            const auto &posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
            const auto &posBufferView = model.bufferViews[posAccessor.bufferView];
            const auto &posBuffer = model.buffers[posBufferView.buffer];
            const float *positions = reinterpret_cast<const float *>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);
            size_t posCount = posAccessor.count;

            // Log vertex positions
            std::cout << "Vertex Positions:" << std::endl;
            for (size_t i = 0; i < posCount; ++i) {
                std::cout << "  [" << i << "] "
                          << positions[i * 3 + 0] << ", "
                          << positions[i * 3 + 1] << ", "
                          << positions[i * 3 + 2] << std::endl;
            }

            // Extract indices
            const auto &indexAccessor = model.accessors[primitive.indices];
            const auto &indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const auto &indexBuffer = model.buffers[indexBufferView.buffer];
            const unsigned short *indices = reinterpret_cast<const unsigned short *>(&indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset]);
            size_t indexCount = indexAccessor.count;

            gltfMesh.indexCount = static_cast<GLsizei>(indexCount);

            // Log index data
            std::cout << "Indices:" << std::endl;
            for (size_t i = 0; i < indexCount; ++i) {
                std::cout << "  [" << i << "] " << indices[i] << std::endl;
            }

            // Generate and bind VAO
            glGenVertexArrays(1, &gltfMesh.vao);
            glBindVertexArray(gltfMesh.vao);

            // Generate and bind VBO for positions
            glGenBuffers(1, &gltfMesh.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, gltfMesh.vbo);
            glBufferData(GL_ARRAY_BUFFER, posCount * 3 * sizeof(float), positions, GL_STATIC_DRAW);

            // Enable vertex attribute for positions
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
            glEnableVertexAttribArray(0);

            // Generate and bind EBO for indices
            glGenBuffers(1, &gltfMesh.ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gltfMesh.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned short), indices, GL_STATIC_DRAW);

            glBindVertexArray(0);

            meshes.push_back(gltfMesh);
        }
    }

    return meshes;
}

// Function to render meshes
void RenderGLTFModel(const std::vector<GLTFMesh> &meshes, const glm::mat4 &vpMatrix) {
    glm::mat4 modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(20.0f)); // Scale by 100 times
    glm::mat4 mvpMatrix = vpMatrix * modelMatrix; // Combine view-projection with model matrix

    // Bind shader and set the uniform
    GLuint programID = LoadShadersFromFile("/Users/selinawang/Downloads/Graphics Final Project/final_project/robot.vert", "/Users/selinawang/Downloads/Graphics Final Project/final_project/robot.frag");
    glUseProgram(programID);

    GLint mvpLocation = glGetUniformLocation(programID, "uMVP");
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, glm::value_ptr(mvpMatrix));

    for (const auto &mesh : meshes) {
        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_SHORT, nullptr);
        glBindVertexArray(0);
    }
    
    glUseProgram(0);
*/

int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW." << std::endl;
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // For MacOS
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(windowWidth, windowHeight, "Final Project", NULL, NULL);
	if (window == NULL)
	{
		std::cerr << "Failed to open a GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetKeyCallback(window, key_callback);

	setupMouseControl();
	glfwSetCursorPosCallback(window, cursor_callback);

	// Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0)
	{
		std::cerr << "Failed to initialize OpenGL context." << std::endl;
		return -1;
	}

	// Prepare shadow map size for shadow mapping. Usually this is the size of the window itself, but on some platforms like Mac this can be 2x the size of the window. Use glfwGetFramebufferSize to get the shadow map size properly. 
    glfwGetFramebufferSize(window, &shadowMapWidth, &shadowMapHeight);

	// Background
	glClearColor(0.2f, 0.2f, 0.25f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

    // Create the ground plane
	Ground b;
	b.initialize();

	UFO u;
	u.initialize();

	/*
	// Load the GLTF model
    std::string gltfFilePath = "/Users/selinawang/Downloads/glTF test/final_project/model/Robot_dog.gltf";
    auto meshes = LoadGLTFModel(gltfFilePath);
*/
    glm::mat4 viewMatrix, projectionMatrix;
	projectionMatrix = glm::perspective(glm::radians(FoV), (float)windowWidth / windowHeight, zNear, zFar);
	
	do
	{
		// First pass: Render depth to the FBO
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Set up light's view and projection matrix
		glm::mat4 lightProjection = glm::perspective(glm::radians(depthFoV), (float)windowWidth / windowHeight, depthNear, depthFar);
		glm::mat4 lightView = glm::lookAt(lightPosition, lightTarget, lightUp);
		glm::mat4 lightSpaceMatrix = lightProjection * lightView;

		b.renderDepth(lightSpaceMatrix);

		// Save the depth texture from the light's perspective (shadowFBO)
		if (saveDepth) {
			std::string lightFilename = "depth_light.png";
			saveDepthTexture(shadowFBO, lightFilename);
			std::cout << "Depth texture from light's perspective saved to " << lightFilename << std::endl;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Second pass: Render the scene to the default framebuffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		viewMatrix = glm::lookAt(eye_center, lookat, up);
		glm::mat4 vp = projectionMatrix * viewMatrix;

		// RenderGLTFModel(meshes,vp);
		b.render(vp, lightSpaceMatrix);

		// Increment the UFO's rotation angle
    	u.rotationAngle += 0.12f; // Adjust speed as needed
    	if (u.rotationAngle >= 360.0f) u.rotationAngle -= 360.0f;
		u.render(vp, lightSpaceMatrix);

		if (saveDepth) {
            std::string filename = "depth_camera.png";
            saveDepthTexture(0, filename);
            std::cout << "Depth texture from camera's perspective saved to " << filename << std::endl;
            saveDepth = false;
        }

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (!glfwWindowShouldClose(window));

	// Clean up
	b.cleanup();
	u.cleanup();

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
	static float moveSpeed = 80.0f;
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
	{
		eye_center = glm::vec3(-278.0f, 273.0f, 800.0f);
		lightPosition = glm::vec3(-275.0f, 500.0f, -275.0f);
	}

	if ((key == GLFW_KEY_UP || key == GLFW_KEY_W) && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
        glm::vec3 forward = glm::normalize(lookat - eye_center);
		forward.y = 0; // Keep movement horizontal
        eye_center += forward * moveSpeed;
        lookat += forward * moveSpeed;
	}

	if ((key == GLFW_KEY_DOWN || key == GLFW_KEY_S) && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
        glm::vec3 backward = glm::normalize(eye_center - lookat);
		backward.y = 0; // Keep movement horizontal
        eye_center += backward * moveSpeed;
        lookat += backward * moveSpeed;
	}

	if ((key == GLFW_KEY_LEFT || key == GLFW_KEY_A) && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
        glm::vec3 left = glm::normalize(glm::cross(up, lookat - eye_center));
        eye_center += left * moveSpeed;
        lookat += left * moveSpeed;
	}

	if ((key == GLFW_KEY_RIGHT || key == GLFW_KEY_D) && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
        glm::vec3 right = glm::normalize(glm::cross(lookat - eye_center, up));
        eye_center += right * moveSpeed;
        lookat += right * moveSpeed;
	}
	if (key == GLFW_KEY_SPACE && (action == GLFW_REPEAT || action == GLFW_PRESS)) 
    {
        saveDepth = true;
    }

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

static void cursor_callback(GLFWwindow *window, double xpos, double ypos) {
    // Calculate mouse offsets
    float xOffset = (float)(xpos - lastX) * mouseSensitivity;
    float yOffset = (float)(lastY - ypos) * mouseSensitivity; // Reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    // Update yaw and pitch
    yaw += xOffset;
    pitch += yOffset;

    // Constrain pitch to avoid screen flipping
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    // Calculate the new direction vector
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    lookat = eye_center + glm::normalize(front);
}
