#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <cstdio>
#include "camera.h"

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_Window* window = SDL_CreateWindow(
        "cs15engine",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(1); // vsync
    SDL_SetRelativeMouseMode(SDL_TRUE);

    Camera camera;
    Uint64 lastTicks = SDL_GetPerformanceCounter();
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            } else if (event.type == SDL_MOUSEMOTION) {
                camera.look((float)event.motion.xrel, (float)event.motion.yrel);
            }
        }

        Uint64 nowTicks = SDL_GetPerformanceCounter();
        float dt = (float)(nowTicks - lastTicks) / (float)SDL_GetPerformanceFrequency();
        lastTicks = nowTicks;

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        float forward = 0.0f, strafe = 0.0f;
        if (keys[SDL_SCANCODE_W]) forward += 1.0f;
        if (keys[SDL_SCANCODE_S]) forward -= 1.0f;
        if (keys[SDL_SCANCODE_D]) strafe += 1.0f;
        if (keys[SDL_SCANCODE_A]) strafe -= 1.0f;
        camera.move(forward, strafe, dt);

        glViewport(0, 0, 1280, 720);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
