#include <glew.h>
#include <glfw3.h>
#include <glm.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>

// Constants
const int NUM_PARTICLES = 1000; // Number of particles
const int WORK_GROUP_SIZE = 10; // Ensure this matches the compute shader's local_size_x

// Particle structure
struct Particle {
    glm::vec2 position; // Add position attribute
    glm::vec2 velocity; // Add velocity attribute
    glm::vec4 color;    // Add color attribute
    float age;          // Add age attribute
    float lifeTime;     // Add lifetime attribute
};

std::vector<Particle> particles(NUM_PARTICLES); // Vector of particles

// Function prototypes
std::string ReadShaderFile(const std::string& shaderPath);
GLuint CompileShader(const std::string& source, GLenum shaderType);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

// Global variable for mouse position
glm::vec2 mousePos;

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create a windowed mode window and its OpenGL context
    GLFWwindow* window = glfwCreateWindow(640, 480, "Compute Shader Particle System", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = GL_TRUE; // Needed for core profile
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // Set the mouse callback
    glfwSetCursorPosCallback(window, mouse_callback);

    // Using std::chrono for high-precision time
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    float deltaTime = 0.0f;

    std::random_device rd;  // Obtain a seed from a random device (e.g., entropy source)
    std::mt19937 eng(rd()); // Initialize Mersenne Twister with the seed

    std::uniform_real_distribution<> distr(-1.0, 1.0); // Range for random direction
    std::uniform_real_distribution<> colorDistr(0.0, 1.0); // Range for random color
    std::uniform_real_distribution<float> lifeTimeDistr(1.5f, 3.0f); // Lifetime between 1 and 5 seconds

    // Initialize particles
    for (auto& particle : particles) {
        float dirX = distr(eng); // Random direction
        float dirY = distr(eng);  // Random direction
        glm::vec2 direction = glm::normalize(glm::vec2(dirX, dirY));

        particle.position = glm::vec2(20,20); // Start at the mouse position

        // Cast to float to ensure the whole expression is treated as a float
        float randomSpeedFactor = 0.2f + 0.005f;
        particle.velocity = direction * randomSpeedFactor;
        particle.color = glm::vec4(colorDistr(eng), colorDistr(eng), colorDistr(eng), 1.0f); // Random color

        particle.age = 0.0f; // Start at age 0
        particle.lifeTime = lifeTimeDistr(eng); // Assign random lifetime
    }

    // Load and compile compute shader
    std::string computeShaderSource = ReadShaderFile("compute_shader.glsl"); // Read compute shader file
    GLuint computeShader = CompileShader(computeShaderSource, GL_COMPUTE_SHADER); // Compile compute shader

    GLint success; // Check for shader compile errors
    GLchar infoLog[512]; // Check for shader compile errors
    glGetShaderiv(computeShader, GL_COMPILE_STATUS, &success); 
    if (!success) { 
        glGetShaderInfoLog(computeShader, 512, NULL, infoLog);
        std::cerr << "ERROR::COMPUTESHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint computeShaderProgram = glCreateProgram(); // Create compute shader program
    glAttachShader(computeShaderProgram, computeShader); // Attach compute shader
    glLinkProgram(computeShaderProgram); // Link compute shader program

    glGetProgramiv(computeShaderProgram, GL_LINK_STATUS, &success); // Check for linking errors
    if (!success) {
        glGetProgramInfoLog(computeShaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::COMPUTEPROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    // Load and compile vertex and fragment shaders
    std::string vertexShaderSource = ReadShaderFile("vertex_shader.glsl");
    std::string fragmentShaderSource = ReadShaderFile("fragment_shader.glsl");
    GLuint vertexShader = CompileShader(vertexShaderSource, GL_VERTEX_SHADER);

    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success); // Check for shader compile errors
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::VERTEXSHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint fragmentShader = CompileShader(fragmentShaderSource, GL_FRAGMENT_SHADER); // Compile fragment shader

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success); // Check for shader compile errors
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::FRAGMENTSHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint renderShaderProgram = glCreateProgram(); // Create render shader program
    glAttachShader(renderShaderProgram, vertexShader); // Attach vertex shader
    glAttachShader(renderShaderProgram, fragmentShader); // Attach fragment shader
    glLinkProgram(renderShaderProgram); // Link render shader program

    glGetProgramiv(renderShaderProgram, GL_LINK_STATUS, &success); // Check for linking errors
    if (!success) {
        glGetProgramInfoLog(renderShaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::RENDERPROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    // Create the SSBO for particles
    GLuint particleSSBO;  
    glGenBuffers(1, &particleSSBO); // Create SSBO
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);  // Bind SSBO
    glBufferData(GL_SHADER_STORAGE_BUFFER, particles.size() * sizeof(Particle), particles.data(), GL_DYNAMIC_DRAW); // Allocate memory for SSBO

    // Setup VAO and VBO for rendering particles
    GLuint particleVAO, particleVBO;
    glGenVertexArrays(1, &particleVAO); // Create VAO
    glGenBuffers(1, &particleVBO); // Create VBO
    glBindVertexArray(particleVAO); // Bind VAO
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO); // Bind VBO
    glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), nullptr, GL_STREAM_DRAW); // Allocate memory for VBO

    // Vertex attributes
    glEnableVertexAttribArray(0); // Position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)offsetof(Particle, position)); // Position
    glEnableVertexAttribArray(1); // Color
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)offsetof(Particle, color)); // Color

    glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind VBO
    glBindVertexArray(0); // Unbind VAO



    glEnable(GL_BLEND); // Enable blending
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Set blending function


    // Main loop
    while (!glfwWindowShouldClose(window)) {


        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the screen
        glPointSize(10.0f); // Set point size if using GL_POINTS

        // Calculate delta time 
        auto currentFrameTime = std::chrono::high_resolution_clock::now(); // Get current time
        deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentFrameTime - lastFrameTime).count(); // Calculate delta time
        lastFrameTime = currentFrameTime; // Update last frame time

        // Update particles using compute shader
        glUseProgram(computeShaderProgram); 

        // Bind the SSBO
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);

        // Map the SSBO to a pointer
        Particle* particleData = (Particle*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

        if (particleData) {
            // Read particle data for debugging
            for (int i = 0; i < 10; ++i) {
                std::cout << "Particle " << i << ": Pos(" << particleData[i].position.x << ", "
                    << particleData[i].position.y << "), Vel(" << particleData[i].velocity.x << ", "
                    << particleData[i].velocity.y << "), Age: " << particleData[i].age
                    << ", Lifetime: " << particleData[i].lifeTime << std::endl;
            }

            // Unmap the buffer after reading
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }

        // Unbind the SSBO
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glBindBuffer(GL_COPY_READ_BUFFER, particleSSBO); // Bind the SSBO as the copy read buffer
        glBindBuffer(GL_COPY_WRITE_BUFFER, particleVBO); // Bind the VBO as the copy write buffer
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, particles.size() * sizeof(Particle)); // Copy the SSBO to the VBO

        // Uniform update checks
        GLint mousePosLoc = glGetUniformLocation(computeShaderProgram, "mousePos");
        GLint deltaTimeLoc = glGetUniformLocation(computeShaderProgram, "deltaTime");

        if (mousePosLoc != -1) {
            glUniform2f(mousePosLoc, mousePos.x, mousePos.y);
        }
        else {
            std::cerr << "mousePos uniform location not found." << std::endl;
        }

        if (deltaTimeLoc != -1) {
            glUniform1f(deltaTimeLoc, deltaTime);
        }
        else {
            std::cerr << "deltaTime uniform location not found." << std::endl;
        }
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO); // Bind the SSBO to the compute shader
        glDispatchCompute(NUM_PARTICLES +9/ WORK_GROUP_SIZE, 1, 1); // Dispatch compute shader

    

        // Render particles

        glUseProgram(renderShaderProgram);
        glBindVertexArray(particleVAO);
        glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);

        // Swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

        glGetError(); // Clear error flag

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // Ensure that the compute shader has finished writing to the buffer

        

    // Cleanup
    glDeleteBuffers(1, &particleSSBO);
    glDeleteVertexArrays(1, &particleVAO);
    glDeleteBuffers(1, &particleVBO);
    glDeleteShader(computeShader);
    glDeleteProgram(computeShaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteProgram(renderShaderProgram);
    glfwTerminate();
    return 0;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    mousePos.x = (xpos / width) * 2.0f - 1.0f; // Convert to normalized device coordinates
    mousePos.y = 1.0f - (ypos / height) * 2.0f; // Convert to normalized device coordinates
    std::cout << "Mouse position: " << mousePos.x << ", " << mousePos.y << std::endl;
}

std::string ReadShaderFile(const std::string& shaderPath) {
    std::ifstream shaderFile(shaderPath);
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    shaderFile.close();
    return shaderStream.str();
}

GLuint CompileShader(const std::string& source, GLenum shaderType) {
    GLuint shader = glCreateShader(shaderType);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // Check for shader compile errors
    GLint success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        return 0;
    }

    return shader;
}