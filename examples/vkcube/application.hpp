#pragma once

#include "cube.hpp"
#include "VikRenderer.hpp"

class CubeApplication {
public:
   Cube model;
   VikRenderer *renderer;

   CubeApplication(uint32_t w, uint32_t h) {
        renderer = new VikRenderer(w, h);
   }

   ~CubeApplication() {
        delete renderer;
   }
};


