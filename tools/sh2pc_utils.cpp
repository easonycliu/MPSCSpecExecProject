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
void encrypt_file(int party, const std::vector<std::bitset<width>>& input_data, std::vector<Integer>& output_data) {
    std::vector<Integer> alice_output_data;
    std::vector<Integer> bob_output_data;
    for (std::bitset<width> data : input_data) {
        alice_output_data.push_back(Integer((party == ALICE) ? data : std::bitset<width>(8), ALICE));
        bob_output_data.push_back(Integer((party == BOB) ? data : std::bitset<width>(), BOB));
    }
    output_data.insert(output_data.end(), alice_output_data.begin(), alice_output_data.end());
    output_data.insert(output_data.end(), bob_output_data.begin(), bob_output_data.end());
}

template <std::size_t width>
void decrypt_file(int party, const std::vector<Integer>& input_data, std::vector<std::bitset<width>>& output_data) {
    for (Integer data : input_data) {
        output_data.push_back(data.reveal<width>());
    }
}

template <std::size_t width>
void merge_sorted(int party, const std::vector<Integer>& input_data, std::vector<Integer>& output_data) {
    static_assert(width % 8 == 0, "Width must be multiple of 8");

    constexpr std::size_t bytes = width / 8;

    std::vector<Integer> key;
    std::vector<Integer> value;

    for (std::size_t i = 0; i < input_data.size(); i += 4) {
        key.push_back(input_data[i]);

        Integer vitem(std::vector<Bit>(input_data[i + 1].bits.begin(), input_data[i + 1].bits.end()));
        vitem.bits.insert(vitem.bits.end(), input_data[i + 2].bits.begin(), input_data[i + 2].bits.end());
        vitem.bits.insert(vitem.bits.end(), input_data[i + 3].bits.begin(), input_data[i + 3].bits.end());
        value.push_back(vitem);
    }

    bitonic_merge(key.data(), value.data(), 0, key.size(), true);

    for (std::size_t i = 0; i != key.size(); i++) {
        output_data.push_back(key[i]);
        output_data.push_back(Integer(std::vector<Bit>(value[i].bits.begin(), value[i].bits.begin() + width)));
        output_data.push_back(
            Integer(std::vector<Bit>(value[i].bits.begin() + width, value[i].bits.begin() + 2 * width)));
        output_data.push_back(
            Integer(std::vector<Bit>(value[i].bits.begin() + 2 * width, value[i].bits.begin() + 3 * width)));
    }
}

int main(int argc, char** argv) {
    if (argc < 7) {
        std::cout << "Usage: " << argv[0] << " [problem_name] [party] [port] [other_ip] [input_file] [output_file]"
                  << std::endl;
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
    read_from_file<width, bs>(input_file, input_data);

    NetIO io(party == ALICE ? nullptr : other_ip, port, true);
    setup_semi_honest(&io, party);

    auto start = std::chrono::steady_clock::now();

    std::vector<Integer> input_data_encrypt;
    encrypt_file(party, input_data, input_data_encrypt);

    std::vector<Integer> output_data_encrypt;
    if (strcmp(problem_name, "merge_sorted") == 0) {
        merge_sorted<width>(party, input_data_encrypt, output_data_encrypt);
    }

    std::vector<std::bitset<width>> output_data;
    decrypt_file(party, output_data_encrypt, output_data);

    auto end = std::chrono::steady_clock::now();

    write_to_file<width, bs>(output_file, output_data);

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Total: " << ms.count() << " ms" << std::endl;
}
