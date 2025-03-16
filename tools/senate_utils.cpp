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
		std::byte* bitems = reinterpret_cast<std::byte*>(addr);
		std::bitset<width> item(0);
		for (std::size_t i = 0; i < bytes; i++) {
			item |= std::bitset<width>(std::to_integer<int>(bitems[i])) << (i * 8);
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
			std::byte bitem = static_cast<std::byte>((item >> (i * 8)).to_ulong());
			stream.write(reinterpret_cast<const char*>(&bitem), sizeof(bitem));
		}
	}

	stream.flush();
	stream.close();

	return true;
}

template <std::size_t width>
void encrypt_file(
	int party, std::size_t other_input_size, NetIO& io, const std::vector<std::bitset<width>>& input_data,
	std::vector<Integer>& output_data
) {
	std::vector<Integer> alice_output_data;
	std::vector<Integer> bob_output_data;

	std::size_t iters = std::max(input_data.size(), other_input_size);
	for (std::size_t i = 0; i < iters; i++) {
		alice_output_data.push_back(
			Integer((party == ALICE && i < input_data.size()) ? input_data[i] : std::bitset<width>(0), ALICE)
		);
		bob_output_data.push_back(
			Integer((party == BOB && i < input_data.size()) ? input_data[i] : std::bitset<width>(0), BOB)
		);
	}
	io.flush();
	output_data.insert(
		output_data.end(), alice_output_data.begin(),
		alice_output_data.begin() + ((party == ALICE) ? input_data.size() : other_input_size)
	);
	output_data.insert(
		output_data.end(), bob_output_data.begin(),
		bob_output_data.begin() + ((party == BOB) ? input_data.size() : other_input_size)
	);
}

template <std::size_t width>
void decrypt_file(int party, const std::vector<Integer>& input_data, std::vector<std::bitset<width>>& output_data) {
	constexpr std::size_t bs = 4096;
	for (std::size_t i = 0; i < input_data.size(); i += bs) {
		Integer batch(std::vector<Bit>(0));
		for (std::size_t j = 0; j < bs; j++) {
			if (i + j < input_data.size()) {
				batch.bits.insert(batch.bits.end(), input_data[i + j].bits.begin(), input_data[i + j].bits.end());
			}
		}

		std::bitset<width* bs> bbatch = batch.reveal<width * bs>();
		std::size_t output_size = std::min(bs, input_data.size() - i);
		for (std::size_t j = 0; j < width * output_size; j += width) {
			std::bitset<width> item(0);
			for (std::size_t k = 0; k < width; k++) {
				item.set(k, bbatch[j + k]);
			}
			output_data.push_back(item);
		}
	}
}

#include "senate_tpc_h/q4.cpp"
#include "senate_tpc_h/q8.cpp"

std::size_t get_other_input_size(int party, char* problem_name, std::size_t problem_size) {
	if (strcmp(problem_name, "tpc_h_q4") == 0) {
		return senate_tpc_h_q4::get_other_input_size(party, problem_size);
	} else if (strcmp(problem_name, "tpc_h_q8") == 0) {
		return senate_tpc_h_q8::get_other_input_size(party, problem_size);
	} else {
		std::cerr << "Unknown problem name " << problem_name << std::endl;
		std::abort();
	}
}

int main(int argc, char** argv) {
	if (argc != 8) {
		std::cout << "Usage: " << argv[0]
				  << " [problem_name] [problem_size] [party] [port] [other_ip] [input_file] [output_file]" << std::endl;
		return 1;
	}

	char* problem_name = argv[1];
	std::size_t problem_size = std::stoull(argv[2]);
	int party = atoi(argv[3]);
	int port = atoi(argv[4]);
	char* other_ip = argv[5];
	char* input_file = argv[6];
	char* output_file = argv[7];

	constexpr std::size_t width = 32;
	constexpr std::size_t bs = 4096;

	std::vector<std::bitset<width>> input_data;
	read_from_file<width, bs>(input_file, input_data);

	NetIO io(party == ALICE ? nullptr : other_ip, port, true);
	setup_semi_honest(&io, party);

	std::chrono::high_resolution_clock::time_point encrypt_start = std::chrono::high_resolution_clock::now();
	std::vector<Integer> input_data_encrypt;
	encrypt_file(party, get_other_input_size(party, problem_name, problem_size), io, input_data, input_data_encrypt);
	std::chrono::high_resolution_clock::time_point encrypt_end = std::chrono::high_resolution_clock::now();
	std::cout << "Encrypt time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(encrypt_end - encrypt_start).count() << " ms"
			  << std::endl;

	std::vector<Integer> output_data_encrypt;
	if (strcmp(problem_name, "tpc_h_q4") == 0) {
		senate_tpc_h_q4::join_and_aggregate<width>(party, problem_size, input_data_encrypt, output_data_encrypt);
	} else if (strcmp(problem_name, "tpc_h_q8") == 0) {
		senate_tpc_h_q8::join_and_aggregate<width>(party, problem_size, input_data_encrypt, output_data_encrypt);
	} else {
		std::cerr << "Unknown problem name" << std::endl;
		return 1;
	}

	std::chrono::high_resolution_clock::time_point calc_end = std::chrono::high_resolution_clock::now();
	std::cout << "Calc time: " << std::chrono::duration_cast<std::chrono::milliseconds>(calc_end - encrypt_end).count()
			  << " ms" << std::endl;

	std::vector<std::bitset<width>> output_data;
	decrypt_file(party, output_data_encrypt, output_data);
	std::chrono::high_resolution_clock::time_point decrypt_end = std::chrono::high_resolution_clock::now();
	std::cout << "Decrypt time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(decrypt_end - calc_end).count() << " ms"
			  << std::endl;

	std::cout << "Total time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(decrypt_end - encrypt_start).count() << " ms"
			  << std::endl;

	write_to_file<width, bs>(output_file, output_data);
}
