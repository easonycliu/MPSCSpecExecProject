#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <iostream>

#include "emp-sh2pc/emp-sh2pc.h"

using namespace emp;

void encrypt_merge_data(int party, int input_size_per_party, char* output_file, int key_bits = 32,
                        int value_bits = 96) {
    int input_array_length = input_size_per_party * 2;
    Integer* key = new Integer[input_array_length];
    Integer* value = new Integer[input_array_length];

    auto start_gen = std::chrono::steady_clock::now();
    for (int i = 0; i != input_array_length; i++) {
        if (i < input_size_per_party) {
            key[i] = Integer(key_bits, i, ALICE);
            value[i] = Integer(value_bits, i, ALICE);
        } else {
            key[i] = Integer(key_bits, input_array_length - i - 1, BOB);
            value[i] = Integer(value_bits, input_array_length - i - 1, BOB);
        }
    }
    auto end_gen = std::chrono::steady_clock::now();
    std::chrono::milliseconds ms_gen = std::chrono::duration_cast<std::chrono::milliseconds>(end_gen - start_gen);
    std::cout << "Generate: " << ms_gen.count() << " ms" << std::endl;

    int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    for (int i = 0; i != input_array_length; i++) {
        int size = write(output_fd, key[i].bits, sizeof(Bit) * key_bits);
        if (size != sizeof(Bit) * key_bits) {
            std::cout << "Write key failed" << std::endl;
            goto exit;
        }
        size = write(output_fd, value[i].bits, sizeof(Bit) * value_bits);
        if (size != sizeof(Bit) * value_bits) {
            std::cout << "Write value failed" << std::endl;
            goto exit;
        }

        std::cout << "Write key: " << key[i].reveal<int>() << std::endl;
    }

exit:
    close(output_fd);

    delete[] key;
    delete[] value;
}

void decrypt_merge_data(int party, int input_size_per_party, char* input_file, int key_bits = 32, int value_bits = 96) {
    int input_array_length = input_size_per_party * 2;
    Integer* key = new Integer[input_array_length];
    Integer* value = new Integer[input_array_length];

    int input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        std::cout << "Open file failed" << std::endl;
        goto exit;
    }

    for (int i = 0; i != input_array_length; i++) {
        key[i] = Integer(key_bits, ALICE);
        value[i] = Integer(value_bits, ALICE);
        int size = read(input_fd, key[i].bits, sizeof(Bit) * key_bits);
        if (size != sizeof(Bit) * key_bits) {
            std::cout << "Read key failed" << std::endl;
            goto exit;
        }
        size = read(input_fd, value[i].bits, sizeof(Bit) * value_bits);
        if (size != sizeof(Bit) * value_bits) {
            std::cout << "Read value failed" << std::endl;
            goto exit;
        }

        std::cout << "Read key: " << key[i].reveal<int>() << std::endl;
    }

exit:
    close(input_fd);

    delete[] key;
    delete[] value;
}

int main(int argc, char** argv) {
    if (argc != 7) {
        std::cout << "Usage: " << argv[0] << " party port problem_size other_ip mode file" << std::endl;
        return 1;
    }

    int size = atoi(argv[3]);

    int port, party;
    parse_party_and_port(argv, &party, &port);
    NetIO* io = new NetIO(party == ALICE ? nullptr : argv[4], port);
    char* mode = argv[5];
    char* file = argv[6];

    setup_semi_honest(io, party);

    auto start = std::chrono::steady_clock::now();
    if (strcmp(mode, "encrypt") == 0)
        encrypt_merge_data(party, size, file);
    else if (strcmp(mode, "decrypt") == 0)
        decrypt_merge_data(party, size, file);
    auto end = std::chrono::steady_clock::now();

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Total: " << ms.count() << " ms" << std::endl;
    delete io;
}
