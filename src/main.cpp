#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <openglDebug.h>
#include <demoShader.h>
#include <iostream>
#include "Circle.h"

#if defined(_WIN32) || defined(_WIN64)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#define USE_GPU_ENGINE 0

extern "C"
{
	EXPORT unsigned long NvOptimusEnablement = USE_GPU_ENGINE;
    EXPORT int AmdPowerXpressRequestHighPerformance = USE_GPU_ENGINE;
}

extern void GLAPIENTRY glDebugOutput(GLenum source,
	GLenum type,
	unsigned int id,
	GLenum severity,
	GLsizei length,
	const char *message,
	const void *userParam);

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    // Check if the key pressed is the Escape key and if the action is a key press
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        // Set the window to close
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

bool initGLAD() {
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    return true;
}

void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main(void)
{
    glfwSetErrorCallback(glfwErrorCallback);

    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Set the necessary window hints for macOS
    std::cerr << "Before window hints for macOS" << std::endl;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    std::cerr << "After window hints for macOS" << std::endl;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(1280, 720, "Fluid Simulation", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
     /* Make the window's context current */
    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, key_callback);

    /* Initializes GLAD to load OpenGL functions*/
    if(!initGLAD()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }
    std::cerr << "After Initializing glad to load OpenGL functions" << std::endl;
 //enables VSync to prevent screen tearing
    glfwSwapInterval(1);   

    std::cerr << "After enabling Vsync" << std::endl;
    #pragma region report opengl errors to std
        glEnable(GL_DEBUG_OUTPUT);
                std::cerr << "1" << std::endl;
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
                std::cerr << "2" << std::endl;
    #pragma endregion

     std::cerr << "Before shader" << std::endl;

    //FIXME: Shader Loading PRIMAL 
        Shader s;
        if (!s.loadShaderProgramFromFile(RESOURCES_PATH "vertex.vert", RESOURCES_PATH "fragment.frag")) 
        {
            std::cerr << "Failed to load shader program" << std::endl;
            glfwTerminate();
            return -1;
         }
	    s.bind();

        Shader blurShader;
    if (!blurShader.loadShaderProgramFromFile(RESOURCES_PATH "vertex.vert", RESOURCES_PATH "blur.frag"))
    {
        std::cerr << "Failed to load blur shader program" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Set up framebuffers for post-processing
    unsigned int framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    unsigned int textureColorbuffer;
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1280, 720, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1280, 720);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    Circle circle(100, 0.1f, glm::vec3(0.0f, 0.0f, 1.0f));  

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    std::cerr << "After shader" << std::endl;

    int numParticles = 15;
    float blurAmount = 1.0f;
    float gravity = 0.98f;
    float collisionDampening = 0.9f;
    float particleSize = 0.025f;
    glm::vec3 color(0.0f, 0.0f, 1.0f);

    // Set up VAO and VBO for the fullscreen quad
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    unsigned int quadVAO, quadVBO;

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        int width = 0;
        int height = 0;

        glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

         // Create ImGui window
        ImGui::Begin("Simulation Controls");
        static int numParticles = 100;
        static bool numParticlesChanged = false;
        ImGui::SliderInt("Number of Particles", &numParticles, 10, 100);
        ImGui::SliderFloat("Gravity", &gravity, 0.0f, 10.0f);
        ImGui::SliderFloat("Collision Dampening", &collisionDampening, 0.0f, 1.0f);
        ImGui::SliderFloat("Particle Size", &particleSize, 0.01f, 0.5f);
        ImGui::SliderFloat("Blur Amount", &blurAmount, 0.0f, 10.0f);
        ImGui::ColorEdit3("Color", (float*)&color);
        ImGui::End();

        //Update circle with ImGui-parameters
        circle.setNumParticles(numParticles);
        circle.setGravity(gravity);
        circle.setCollisionDampening(collisionDampening);
        circle.setRadius(particleSize);
        circle.setColor(glm::vec3(color[0], color[1], color[2]));

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		circle.update(0.016f);
        circle.draw(s);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Apply blur shader
        blurShader.bind();
        glUniform1i(blurShader.getUniform("screenTexture"), 0);
        glUniform1f(blurShader.getUniform("blurSize"), blurAmount);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }
     // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    //there is no need to call the clear function for the libraries since the os will do that for us.
	//by calling this functions we are just wasting time.
	
    return 0;
}