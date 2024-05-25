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

    Circle circle(100, 0.1f, glm::vec3(0.0f, 0.0f, 1.0f));  

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO const& io = ImGui::GetIO(); (void)io;
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    std::cerr << "After shader" << std::endl;

    int numParticles = 15;
    float alpha = 1.0f;
    float gravity = 0.98f;
    float collisionDampening = 0.9f;
    float particleSize = 0.025f;
    glm::vec3 color(0.0f, 0.0f, 1.0f);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        int width = 0;
        int height = 0;

        glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
        

       glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
       s.bind();
       circle.update(0.016f);
       circle.draw(s);
       
        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Create ImGui window
       ImGui::Begin("Simulation Controls");
       static int numParticles = 1000;
       static bool numParticlesChanged = false;
       if (ImGui::SliderInt("Number of Particles", &numParticles, 10, 5000)) {
           numParticlesChanged = true;
       }
       ImGui::SliderFloat("Gravity", &gravity, 0.0f, 10.0f);
       ImGui::SliderFloat("Collision Dampening", &collisionDampening, 0.0f, 1.0f);
       ImGui::SliderFloat("Particle Size", &particleSize, 0.01f, 0.5f);
       ImGui::SliderFloat("Blur Amount", &alpha, 0.1f, 1.0f);
       ImGui::ColorEdit3("Color", (float*)&color);
       ImGui::End();

       if (numParticlesChanged) {
           circle.setNumParticles(numParticles);
           numParticlesChanged = false;
       }

        //Update circle with ImGui-parameters
        circle.setGravity(gravity);
        circle.setCollisionDampening(collisionDampening);
        circle.setRadius(particleSize);
        circle.setColor(glm::vec3(color[0], color[1], color[2]));
        circle.setAlpha(alpha);

        //Enable Blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
	//glDeleteFramebuffers(1, &framebuffer);
    //glDeleteTextures(1, &textureColorbuffer);
    glfwTerminate();
    return 0;
}