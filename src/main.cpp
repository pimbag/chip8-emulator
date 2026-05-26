#include <iostream>
#include <chrono>
#include <SDL3/SDL.h>

#include "chip8.h"

constexpr std::size_t width  = 1024;
constexpr std::size_t height = 512;

constexpr double cpu_hz   = 500.0;
constexpr double timer_hz = 60.0;

constexpr auto cpu_interval =
    std::chrono::duration<double>(1.0 / cpu_hz);

constexpr auto timer_interval =
    std::chrono::duration<double>(1.0 / timer_hz);

constexpr auto render_interval = timer_interval;

constexpr SDL_Scancode KeyMap[16] = {
    SDL_SCANCODE_X, // 0
    SDL_SCANCODE_1, // 1
    SDL_SCANCODE_2, // 2
    SDL_SCANCODE_3, // 3
    SDL_SCANCODE_Q, // 4
    SDL_SCANCODE_W, // 5
    SDL_SCANCODE_E, // 6
    SDL_SCANCODE_A, // 7
    SDL_SCANCODE_S, // 8
    SDL_SCANCODE_D, // 9
    SDL_SCANCODE_Z, // A
    SDL_SCANCODE_C, // B
    SDL_SCANCODE_4, // C
    SDL_SCANCODE_R, // D
    SDL_SCANCODE_F, // E
    SDL_SCANCODE_V  // F
};

int main(int argc, char* argv[]){
	if (argc != 2) {
		std::cerr << "Expected 2 arguments. ROM file must be provided.\n";
		return 1;
	}
	
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
		SDL_Log("SDL Initialization Error: %s", SDL_GetError());
		return 1;
	}

	std::clog << "Initialized SDL\n";

	SDL_Window* win = SDL_CreateWindow(
		argv[1],
		width,
		height,
		SDL_WINDOW_RESIZABLE
	);
	if (win == nullptr) {
		SDL_Log("SDL Failed to create window: %s", SDL_GetError());
		SDL_Quit();
		return 2;
	}

	std::clog << "Created SDL Window\n";

	SDL_Renderer* renderer = SDL_CreateRenderer(win, nullptr);
	SDL_SetRenderVSync(renderer, 1);
	SDL_SetRenderLogicalPresentation(renderer, 64, 32, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
	
	SDL_Texture* sdl_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 64, 32);
	SDL_SetTextureScaleMode(
    		sdl_texture,
    		SDL_SCALEMODE_NEAREST
	);
	uint32_t pixels[2048];

	std::clog << "Finished Creating SDL Texture & SDL Renderer\n";
	
	reload:

	Chip8 chip8 = Chip8();
	
	if (!chip8.load(argv[1])) {
		std::cerr << "Failed to load ROM.\n";
		return 3;
	}
	std::clog << "Loaded ROM: " << argv[1] << "\n";

	auto last_cpu    = std::chrono::steady_clock::now();
	auto last_timer  = std::chrono::steady_clock::now();
	auto last_render = std::chrono::steady_clock::now();

	while (true) {	
		SDL_Event e;
		while(SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT) return 0;
			if (e.type == SDL_EVENT_KEY_DOWN) {
				for (std::size_t i = 0; i < 16; ++i) {
					if (e.key.scancode == KeyMap[i]) {
						chip8.keys[i] = true;
					}
				}
			}
			if (e.type == SDL_EVENT_KEY_UP) {
				if (e.key.key == SDLK_F1) goto reload;

				if (e.key.key == SDLK_F2)
				{
    					chip8.blocked = !chip8.blocked;

    					if (!chip8.blocked)
    					{
        					last_cpu    = std::chrono::steady_clock::now();
        					last_timer  = last_cpu;
						last_render = last_cpu;
    					}
				}

				for (std::size_t i = 0; i < 16; ++i) {
					if (e.key.scancode == KeyMap[i]) {
						chip8.keys[i] = false;
					}
				}
			}
		}
		
		auto now = std::chrono::steady_clock::now();
		while (now - last_cpu >= cpu_interval && !chip8.blocked) {
			chip8.emu_cycle();
			last_cpu += std::chrono::duration_cast<std::chrono::steady_clock::duration>(cpu_interval);
		}

		while (now - last_timer >= timer_interval && !chip8.blocked) {
			if (chip8.delay_timer > 0) {
				--chip8.delay_timer;
			}

			if (chip8.sound_timer > 0) {
				--chip8.sound_timer;
			}

			last_timer += std::chrono::duration_cast<std::chrono::steady_clock::duration>(timer_interval);
		}

		if (now - last_render >= render_interval) {
			last_render += std::chrono::duration_cast<std::chrono::steady_clock::duration>(render_interval);
			if (!chip8.draw) continue;
			chip8.draw = false;

			for (std::size_t i = 0; i < 2048; ++i) {
				bool px = chip8.graphics[i];
				pixels[i] = px
					? 0xFFFFFFFF   // WHITE
					: 0xFF000000;  // BLACK
			}

			SDL_UpdateTexture(
				sdl_texture,
				nullptr,
				pixels,
				64 * sizeof(uint32_t)
			);
			SDL_RenderClear(renderer);

			SDL_RenderTexture(
				renderer,
				sdl_texture,
				nullptr,
				nullptr
			);

			SDL_RenderPresent(renderer);	
		}

		SDL_Delay(1);
	}

	SDL_DestroyTexture(sdl_texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(win);
	SDL_Quit();
}
