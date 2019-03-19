// PhysicsSystem.cpp
// System to simulate physics in game

#include "PhysicsSystem.h"

// Define statics
const float PhysicsSystem::scale = 100.f;

// Register a physics system in this world
void
PhysicsSystem::registerPhysicsSystemFunctions(ECS::World* world) {

  // Create and install physics system
  Game::lua.set_function("usePhysicsSystem", [world]() { 

    // Debug message
    if (Game::getDebugMode()) {
      printf("Initialising Physics system..\n");
    }

    // Create the physics system to return to the world
    auto* newPS = new PhysicsSystem();
    world->registerSystem(newPS);

    // Get the new physics world
    b2World* physicsWorld = newPS->getWorld();

    // Register functions to address this system
    Game::lua.set_function("getGravity", &PhysicsSystem::getGravity, newPS);
    Game::lua.set_function("setGravity", &PhysicsSystem::setGravity, newPS);
    Game::lua.set_function("setGravityMult", &PhysicsSystem::setGravityMult, newPS);
    Game::lua.set_function("physicsBodyCount", &b2World::GetBodyCount, physicsWorld);

    // Allow the use of RigidBodies
    RigidBody::registerFunctions(physicsWorld);

    // Return the newly made physics system
    return newPS;
  });
}

// Constructor, enable debugging of physics
PhysicsSystem::PhysicsSystem() 
  : defaultGravity_(sf::Vector2f(0.f, 10.f))
  , world_(b2Vec2(defaultGravity_.x, defaultGravity_.y))
  , timeStepAccumilator_(0.0f) {

  // Set up the drawing of physics
	physicsDebugDraw_.SetFlags(PhysicsDebugDraw::e_shapeBit);
	world_.SetDebugDraw(&physicsDebugDraw_);
}

// Clean up the world
PhysicsSystem::~PhysicsSystem() {
  //@TODO: Figure out if we need to do something here?
}

// Handle the physics independant to game FPS
void 
PhysicsSystem::update(ECS::World* world, const sf::Time& dt) {

  // Accumilate time and calculate steps
  timeStepAccumilator_ += dt.asSeconds();
  const auto steps = static_cast<int>(
    std::floor(timeStepAccumilator_ / fixedTimeStep_));

  // Calculate if we should simulate
  if (steps > 0) {
    timeStepAccumilator_ -= steps * fixedTimeStep_;
  }
  fixedTimeStepRatio_ = timeStepAccumilator_ / fixedTimeStep_;
  const int stepsClamped = std::min(steps, maxSteps_);

  // Check if any b2Body's are out of sync
  world->each<Transform, RigidBody>([&](ECS::Entity* e, ECS::ComponentHandle<Transform> t, ECS::ComponentHandle<RigidBody> r) {
    b2Body* const body = r->body_;
    if (r->isOutOfSync_ && body != nullptr) {
      const b2Vec2 newPos = convertToB2(t->position);
      const float newRot = t->rotation;
      //printf("Setting new location to (%f, %f)\n", newPos.x, newPos.y);
      //printf("Setting new rotation to %f\n", t->rotation);
      body->SetTransform(newPos, newRot);
      body->SetAwake(true);
      r->previousPosition_ = newPos;
      r->previousAngle_ = newRot;
      r->isOutOfSync_ = false;
    }
  });

  // Simulate only when we should
  for (int i = 0; i < stepsClamped; ++i) {

    // Reset smoothing states of all entities
    world->each<Transform, RigidBody>([&](ECS::Entity* e, ECS::ComponentHandle<Transform> t, ECS::ComponentHandle<RigidBody> r) {
      resetSmoothStates(t, r);
    });

    // Simulate
    singleStep(fixedTimeStep_);
  }

  // Reset applied forces
  world_.ClearForces();

  // Tween in between physics steps and destroy any old bodies
  world->each<Transform, RigidBody>([&](ECS::Entity* e, ECS::ComponentHandle<Transform> t, ECS::ComponentHandle<RigidBody> r) {
    smoothState(t, r);

    // Dispose of any old bodies and clear the disposal list
    for (auto* d : r->disposeList_) {
      world_.DestroyBody(d);
    }
    r->disposeList_.clear();
  });
}

// Single-step the physics
void
PhysicsSystem::singleStep(float timeStep) {
  world_.Step(timeStep, velocityIterations_, positionIterations_);
}

// Interpolation between frames
void
PhysicsSystem::smoothState(ECS::ComponentHandle<Transform> t, ECS::ComponentHandle<RigidBody> rb) {

  // Ensure there's a body to work with
  b2Body* const body = rb->body_;
  if (body != nullptr) {

    // Set up variables
    const float oneMinusRatio = 1.0f - fixedTimeStepRatio_;

    // Interpolate based on previous position
    if (body->GetType() != b2_staticBody) {

      // Tween position
      t->position = convertToSF(
        fixedTimeStepRatio_ * body->GetPosition() +
        oneMinusRatio * rb->previousPosition_);

      // Tween rotation
      t->rotation = body->GetAngle();
      // @TODO: This code sets t->rotation to infinity and breaks the game
      // it must be fixed in order to tween rotation
      //  t->rotation = body->GetAngle() +
      //  oneMinusRatio * rb->previousAngle_;
    }
  }
}

// When the step occurs, reset any smoothing
void
PhysicsSystem::resetSmoothStates(ECS::ComponentHandle<Transform> t, ECS::ComponentHandle<RigidBody> r) {

  // Set up variables
  b2Body* const body = r->body_;

  // Reset any smoothing
  if (body != nullptr && body->GetType() != b2_staticBody) {
    r->previousPosition_ = body->GetPosition();
    r->previousAngle_ = body->GetAngle();
  }
}

// Get this system's physics world
b2World*
PhysicsSystem::getWorld() {
  return &world_;
}

// Get gravity
sf::Vector2f
PhysicsSystem::getGravity() const {
  return convertToSF(world_.GetGravity());
}

// Set gravity multiplier
void
PhysicsSystem::setGravityMult(float m) {
  const auto g = getGravity();
  setGravity(g.x * m, g.y * m);
}

// Set gravity
void
PhysicsSystem::setGravity(float gx, float gy) {
  world_.SetGravity(convertToB2(sf::Vector2f(gx, gy)));
}

// Convert to SFML vectors
sf::Vector2f 
PhysicsSystem::convertToSF(const b2Vec2& vec) {
  return sf::Vector2f(vec.x * PhysicsSystem::scale, vec.y * PhysicsSystem::scale);
}

// Convert to Box2D vectors
b2Vec2 
PhysicsSystem::convertToB2(const sf::Vector2f& vec) {
  return b2Vec2(vec.x / PhysicsSystem::scale, vec.y / PhysicsSystem::scale);
}

// Render the physics in debug mode
void
PhysicsSystem::receive(ECS::World* w, const DebugRenderPhysicsEvent& e) {

  // Save the pointer to the window
  physicsDebugDraw_.window = &e.window;

  // Render the physics system
  world_.DrawDebugData();
}
