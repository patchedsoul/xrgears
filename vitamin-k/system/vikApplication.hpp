#pragma once

#include "vikSettings.hpp"

namespace vik {
class Application {
public:
    Settings settings;
    Window *window;
    bool quit = false;

    Application() {}
    ~Application()  {}
};
}
