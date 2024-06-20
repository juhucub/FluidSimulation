#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <openglDebug.h>
#include <thread>
#include <iostream>
#include "objLoader.h"
#include "demoShader.h"



// Function prototypes
void processInput(GLFWwindow *window);
static void framebuffer_callback(GLFWwindow* window, int width, int height);
bool initGLAD();
void glfwErrorCallback(int error, const char* description);

//Window Settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

static void framebuffer_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window) {
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

bool initGLAD() {
    return gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        
}

void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main()
{
    glfwSetErrorCallback(glfwErrorCallback);

    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit()) {
        return -1;
    }

    // Set the necessary window hints OpenGL VERSION 4.1
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    #ifdef __APPLE__ 
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);    //Apple Users
    #endif

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Fluid Simulation", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    /*Setup a callback function for when the window is resized*/
    glfwSetFramebufferSizeCallback(window, framebuffer_callback);

    /*Enable Vsync to prevent screen tearing <3*/
    glfwSwapInterval(1);    

    /* Initializes GLAD to load OpenGL functions*/
    if(!initGLAD()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    Shader shader("../resources/vertex.vert", "../resources/fragment.frag");
    if (!shader.loadShaderProgramFromFile("../resources/vertex.vert", "../resources/fragment.frag")) {
        std::cerr << "Failed to load shaders" << std::endl;
        glfwTerminate();
        return -1;
    }

    /*Read the .obj file*/
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    bool res = loadOBJ("../models/sphere.obj", vertices, uvs, normals);
    //Model model("../models/sphere.obj");//path to model 
    if(!res) {
        std::cerr << "failed to load .obj file" << std::endl;
        return -1;
    }

    /*Create and bind the vertex Array Object*/
    GLuint VAO, VBO, UVBO, NBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    if (!uvs.empty()) {
        glGenBuffers(1, &UVBO);
        glBindBuffer(GL_ARRAY_BUFFER, UVBO);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glEnableVertexAttribArray(1);
    }

    if (!normals.empty()) {
        glGenBuffers(1, &NBO);
        glBindBuffer(GL_ARRAY_BUFFER, NBO);
        glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glEnableVertexAttribArray(2);
    }

    /*OpenGL Settings*/
    glEnable(GL_DEPTH_TEST);
    /*Set background color of the window. Dark gray with full opacity*/
    glClearColor( 0.1, 0.1, 0.1, 1.0);

    //glClearStencil(0) For later

    #pragma region report opengl errors to std
        glEnable(GL_DEBUG_OUTPUT);
                std::cerr << "1" << std::endl;
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
                std::cerr << "2" << std::endl;
    #pragma endregion

    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");
    ImGui::StyleColorsDark();


     // Create a thread for simulation (Assuming `simulationThread` is defined elsewhere)
    /*std::thread simulationThread([&] {
        while (circle.running) {
            circle.applyForces();
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // 60 FPS
        }
    });*/

    
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
       processInput(window);
       glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      
        shader.use();
        
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        glm::mat4 viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
        glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        shader.setMat4("model", modelMatrix);
        shader.setMat4("view", viewMatrix);
        shader.setMat4("projection", projectionMatrix);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertices.size());

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }
     // Cleanup ImGui
    glDeleteBuffers(1, &VBO);
    if (!uvs.empty()) glDeleteBuffers(1, &UVBO);
    if (!normals.empty()) glDeleteBuffers(1, &NBO);
    glDeleteVertexArrays(1, &VAO);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}