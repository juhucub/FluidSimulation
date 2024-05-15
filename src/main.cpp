#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <openglDebug.h>
#include <demoShader.h>
#include <iostream>

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

   // glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
    std::cerr << "After window hints for macOS" << std::endl;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Fluid Simulation", NULL, NULL);
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

    std::cerr << "After shader" << std::endl;
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        int width = 0;
        int height = 0;

        glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        //I'm using the old pipeline here just to test, you shouldn't learn this,
		//Also It might not work on apple
		glBegin(GL_TRIANGLES);
		glColor3f(1, 0, 0);
		glVertex2f(0,1);
		glColor3f(0, 1, 0);
		glVertex2f(1,-1);
		glColor3f(0, 0, 1);
		glVertex2f(-1,-1);
		glEnd();

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }
    //there is no need to call the clear function for the libraries since the os will do that for us.
	//by calling this functions we are just wasting time.
	
    return 0;
}