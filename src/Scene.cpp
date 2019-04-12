// Scene.cpp
// Logic more oriented to the game goes here

#include "Scene.h"

// Avoid cyclic dependencies
#include "ControlSystem.h"

// Register scene functionality to Lua
void
Scene::registerSceneType() {
  Game::lua.new_usertype<Scene>("Scene",
    sol::constructors<Scene(
      sol::protected_function,
      sol::protected_function,
      sol::protected_function,
      sol::protected_function,
      sol::protected_function,
      sol::protected_function)>()
    );
}

// Constructor
Scene::Scene
  ( sol::protected_function begin
  , sol::protected_function show
  , sol::protected_function hide
  , sol::protected_function update
  , sol::protected_function windowEvent
  , sol::protected_function quit)

  : hasBegun_(false)
  , world_(ECS::World::createWorld())
  , onBegin_(begin)
  , onShow_(show)
  , onHide_(hide)
  , onUpdate_(update)
  , onWindowEvent_(windowEvent)
  , onQuit_(quit) {
}

// Copy constructor
Scene::Scene(const Scene& other) 
  : hasBegun_(other.hasBegun_)
  , world_(ECS::World::createWorld())
  , onBegin_(other.onBegin_)
  , onShow_(other.onShow_)
  , onHide_(other.onHide_)
  , onUpdate_(other.onUpdate_)
  , onWindowEvent_(other.onWindowEvent_)
  , onQuit_(other.onQuit_) {
}

// Destructor
Scene::~Scene() {
  world_->destroyWorld();
}

// Called when the scene is started
void
Scene::begin() {

  // Try to call the begin function from this scene's lua
  if (onBegin_.valid()) {
    auto attempt = onBegin_();
    if (!attempt.valid()) {
      sol::error err = attempt;
      Console::log("[Error] in Scene.begin():\n> %s", err.what());
    }
  }

  // Flag that the scene has started
  hasBegun_ = true;
}

// Run this scene's script to register it's functions
void
Scene::registerFunctions() {

  // Set up environment
  lua_ = sol::environment(Game::lua, sol::create, Game::lua.globals());
  Script::registerSceneFunctions(lua_, world_);

  // Expose the world in the scene
  Game::lua["World"] = lua_;

  // Add to autocomplete
  Console::addCommand("[Class] World");
  Console::addCommand("World.createEntity");
}

// When the screen is shown
void
Scene::showScene() {

  // Begin the scene if it hasn't yet
  if (!hasBegun_) {
    begin();
  }

  // Try to call the begin function from this scene's lua
  if (onShow_.valid()) {
    auto attempt = onShow_();
    if (!attempt.valid()) {
      sol::error err = attempt;
      Console::log("[Error] in Scene.showScene():\n> %s", err.what());
    }
  }
}

// When the screen is hidden
void
Scene::hideScene() {
  if (onHide_.valid()) {
    auto attempt = onHide_();
    if (!attempt.valid()) {
      sol::error err = attempt;
      Console::log("[Error] in Scene.hideScene():\n> %s", err.what());
    }
  }
}

// Update the game every frame
void
Scene::update(const sf::Time& dt) {

  // Call scene's update script
  if (onUpdate_.valid()) {
    auto attempt = onUpdate_(dt);
    if (!attempt.valid()) {
      sol::error err = attempt;
      Console::log("[Error] in Scene.update():\n> %s", err.what());
    }
  }

  // Update the ECS
  world_->update(dt);
}

// Render the game every frame
void
Scene::render(sf::RenderWindow& window) {

  // Render a simple circle for testing
  float rad = 100.0f;
  sf::CircleShape shape(rad);
  const sf::Vector2u size = Game::getWindow()->getSize();
  auto pos = sf::Vector2f((size.x * 0.5f) - rad, (size.y * 0.5f) - rad);
  shape.setPosition(pos);

  shape.setFillColor(sf::Color::Green);
  window.draw(shape);

  // Do any debug-only rendering
  if (Game::getDebugMode()) {
    world_->emit<DebugRenderPhysicsEvent>({window});
  }
}

// Handle keypresses
void
Scene::handleEvent(const sf::Event& event) {

  // Send event to control system
  ControlSystem::handleInput(event);

  // Call scene's input script
  if (onWindowEvent_.valid()) {
    auto attempt = onWindowEvent_(event);
    if (!attempt.valid()) {
      sol::error err = attempt;
      Console::log("[Error] in Scene.handleEvent():\n> %s", err.what());
    }
  }
}

// When the game is quit
void
Scene::quit() {
  bool quitAnyway = !onQuit_.valid();
  if (!quitAnyway) {
    auto attempt = onQuit_();
    if (!attempt.valid()) {
      quitAnyway = true;
      sol::error err = attempt;
      Console::log("[Error] in Scene.quit():\n> %s", err.what());
    }
  }
  if (quitAnyway) {
    quitAnyway = false;
    Console::log("Terminating program.");
    Game::terminate();
  }
}

// Add entries to debug menu
void
Scene::addDebugMenuEntries() {
  world_->emit<addDebugMenuEntryEvent>({});
}

// Add information to the main IMGUI debug window
void
Scene::addDebugInfoToDefault() {
  world_->emit<addDebugInfoEvent>({});
}
