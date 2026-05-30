#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "chip8.h"

bool Chip8::load(const std::filesystem::path& path)
{
	constexpr std::size_t program_start = 0x200; // 512
					 
	std::ifstream rom(path, std::ios::binary);
	if (!rom) {
		std::cerr << "Failed to open ROM\n";
		return false;
	}

	const auto rom_size = std::filesystem::file_size(path);
	if (rom_size > (ram.size() - program_start)) {
		std::cerr << "ROM is too large compared to available memory (" << rom_size << "b/3584b)\n";
		return false;
	}
	
	rom.read(
		reinterpret_cast<char*>(ram.data() + program_start),
		static_cast<std::streamsize>(rom_size)
	);

	for (std::size_t i = 0; i < 80; ++i) {
		ram[i] = FontSet[i]; 
	}
	
	return true;
}

void Chip8::emu_cycle()
{
    opcode = (ram[pc] << 8) | ram[pc + 1];

    uint8_t x    = (opcode & 0x0F00) >> 8;
    uint8_t y    = (opcode & 0x00F0) >> 4;
    uint8_t n    =  opcode & 0x000F;
    uint8_t kk   =  opcode & 0x00FF;

    uint16_t nnn = opcode & 0x0FFF;

    switch (opcode & 0xF000)
    {
        case 0x0000:
            switch (opcode)
            {
                case 0x00E0: // 00E0 -- Clears display
                    graphics.fill(0);
                    draw = true;
                    break;

                case 0x00EE: // 00EE -- Returns from a subroutine via stack pointer
                    pc = stack[--sp];
                    break;
		default:
		    std::clog << "Unknown opcode: " << std::hex << opcode << "\n";
            }
            break;

        case 0x1000: // 1NNN -- Jump to memory location NNN
            pc = nnn - 2;
            break;

        case 0x2000: // 2NNN -- Calls subroutine at memory location NNN, pushes current memory location
            stack[sp++] = pc;
            pc = nnn - 2;
            break;

        case 0x3000: // 3XNN -- Skips one instruction if the value in VX is equal to NN
            if (v_registers[x] == kk) pc += 2;
            break;

        case 0x4000: // 4XNN -- Skips one instruction if the value in VX is NOT equal to NN
            if (v_registers[x] != kk) pc += 2;
            break;

        case 0x5000: // 5XY0 -- Skips next instruction if the value in VX and VY are equal
            if (v_registers[x] == v_registers[y]) pc += 2;
            break;

        case 0x6000: // 6XNN -- Sets register VX to value NN
            v_registers[x] = kk;
            break;

        case 0x7000: // 7XNN -- Adds value NN to VX
            v_registers[x] += kk;
            break;

        case 0x8000:
            switch (n)
            {
                case 0x0: // 8XY0 -- VX is set to the value of VY
                    v_registers[x] = v_registers[y];
                    break;

                case 0x1: // 8XY1 -- Bitwise OR of VX and VY
                    v_registers[x] |= v_registers[y];
                    break;

                case 0x2: // 8XY2 -- Bitwise AND of VX and VY
                    v_registers[x] &= v_registers[y];
                    break;

                case 0x3: // 8XY3 -- Bitwise XOR of VX and VY
                    v_registers[x] ^= v_registers[y];
                    break;

                case 0x4: // 8XY4 -- VX is set to the value of the sum of VX and VY [Potential Flag]
                {
                    uint16_t sum = v_registers[x] + v_registers[y];
                    v_registers[0xF] = sum > 0xFF;
                    v_registers[x] = sum & 0xFF;
                    break;
                }

                case 0x5: // 8XY5 -- VX is set to the value of the difference of VX and VY [Potential Flag]
                    v_registers[0xF] = v_registers[x] >= v_registers[y];
                    v_registers[x] -= v_registers[y];
                    break;

                case 0x6: // 8XY6 -- VX is set to the value of the right shift bitwise operation on VX and VY [Potential Flag]
                    v_registers[0xF] = v_registers[x] & 0x01;
                    v_registers[x] >>= 1;
                    break;

                case 0x7: // 8XY7 -- VX is set to the value of the difference of VY and VX [Potential Flag]
                    v_registers[0xF] = v_registers[y] >= v_registers[x];
                    v_registers[x] = v_registers[y] - v_registers[x];
                    break;

                case 0xE: // 8XYE -- VX is set to the value of the left shift bitwise operation on VX and VY [Potential Flag]
                    v_registers[0xF] = (v_registers[x] & 0x80) >> 7;
                    v_registers[x] <<= 1;
                    break;

		default:
			std::clog << "Unknown opcode: " << std::hex << opcode << "\n";
            }
            break;

        case 0x9000: // 9XY0 -- Skips the next instruction if the values in VX and VY are NOT equal
            if (v_registers[x] != v_registers[y]) pc += 2;
            break;

        case 0xA000: // ANNN -- Sets the index register to the value NNN
            index = nnn;
            break;

        case 0xB000: // BNNN -- Jumps to the instruction at NNN with an offset of V0
            pc = nnn + v_registers[0] - 2;
            break;

        case 0xC000: // CXNN -- Generates a random number in VX
            v_registers[x] = (rand() % 256) & kk;
            break;

        case 0xD000: // DXYN -- Draws an N tall "sprite" starting at the memory location of the index register drawing it on the X coordinate gained from VX and Y coordinate gained from VY [Potential Flag]
        {
            uint8_t xpos = v_registers[x] % 64;
            uint8_t ypos = v_registers[y] % 32;

            v_registers[0xF] = 0;

            for (std::size_t row = 0; row < n; ++row)
            {
                uint8_t sprite_byte = ram[index + row];

                for (std::size_t col = 0; col < 8; ++col)
                {
                    if ((sprite_byte & (0x80 >> col)) != 0)
                    {
			uint8_t screen_x = (xpos + col) % 64;
			uint8_t screen_y = (ypos + row) % 32;
                        std::size_t pixel_index = index_pixel(screen_x, screen_y);

                        if (graphics[pixel_index])
                            v_registers[0xF] = 1;

                        graphics[pixel_index] ^= 1;
                    }
                }
            }

            draw = true;
            break;
        }

        case 0xE000:
            switch (kk)
            {
                case 0x9E: // EX9E -- Skips one instruction if the current key being held down is correspondant to the value of VX
                    if (keys[v_registers[x]]) pc += 2;
                    break;

                case 0xA1: // EXA1 -- Skips one instruction if the current key being held down is NOT correspondant to the value of VX
                    if (!keys[v_registers[x]]) pc += 2;
                    break;

		default:
			std::clog << "Unknown opcode: " << std::hex << opcode << "\n";
            }
            break;

        case 0xF000:
            switch (kk)
            {
                case 0x07: // FX07 -- Sets VX to the current value of the delay timer
                    v_registers[x] = delay_timer;
                    break;

                case 0x0A: // FX0A -- Will halt the program counter incrementation until the required key that is correspondant to VX is pressed
                {
                    bool found = false;

                    for (std::size_t i = 0; i < 16; ++i)
                    {
                        if (keys[i])
                        {
                            v_registers[x] = i;
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                        pc -= 2;

                    break;
                }

                case 0x15: // FX15 -- Sets the delay timer to the current value in VX
                    delay_timer = v_registers[x];
                    break;

                case 0x18: // FX18 -- Sets the sound timer to the current value in VX
                    sound_timer = v_registers[x];
                    break;

                case 0x1E: // FX1E -- The index register will get the value of VX added into it
                    index += v_registers[x];
                    break;

                case 0x29: // FX29 -- Index register is set to the start of the address of the hexadecimal character correspondant to VX
                    index = v_registers[x] * 5;
                    break;

                case 0x33: // FX33 -- Binary coded decimal conversion for a 3 digit number
                    ram[index]     = v_registers[x] / 100;
                    ram[index + 1] = (v_registers[x] / 10) % 10;
                    ram[index + 2] = v_registers[x] % 10;
                    break;

                case 0x55: // FX55 -- Stores memory from V0 to VX starting at the index pointer in the memory
                    for (std::size_t i = 0; i <= x; ++i)
                        ram[index + i] = v_registers[i];
                    break;

                case 0x65: // FX65 -- Loads memory starting from the index pointer in the memory into V0 to VX
                    for (std::size_t i = 0; i <= x; ++i)
                        v_registers[i] = ram[index + i];
                    break;

		default:
			std::clog << "Unknown opcode: " << std::hex << opcode << "\n";
            }
            break;
	default:
		std::cout << "Found unknown opcode: " << std::hex << opcode << '\n';
    }

    pc += 2;
}
