#include <fcntl.h>
#include <unistd.h>

#include <bitset>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "emp-sh2pc/emp-sh2pc.h"

using namespace emp;

void encrypt_file(int party, char* input_file, char* output_file) {
    std::vector<std::bitset<64>> input_data;

    int input_fd, output_fd;

    input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        std::cout << "Open file failed" << std::endl;
        return;
    }

    std::bitset<64> read_data;
    while (read(input_fd, &read_data, sizeof(std::bitset<64>)) == sizeof(std::bitset<64>)) {
        input_data.push_back(read_data);
    }

    std::vector<Integer> alice_encrypt;
    std::vector<Integer> bob_encrypt;
    for (std::bitset<64> data : input_data) {
        alice_encrypt.push_back(Integer((party == ALICE) ? data : std::bitset<64>(), ALICE));
        bob_encrypt.push_back(Integer((party == BOB) ? data : std::bitset<64>(), BOB));
    }

    output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd == -1) {
        std::cout << "Open file failed" << std::endl;
        goto exit;
    }

    for (Integer data : alice_encrypt) {
        if (write(output_fd, data.bits, sizeof(Bit) * 64) != sizeof(Bit) * 64) {
            std::cout << "Write data failed" << std::endl;
            break;
        }
    }

    for (Integer data : bob_encrypt) {
        if (write(output_fd, data.bits, sizeof(Bit) * 64) != sizeof(Bit) * 64) {
            std::cout << "Write data failed" << std::endl;
            break;
        }
    }

    close(output_fd);
exit:
    close(input_fd);
}

void decrypt_file(int party, char* input_file, char* output_file) {
    std::vector<std::array<block, 64>> input_data;

    int input_fd, output_fd;

    input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        std::cout << "Open file failed" << std::endl;
        return;
    }

    std::array<block, 64> read_data;
    while (read(input_fd, &read_data, sizeof(std::array<block, 64>)) == sizeof(std::array<block, 64>)) {
        input_data.push_back(read_data);
    }

    std::vector<std::bitset<64>> plain;
    for (std::array<block, 64> data : input_data) {
        Integer encrypt(64, data.data());
        plain.push_back(encrypt.reveal<64>());
    }

    output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd == -1) {
        std::cout << "Open file failed" << std::endl;
        goto exit;
    }

    if (write(output_fd, plain.data(), sizeof(std::bitset<64>) * plain.size()) !=
        sizeof(std::bitset<64>) * plain.size()) {
        std::cout << "Write data failed" << std::endl;
    }

    close(output_fd);
exit:
    close(input_fd);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " [commands]" << std::endl;
        return 1;
    }

    auto start = std::chrono::steady_clock::now();
    if (strcmp(argv[1], "encrypt_file") == 0) {
        if (argc != 7) {
            std::cout << "Usage: " << argv[0] << " encrypt_file [party] [port] [other_ip] [input_file] [output_file]"
                      << std::endl;
            return 1;
        }

        int party = atoi(argv[2]);
        int port = atoi(argv[3]);
        char* other_ip = argv[4];
        char* input_file = argv[5];
        char* output_file = argv[6];

        NetIO* io = new NetIO(party == ALICE ? nullptr : other_ip, port);

        setup_semi_honest(io, party);

        encrypt_file(party, input_file, output_file);

        delete io;
    } else if (strcmp(argv[1], "decrypt_file") == 0) {
        if (argc != 7) {
            std::cout << "Usage: " << argv[0] << " decrypt_file [party] [port] [other_ip] [input_file] [output_file]"
                      << std::endl;
            return 1;
        }

        int party = atoi(argv[2]);
        int port = atoi(argv[3]);
        char* other_ip = argv[4];
        char* input_file = argv[5];
        char* output_file = argv[6];

        NetIO* io = new NetIO(party == ALICE ? nullptr : other_ip, port);

        setup_semi_honest(io, party);

        decrypt_file(party, input_file, output_file);

        delete io;
    }
    auto end = std::chrono::steady_clock::now();

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Total: " << ms.count() << " ms" << std::endl;
}
