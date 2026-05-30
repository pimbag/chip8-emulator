#include <iostream>
#include <chrono>
#include <span>
#include <format>
#include <memory>
#include <SDL3/SDL.h>

#include "chip8.h"

constexpr std::size_t width  = 1024;
constexpr std::size_t height = 512;

constexpr double cpu_hz   = 500.0;
constexpr double timer_hz = 60.0;

constexpr auto cpu_interval    = std::chrono::duration<double>(1.0 / cpu_hz);
constexpr auto timer_interval  = std::chrono::duration<double>(1.0 / timer_hz);
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

using WindowPtr   = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
using RendererPtr = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;
using TexturePtr  = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
class Window 
{
	private:
		WindowPtr   win      {nullptr, SDL_DestroyWindow};
		RendererPtr renderer {nullptr, SDL_DestroyRenderer};
		TexturePtr  texture  {nullptr, SDL_DestroyTexture};

		std::array<uint32_t, 2048> pixels{};
	public:
		Window(const char* window_name)
		{
			win.reset(SDL_CreateWindow(window_name, width, height, SDL_WINDOW_RESIZABLE));
			if (!win) throw std::runtime_error( std::format("Failed to create SDL Window: {}", SDL_GetError()) );
			
			renderer.reset(SDL_CreateRenderer(win.get(), nullptr));
			if (!renderer) throw std::runtime_error( std::format("Failed to create SDL renderer: {}", SDL_GetError()) );

			SDL_SetRenderVSync(renderer.get(), 1);
			SDL_SetRenderLogicalPresentation(renderer.get(), 64, 32, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
			
			texture.reset(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 64, 32));
			if (!texture) throw std::runtime_error( std::format("Failed to create SDL texture: {}", SDL_GetError()) );

			SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_NEAREST);
		}

		Window(const Window&) noexcept            = delete;
		Window& operator=(const Window&) noexcept = delete;

		void render(const Chip8 &chip8)
		{
			for (std::size_t i = 0; i < pixels.size(); ++i) {
				bool px = chip8.graphics[i];
				pixels[i] = px 
					? 0xFFFFFFFF  // WHITE
					: 0xFF000000; // BLACK
			}

			SDL_UpdateTexture(texture.get(), nullptr, pixels.data(), 64 * sizeof(uint32_t));
			
			SDL_RenderClear(renderer.get());
			SDL_RenderTexture(renderer.get(), texture.get(), nullptr, nullptr);

			SDL_RenderPresent(renderer.get());
		}
};

void gen_audio(std::span<float> buffer, bool beep, float& phase)
{
	constexpr float frequency  = 500.0f;
	constexpr float sampleRate = 48000.0f;
	constexpr float amplitude  = 0.2f;

	constexpr float increment  = frequency / sampleRate;

	for (float& sample : buffer) {
		sample = beep ? (phase < 0.5f ? amplitude : -amplitude) : 0.0f;
		phase += increment;

		if (phase >= 1.0f) phase -= 1.0f;
	}
		
	if (phase >= 1.0f) phase = 0.0f;
}

using AudioPtr = std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)>;
class Audio
{
	private:
		AudioPtr stream{nullptr, SDL_DestroyAudioStream};
		bool audio_enabled{true};

		float phase{0.0f};
		std::array<float, 1024> audioBuffer{};
	public:
		static constexpr std::size_t sample_rate       = 48000;
		static constexpr std::size_t audio_chunk_bytes = sizeof(float) * 1024;
		
		Audio() 
		{
			SDL_AudioSpec spec{};
			spec.freq     = sample_rate;
			spec.format   = SDL_AUDIO_F32;
			spec.channels = 1;

			stream.reset(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr));
			if (!stream) throw std::runtime_error( std::format("Failed to create SDL Audio Stream: {}", SDL_GetError()) );

			SDL_ResumeAudioStreamDevice(stream.get());
		}

		Audio(const Audio&) noexcept            = delete;
		Audio& operator=(const Audio&) noexcept = delete;

		void check_audio(const Chip8 &chip8)
		{
			if (SDL_GetAudioStreamQueued(stream.get()) < audio_chunk_bytes) {	
				gen_audio(audioBuffer, chip8.sound_timer > 0 && !chip8.blocked && audio_enabled, phase);
				SDL_PutAudioStreamData(stream.get(), audioBuffer.data(), audio_chunk_bytes);
			}
		}

		void toggle_audio() {audio_enabled = not audio_enabled;}
};

struct SDLContext
{
	SDLContext() 
	{
		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
			throw std::runtime_error(
				std::format("SDL Failed to initialize: {}", SDL_GetError())
			);
		}
	}

	~SDLContext() noexcept 
	{
		SDL_Quit();
	}

	SDLContext(const SDLContext&) noexcept            = delete;
	SDLContext& operator=(const SDLContext&) noexcept = delete;
};

int main(int argc, char* argv[]){
	if (argc != 2) {
		std::cerr << "Too many arguments. Usage: ./app <rom-file>\n";
		return 1;
	}

	SDLContext context;

	std::clog << "Initialized SDL\n";
	
	Window win(argv[1]);
	Audio audio{};

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

	bool running = true;

	while (running) {	
		SDL_Event e;
		while(SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT) running = false;
			if (e.type == SDL_EVENT_KEY_DOWN) {
				for (std::size_t i = 0; i < 16; ++i) {
					if (e.key.scancode == KeyMap[i]) {
						chip8.keys[i] = true;
					}
				}
			}
			if (e.type == SDL_EVENT_KEY_UP) {
				if (e.key.key == SDLK_F1) goto reload;
				if (e.key.key == SDLK_F2) {
    					chip8.blocked = !chip8.blocked;

    					if (!chip8.blocked)
    					{
        					last_cpu    = std::chrono::steady_clock::now();
        					last_timer  = last_cpu;
						last_render = last_cpu;
    					}
				}
				if (e.key.key == SDLK_F3) audio.toggle_audio();

				if (e.key.key == SDLK_ESCAPE) running = false;

				for (std::size_t i = 0; i < 16; ++i) {
					if (e.key.scancode == KeyMap[i]) {
						chip8.keys[i] = false;
					}
				}
			}
		}

		if (!running) break;
		
		auto now = std::chrono::steady_clock::now();
		while (now - last_cpu >= cpu_interval && !chip8.blocked) {
			chip8.emu_cycle();
			last_cpu += std::chrono::duration_cast<std::chrono::steady_clock::duration>(cpu_interval);
		}

		audio.check_audio(chip8);

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

			win.render(chip8);
		}

		SDL_Delay(1);
	}
}
