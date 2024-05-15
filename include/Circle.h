
#include <glm/glm.hpp>

class Circle {
public:
    Circle(float x, float y, float radius, glm::vec3 color);

    void update(float deltaTime);
    void draw();

    glm::vec2 position;
    float radius;
    glm::vec3 color;
    glm::vec2 velocity;

private:
    void applyGravity(float deltaTime);
    void keepInBounds();
};


