// Game.cpp
// This is a static, high level manager for the game's cycle

#include "Game.h"
#include "Scene.h"
#include "Config.h"
#include "Scripting.h"

// Initialise static members
sf::RenderWindow* Game::window_ = nullptr;
sf::View Game::view = sf::View();
bool Game::multiThread_ = false;
std::mutex Game::windowMutex_;
bool Game::debug_ = false;
Game::Status Game::status_ = Game::Status::Uninitialised;
Scene* Game::currentScene_ = nullptr;
sol::state Game::lua;
unsigned Game::fps_ = 0;

// Initialise the game without starting the loop
void
Game::initialise(const sf::VideoMode& mode, const std::string& title, bool multiThread) {

  // Check that we haven't already initialised
  if (status_ != Game::Status::Uninitialised) {
    printf("Error: Cannot initialise application - already running.\n"); 
    return;
  }

  // Print opening message
  printf("Launching Application %d.%d.%d..\n", 
    Build_VERSION_MAJOR, 
    Build_VERSION_MINOR, 
    Build_VERSION_TWEAK);

  // Flag whether we are in multithreaded mode
  multiThread_ = multiThread;
  printf("Running in %s mode.\n", multiThread_ ? "multithreaded" : "standard");

  // Initialise Lua and ensure it works
  Script::startLua();
	auto functional = Game::lua.script_file("Assets/Scripts/GameConfig.lua", &sol::script_pass_on_error);
  if (functional.valid()) { 
    printf("Lua successfully initialised.\n"); 
  }
  else {
    printf("Error: Cannot initialise Lua correctly.\n"); 
    return;
  }

  // Create window and prepare view
  window_ = new sf::RenderWindow(mode, title);
  view = window_->getDefaultView();

  // Flag that the game is ready to start
  status_ = Game::Status::Ready;
}

// Initiate the game loop
void
Game::start() {

  // Check that the game is ready to start
  if (status_ != Game::Status::Ready) {
    printf("Error: Cannot start application.\n"); 
    return;
  }

  // We are now running the game
  status_ = Game::Status::Running;

  // Create a clock for measuring deltaTime
  sf::Clock clock_;

  // FPS variables
  sf::Clock fpsClock_;
  unsigned fpsFrame_ = 0;

  // Disable the window
  if (multiThread_) {
    window_->setActive(false);
  }

  // Pointer to the render thread if necessary
  std::thread* renderThread;

  // Start a thread to render the game
  if (multiThread_) {
    renderThread = new std::thread(Game::handleRenderThread);
    renderThread->detach();
  }

  // Main game loop while window is open
  while (status_ < Game::Status::ShuttingDown) {

    // Get the time since last tick
    sf::Time elapsed_ = clock_.restart();

    // Calculate FPS
    if (fpsClock_.getElapsedTime().asSeconds() >= 1.0f) {
      fps_ = fpsFrame_;
      fpsFrame_ = 0;
      fpsClock_.restart();
    }
    ++fpsFrame_;

    // Prepare to collect input events
    bool process = true;
    std::vector<sf::Event> events;

    // If multithreading, gain access to window quickly
    if (multiThread_) { process = windowMutex_.try_lock(); }
    sf::Event e;

    // If we're allowed to process data
    if (process) {

      // Collect events
      while (window_->pollEvent(e)) {
        events.push_back(e);
      }

      // Disable the lock, we don't need the window
      if (multiThread_) { 
        windowMutex_.unlock(); 
      }

      // Resume processing events
      for (auto ev : events) {

        // Begin closing the game on quit
        // Otherwise, pass the event to the scene
        if (ev.type != sf::Event::Closed) {
          handleEvent(ev);
        }
        else {
          quit();
        }
      }
    }

    // Update the game
    update(elapsed_);

    // Render every frame after updating
    if (!multiThread_) {
      if (status_ < Game::Status::ShuttingDown) {
        render();
      }
    }
    else {
      std::this_thread::yield();
    }
  }

  // Wait for render thread to finish
  if (multiThread_ && renderThread->joinable()) {
    renderThread->join();
    delete renderThread;
  }

  // Quit the game and exit program
  shutdown();
  printf("Exiting..\n");
}

// Called every frame, returns true when game should end
void
Game::update(const sf::Time& dt) {

  // Update the screen if the pointer is set
  if (currentScene_ != nullptr) {
    currentScene_->update(dt);
  }
}

// Call the render() function from a seperate thread
void
Game::handleRenderThread() {

  // Easy out
  if (Game::getStatus() < Game::Status::Running) return;

  // Continually render the game while
  while (Game::getStatus() < Game::Status::ShuttingDown) {
    windowMutex_.lock();
    Game::render();
    windowMutex_.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
  }
}

// Render the game every frame, after update
void
Game::render() {

  // Clear the window for rendering
  window_->clear();

  // Render the game if pointer is set
  if (currentScene_ != nullptr) {
    window_->setView(view);
    currentScene_->render(*window_);
  }

  // Render everything in the screen
  window_->display();
}

// Respond to any key or mouse related events
void
Game::handleEvent(const sf::Event& event) {

  // Adjust the viewport if window is resized
  if (event.type == sf::Event::Resized) {
    sf::FloatRect visibleArea(0.f, 0.f, event.size.width, event.size.height);
    Game::view = sf::View(visibleArea);
  }

  // Feed the screen the input
  if (currentScene_ != nullptr) {
    currentScene_->handleEvent(event);
  }
}

// Release resources before the game closes
void
Game::quit() {
  printf("Quitting game..\n");

  // Signal that we're quitting the game
  status_ = Game::Status::Quitting;

  // Allow the current screen to halt termination
  if (currentScene_) {
    currentScene_->quit();
  }
  else {
    terminate();
  }
}

// Called from within application to close it down
void
Game::terminate() {

  // Set the game's status to break game loop
  status_ = Game::Status::ShuttingDown;

  // Close the window, exiting the game loop
  window_->close();
  delete window_;
}

// Free resources before program closes
void
Game::shutdown() {
  status_ = Game::Status::Uninitialised;
  printf("Releasing resources..\n");
  delete currentScene_;
}

// Change to the new screen
void 
Game::switchScene(Scene* scene) {

  // Switch away from old screen
  if (currentScene_ != nullptr) {
    currentScene_->hideScene();
  }

  // Change the screen to be rendered
  currentScene_ = scene;

  // Run any logic for showing screen
  if (currentScene_ != nullptr) {
    currentScene_->registerFunctions();
    currentScene_->showScene();
  }
}

// Open the dev console in the terminal
void
Game::openDevConsole() {
  printf("---------------~DEV CONSOLE~---------------\n");
  printf("Please enter a command: ");
  std::string str;
  std::cin >> str;
  
  // If we have a command to process
  if (str != "") {
    sol::protected_function_result result = Game::lua.script(str, sol::script_pass_on_error);
    if (!result.valid()) {
      sol::error err = result;
      std::cout << "Invalid command '" << str << "'.\nError: " << err.what() << std::endl;
    }
  }
  printf("-------------~END OF CONSOLE~-------------\n");
}

// Get a pointer to the game's window
const sf::RenderWindow*
Game::getWindow() {
  return window_;
}

// Get FPS of application
unsigned
Game::getFPS() {
  return fps_;
}

// Get status of application
Game::Status
Game::getStatus() {
  return status_;
}

// Set debug mode
void
Game::setDebugMode(bool enable) {
  debug_ = enable;
  printf("Debug mode %s.\n", enable ? "enabled" : "disabled");
}

// Get debug mode
bool
Game::getDebugMode() {
  return debug_;
}
