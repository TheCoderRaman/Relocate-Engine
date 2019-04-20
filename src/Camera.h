// Camera.h
// Component which the camera will follow

#ifndef CAMERA_H
#define CAMERA_H

#include "Game.h"
#include "Scripting.h"

// Entities with this component will be centered
class Camera {
  public:

    // Make this component scriptable
    static void registerCameraType(sol::environment& env) {

      // Register the usual functions
      Script::registerComponentToEntity<Camera>(env, "Camera");

      // Create the Camera usertype
      env.new_usertype<Camera>("Camera",
        "offset", &Camera::offset
      );
    }

    // Constructors
    Camera(const sf::Vector2f& offsetVec)
      : offset(offsetVec) {}
    Camera(float x = 0.f, float y = 0.f)
      : offset(sf::Vector2f(x, y)) {}

    // Offset for adjustment
    sf::Vector2f offset;

    // Shows the debug information to ImGui
    void showDebugInformation() {}
};

#endif
