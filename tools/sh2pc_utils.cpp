#include <fcntl.h>
#include <unistd.h>

#include <bitset>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "emp-sh2pc/emp-sh2pc.h"

using namespace emp;

template <std::size_t batch_size, typename T>
bool read_from_file(int fd, std::vector<T>& output) {
    std::array<T, batch_size> data;
    std::size_t read_size;
    while ((read_size = read(fd, &data, sizeof(T) * batch_size)) > 0) {
        if (read_size == -1) {
            std::cout << "Read data failed" << std::endl;
            return false;
        }
        if (read_size % sizeof(T) != 0) {
            std::cout << "Read data failed" << std::endl;
            return false;
        }
        read_size /= sizeof(T);
        for (std::size_t i = 0; i != read_size; i++) {
            output.push_back(data[i]);
        }
    }
    return true;
}

void encrypt_file(int party, char* input_file, char* output_file) {
    std::vector<unsigned char> input_data;
    std::vector<Integer> alice_encrypt;
    std::vector<Integer> bob_encrypt;

    int input_fd, output_fd;

    input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        std::cout << "Open file failed" << std::endl;
        return;
    }

    if (read_from_file<4096>(input_fd, input_data) == false) {
        goto exit;
    }

    for (unsigned char data : input_data) {
        alice_encrypt.push_back(Integer((party == ALICE) ? std::bitset<8>(data) : std::bitset<8>(), ALICE));
        bob_encrypt.push_back(Integer((party == BOB) ? std::bitset<8>(data) : std::bitset<8>(), BOB));
    }

    output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd == -1) {
        std::cout << "Open file failed" << std::endl;
        goto exit;
    }

    for (Integer data : alice_encrypt) {
        if (write(output_fd, data.bits, sizeof(Bit) * 8) != sizeof(Bit) * 8) {
            std::cout << "Write data failed" << std::endl;
            break;
        }
    }

    for (Integer data : bob_encrypt) {
        if (write(output_fd, data.bits, sizeof(Bit) * 8) != sizeof(Bit) * 8) {
            std::cout << "Write data failed" << std::endl;
            break;
        }
    }

    close(output_fd);
exit:
    close(input_fd);
}

void decrypt_file(int party, char* input_file, char* output_file) {
    std::vector<std::array<block, 8>> input_data;
    std::vector<unsigned char> plain;

    int input_fd, output_fd;

    input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        std::cout << "Open file failed" << std::endl;
        return;
    }

    if (read_from_file<32>(input_fd, input_data) == false) {
        goto exit;
    }

    for (std::array<block, 8> data : input_data) {
        Integer encrypt(8, data.data());
        plain.push_back(static_cast<unsigned char>(encrypt.reveal<8>().to_ulong()));
    }

    output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd == -1) {
        std::cout << "Open file failed" << std::endl;
        goto exit;
    }

    if (write(output_fd, plain.data(), sizeof(unsigned char) * plain.size()) != sizeof(unsigned char) * plain.size()) {
        std::cout << "Write data failed" << std::endl;
    }

    close(output_fd);
exit:
    close(input_fd);
}

void merge_sorted(int party, char* input_file, char* output_file) {
    std::vector<std::pair<std::array<block, 32>, std::array<block, 96>>> input_data;
    std::vector<Integer> key;
    std::vector<Integer> value;

    int input_fd, output_fd;

    input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        std::cout << "Open file failed" << std::endl;
        return;
    }

    if (read_from_file<2>(input_fd, input_data) == false) {
        goto exit;
    }

    for (std::pair<std::array<block, 32>, std::array<block, 96>> data : input_data) {
        key.push_back(Integer(32, data.first.data()));
        value.push_back(Integer(96, data.second.data()));
    }

    bitonic_merge(key.data(), value.data(), 0, key.size(), true);

    output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd == -1) {
        std::cout << "Open file failed" << std::endl;
        goto exit;
    }

    for (std::size_t i = 0; i != key.size(); i++) {
        if (write(output_fd, key[i].bits, sizeof(Bit) * 32) != sizeof(Bit) * 32) {
            std::cout << "Write key failed" << std::endl;
            break;
        }
        if (write(output_fd, value[i].bits, sizeof(Bit) * 96) != sizeof(Bit) * 96) {
            std::cout << "Write value failed" << std::endl;
            break;
        }
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

        NetIO io(party == ALICE ? nullptr : other_ip, port, true);

        setup_semi_honest(&io, party);

        encrypt_file(party, input_file, output_file);
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

        NetIO io(party == ALICE ? nullptr : other_ip, port, true);

        setup_semi_honest(&io, party);

        decrypt_file(party, input_file, output_file);
    } else if (strcmp(argv[1], "merge_sorted") == 0) {
        if (argc != 7) {
            std::cout << "Usage: " << argv[0] << " merge_sorted [party] [port] [other_ip] [input_file] [output_file]"
                      << std::endl;
            return 1;
        }

        int party = atoi(argv[2]);
        int port = atoi(argv[3]);
        char* other_ip = argv[4];
        char* input_file = argv[5];
        char* output_file = argv[6];

        NetIO io(party == ALICE ? nullptr : other_ip, port, true);

        setup_semi_honest(&io, party);

        merge_sorted(party, input_file, output_file);
    }
    auto end = std::chrono::steady_clock::now();

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Total: " << ms.count() << " ms" << std::endl;
}
