#ifndef CHIP8_H
#define CHIP8_H

#include <cstdint>
#include <cstdlib>
#include <array>
#include <filesystem>

constexpr std::size_t DisplayWidth  = 64;
constexpr std::size_t DisplayHeight = 32;
constexpr std::size_t DisplaySize   = DisplayWidth * DisplayHeight;

constexpr uint8_t FontSet[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};

constexpr std::size_t index_pixel(uint8_t x, uint8_t y)
{
	return y * DisplayWidth + x;
}

class Chip8
{
	private:
		std::array<uint8_t, 4096> ram{};        	// 4kb of memory 
		std::array<uint8_t, 16>   v_registers{};	// 16 8 bit general-purpose variable registers
		std::array<uint16_t, 16>  stack{};

		uint16_t pc{0x200};	// Program counter
		uint16_t index{0};	// Index register
		uint16_t opcode;	// Current opcode
		uint16_t sp{0};		// Stack pointer
	public:
		std::array<bool, DisplaySize> graphics; // 64x32 display monochrome (black or white)
		std::array<bool, 16> keys;		// Pressed/Not pressed
	
		uint8_t sound_timer{0};
		uint8_t	delay_timer{0};

		bool draw{true};
		bool blocked{false};

		void emu_cycle();
		bool load(const std::filesystem::path& path);

};

#endif
