#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "emp-sh2pc/emp-sh2pc.h"

using namespace emp;

template <std::size_t width, std::size_t bs>
bool read_from_file(const std::string& file, std::vector<std::bitset<width>>& data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	constexpr std::size_t bytes = width / 8;

	std::ifstream stream(file, std::ios::binary);
	if (!stream.is_open()) {
		std::cout << "Open file failed" << std::endl;
		return false;
	}


	std::size_t read_size = 0;
	std::array<std::byte, bytes * bs> buffer;

	std::vector<std::byte> bdata;
	while ((read_size = stream.readsome(reinterpret_cast<char*>(buffer.data()), sizeof(buffer))) > 0) {
		bdata.insert(bdata.end(), buffer.begin(), buffer.begin() + read_size);
	}

	stream.close();

	std::uintptr_t start = reinterpret_cast<std::uintptr_t>(bdata.data());
	std::size_t size = bdata.size();

	for (std::uintptr_t addr = start; addr < start + size; addr += bytes) {
		std::byte *bitems = reinterpret_cast<std::byte*>(addr);
		std::bitset<width> item(0);
		for (std::size_t i = 0; i < bytes; i++) {
			item <<= 8;
			item |= std::bitset<width>(std::to_integer<int>(bitems[i]));
		}
		data.push_back(item);
	}

    return true;
}

template <std::size_t width, std::size_t bs>
bool write_to_file(const std::string& file, const std::vector<std::bitset<width>>& data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	constexpr std::size_t bytes = width / 8;

	std::ofstream stream(file, std::ios::binary);
	if (!stream.is_open()) {
		std::cout << "Open file failed" << std::endl;
		return false;
	}

	std::array<std::byte, bytes * bs> buffer;
	stream.rdbuf()->pubsetbuf(reinterpret_cast<char*>(buffer.data()), sizeof(buffer));

	for (std::bitset<width> item : data) {
		for (std::size_t i = 0; i < bytes; i++) {
			std::byte bitem = static_cast<std::byte>((item >> (bytes - i - 1) * 8).to_ulong());
			stream.write(reinterpret_cast<const char*>(&bitem), sizeof(bitem));
		}
	}

	stream.flush();
	stream.close();

	return true;
}

template <std::size_t width>
void encrypt_file(int party, const std::vector<std::bitset<width>>& input_data, std::vector<std::bitset<sizeof(Bit) * width * 8>>& output_data) {
	std::vector<std::bitset<sizeof(Bit) * width * 8>> alice_output_data;
	std::vector<std::bitset<sizeof(Bit) * width * 8>> bob_output_data;
    for (std::bitset<width> data : input_data) {
		Integer alice_encrypt((party == ALICE) ? data : std::bitset<width>(8), ALICE);
		std::bitset<sizeof(Bit) * width * 8> alice_encrypt_bit(0);
		std::memcpy(&alice_encrypt_bit, alice_encrypt.bits.data(), sizeof(Bit) * width);
		alice_output_data.push_back(alice_encrypt_bit);

		Integer bob_encrypt((party == BOB) ? data : std::bitset<width>(), BOB);
		std::bitset<sizeof(Bit) * width * 8> bob_encrypt_bit(0);
		std::memcpy(&bob_encrypt_bit, bob_encrypt.bits.data(), sizeof(Bit) * width);
		bob_output_data.push_back(bob_encrypt_bit);
    }

	output_data.insert(output_data.end(), alice_output_data.begin(), alice_output_data.end());
	output_data.insert(output_data.end(), bob_output_data.begin(), bob_output_data.end());
}

template <std::size_t width>
void encrypt_file(int party, const std::vector<std::bitset<width>>& input_data, std::vector<std::bitset<width>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	constexpr std::size_t bytes = width / 8;

	std::vector<std::bitset<sizeof(Bit) * width * 8>> internal_output_data;
	encrypt_file(party, input_data, internal_output_data);

	for (std::bitset<sizeof(Bit) * width * 8> data : internal_output_data) {
		std::bitset<width> bitem(0);
		for (std::size_t i = 0; i < sizeof(Bit) * 8; i++) {
			std::memcpy(&bitem, reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&data) + i * bytes), bytes);
			output_data.push_back(bitem);
		}
	}
}

template <std::size_t width>
void decrypt_file(int party, const std::vector<std::bitset<sizeof(Bit) * width * 8>>& input_data, std::vector<std::bitset<width>>& output_data) {
	for (std::bitset<sizeof(Bit) * width * 8> data : input_data) {
		Integer encrypt(width, reinterpret_cast<Bit*>(&data));
		std::bitset<width> plain = encrypt.reveal<width>();
		output_data.push_back(plain);
	}
}

template <std::size_t width>
void decrypt_file(int party, const std::vector<std::bitset<width>>& input_data, std::vector<std::bitset<width>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	constexpr std::size_t bytes = width / 8;

	std::vector<std::bitset<sizeof(Bit) * width * 8>> internal_input_data;
	for (std::size_t i = 0; i < input_data.size(); i += sizeof(Bit) * 8) {
		std::bitset<sizeof(Bit) * width * 8> bitem(0);
		for (std::size_t j = 0; j < sizeof(Bit) * 8; j++) {
			std::memcpy(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&bitem) + j * bytes), &input_data[i + j], bytes);
		}
		internal_input_data.push_back(bitem);
	}

	decrypt_file(party, internal_input_data, output_data);
}

template <std::size_t width>
void merge_sorted_internal(int party, const std::vector<std::bitset<sizeof(Bit) * width * 8>>& input_data, std::vector<std::bitset<sizeof(Bit) * width * 8>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	constexpr std::size_t bytes = width / 8;

    std::vector<Integer> key;
    std::vector<Integer> value;

	for (std::size_t i = 0; i < input_data.size(); i += 4) {
        key.push_back(Integer(width, &(input_data[i])));

		std::bitset<sizeof(Bit) * width * 8 * 3> bvalue(0);
		std::memcpy(&bvalue, &(input_data[i + 1]), sizeof(Bit) * width);
		std::memcpy(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&bvalue) + sizeof(Bit) * width), &(input_data[i + 2]), sizeof(Bit) * width);
		std::memcpy(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&bvalue) + sizeof(Bit) * width * 2), &(input_data[i + 3]), sizeof(Bit) * width);
        value.push_back(Integer(width * 3, &bvalue));
    }

    bitonic_merge(key.data(), value.data(), 0, key.size(), true);

    for (std::size_t i = 0; i != key.size(); i++) {
		std::bitset<sizeof(Bit) * width * 8> bitem(0);

		std::memcpy(&bitem, key[i].bits.data(), sizeof(Bit) * width);
		output_data.push_back(bitem);

		std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(value[i].bits.data());

		std::memcpy(&bitem, reinterpret_cast<void*>(addr), sizeof(Bit) * width);
		output_data.push_back(bitem);

		std::memcpy(&bitem, reinterpret_cast<void*>(addr + sizeof(Bit) * width), sizeof(Bit) * width);
		output_data.push_back(bitem);

		std::memcpy(&bitem, reinterpret_cast<void*>(addr + sizeof(Bit) * width * 2), sizeof(Bit) * width);
		output_data.push_back(bitem);
    }
}

template <std::size_t width>
void merge_sorted(int party, const std::vector<std::bitset<width>>& input_data, std::vector<std::bitset<width>>& output_data) {
	static_assert(width % 8 == 0, "Width must be multiple of 8");

	constexpr std::size_t bytes = width / 8;

	std::vector<std::bitset<sizeof(Bit) * width * 8>> internal_input_data;
	for (std::size_t i = 0; i < input_data.size(); i += sizeof(Bit) * 8) {
		std::bitset<sizeof(Bit) * width * 8> bitem(0);
		for (std::size_t j = 0; j < sizeof(Bit) * 8; j++) {
			std::memcpy(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&bitem) + j * bytes), &input_data[i + j], bytes);
		}
		internal_input_data.push_back(bitem);
	}

	std::vector<std::bitset<sizeof(Bit) * width * 8>> internal_output_data;
	merge_sorted_internal<width>(party, internal_input_data, internal_output_data);

	for (std::bitset<sizeof(Bit) * width * 8> data : internal_output_data) {
		std::bitset<width> bitem(0);
		for (std::size_t i = 0; i < sizeof(Bit) * 8; i++) {
			std::memcpy(&bitem, reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&data) + i * bytes), bytes);
			output_data.push_back(bitem);
		}
	}
}

int main(int argc, char** argv) {
    if (argc < 7) {
        std::cout << "Usage: " << argv[0] << " [problem_name] [party] [port] [other_ip] [input_file] [output_file]" << std::endl;
        return 1;
    }

	char* problem_name = argv[1];
    int party = atoi(argv[2]);
    int port = atoi(argv[3]);
    char* other_ip = argv[4];
    char* input_file = argv[5];
    char* output_file = argv[6];

	constexpr std::size_t width = 32;
	constexpr std::size_t bs = 4096;

	std::vector<std::bitset<width>> input_data;
	std::vector<std::bitset<width>> output_data;

	read_from_file<width, bs>(input_file, input_data);

    auto start = std::chrono::steady_clock::now();
    if (strcmp(problem_name, "encrypt_file") == 0) {
        NetIO io(party == ALICE ? nullptr : other_ip, port, true);
        setup_semi_honest(&io, party);
        encrypt_file(party, input_data, output_data);
    } else if (strcmp(problem_name, "decrypt_file") == 0) {
        NetIO io(party == ALICE ? nullptr : other_ip, port, true);
        setup_semi_honest(&io, party);
        decrypt_file(party, input_data, output_data);
    } else if (strcmp(problem_name, "merge_sorted") == 0) {
        NetIO io(party == ALICE ? nullptr : other_ip, port, true);
        setup_semi_honest(&io, party);
        merge_sorted(party, input_data, output_data);
    }
    auto end = std::chrono::steady_clock::now();

	write_to_file<width, bs>(output_file, output_data);

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Total: " << ms.count() << " ms" << std::endl;
}
