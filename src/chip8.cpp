#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "chip8.h"

bool Chip8::load(const std::filesystem::path& path)
{
	std::clog << "Started loading ROM: " << path.string() << "\n";
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
                case 0x00E0:
                    graphics.fill(0);
                    draw = true;
                    break;

                case 0x00EE:
                    pc = stack[--sp];
                    break;
            }
            break;

        case 0x1000:
            pc = nnn - 2;
            break;

        case 0x2000:
            stack[sp++] = pc;
            pc = nnn - 2;
            break;

        case 0x3000:
            if (v_registers[x] == kk) pc += 2;
            break;

        case 0x4000:
            if (v_registers[x] != kk) pc += 2;
            break;

        case 0x5000:
            if (v_registers[x] == v_registers[y]) pc += 2;
            break;

        case 0x6000:
            v_registers[x] = kk;
            break;

        case 0x7000:
            v_registers[x] += kk;
            break;

        case 0x8000:
            switch (n)
            {
                case 0x0:
                    v_registers[x] = v_registers[y];
                    break;

                case 0x1:
                    v_registers[x] |= v_registers[y];
                    break;

                case 0x2:
                    v_registers[x] &= v_registers[y];
                    break;

                case 0x3:
                    v_registers[x] ^= v_registers[y];
                    break;

                case 0x4:
                {
                    uint16_t sum = v_registers[x] + v_registers[y];
                    v_registers[0xF] = sum > 0xFF;
                    v_registers[x] = sum & 0xFF;
                    break;
                }

                case 0x5:
                    v_registers[0xF] = v_registers[x] >= v_registers[y];
                    v_registers[x] -= v_registers[y];
                    break;

                case 0x6:
                    v_registers[0xF] = v_registers[x] & 0x01;
                    v_registers[x] >>= 1;
                    break;

                case 0x7:
                    v_registers[0xF] = v_registers[y] >= v_registers[x];
                    v_registers[x] = v_registers[y] - v_registers[x];
                    break;

                case 0xE:
                    v_registers[0xF] = (v_registers[x] & 0x80) >> 7;
                    v_registers[x] <<= 1;
                    break;
            }
            break;

        case 0x9000:
            if (v_registers[x] != v_registers[y]) pc += 2;
            break;

        case 0xA000:
            index = nnn;
            break;

        case 0xB000:
            pc = nnn + v_registers[0] - 2;
            break;

        case 0xC000:
            v_registers[x] = (rand() % 256) & kk;
            break;

        case 0xD000:
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
                case 0x9E:
                    if (keys[v_registers[x]]) pc += 2;
                    break;

                case 0xA1:
                    if (!keys[v_registers[x]]) pc += 2;
                    break;
            }
            break;

        case 0xF000:
            switch (kk)
            {
                case 0x07:
                    v_registers[x] = delay_timer;
                    break;

                case 0x0A:
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

                case 0x15:
                    delay_timer = v_registers[x];
                    break;

                case 0x18:
                    sound_timer = v_registers[x];
                    break;

                case 0x1E:
                    index += v_registers[x];
                    break;

                case 0x29:
                    index = v_registers[x] * 5;
                    break;

                case 0x33:
                    ram[index]     = v_registers[x] / 100;
                    ram[index + 1] = (v_registers[x] / 10) % 10;
                    ram[index + 2] = v_registers[x] % 10;
                    break;

                case 0x55:
                    for (std::size_t i = 0; i <= x; ++i)
                        ram[index + i] = v_registers[i];
                    break;

                case 0x65:
                    for (std::size_t i = 0; i <= x; ++i)
                        v_registers[i] = ram[index + i];
                    break;
            }
            break;
	default:
		std::cout << "Found unknown opcode: " << std::hex << opcode << '\n';
    }

    pc += 2;
}
