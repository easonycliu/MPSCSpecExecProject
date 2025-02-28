#include <string>
#include <vector>

#include "emp-tool/emp-tool.h"

using namespace emp;

template <std::size_t key_bits, std::size_t value_bits>
void full_sort(std::size_t problem_size, std::size_t num_parties) {
    std::vector<Integer> key;
    std::vector<Integer> value;
    for (std::size_t party = 0; party < num_parties; ++party) {
        for (std::size_t i = 0; i < problem_size; ++i) {
            key.push_back(Integer(key_bits, 0, (party == 0) ? ALICE : BOB));
            value.push_back(Integer(value_bits, 0, (party == 0) ? ALICE : BOB));
        }
    }
    bitonic_sort(key.data(), value.data(), 0, key.size(), true);

    for (std::size_t i = 0; i < problem_size * num_parties; ++i) {
        key[i].reveal<int>();
        value[i].reveal<int>();
    }
}

template <std::size_t key_bits, std::size_t value_bits>
void merge_sorted(std::size_t problem_size, std::size_t num_parties) {
    std::vector<Integer> key;
    std::vector<Integer> value;
    std::size_t array_size = problem_size;
    for (std::size_t i = 0; i < problem_size; ++i) {
        key.push_back(Integer(key_bits, 0, ALICE));
        value.push_back(Integer(value_bits, 0, ALICE));
    }
    for (std::size_t party = 1; party < num_parties; ++party) {
        for (std::size_t i = 0; i < array_size; ++i) {
            key.push_back(Integer(key_bits, 0, BOB));
            value.push_back(Integer(value_bits, 0, BOB));
        }
        array_size *= 2;
    }
    array_size = problem_size * 2;
    while (array_size <= key.size()) {
        bitonic_merge(key.data(), value.data(), 0, array_size, true);
        array_size *= 2;
    }

    array_size = key.size();
    for (std::size_t i = 0; i < problem_size * num_parties; ++i) {
        key[array_size - problem_size * num_parties + i].reveal<int>();
        value[array_size - problem_size * num_parties + i].reveal<int>();
    }
}

int main(int argc, char** argv) {
    if (argc != 5 && argc != 6) {
        std::cout << "Usage: " << argv[0] << " problem_name problem_size num_parties output_path [option]" << std::endl;
        return 0;
    }

    std::size_t problem_size = std::stoull(argv[2]);
    std::size_t num_parties = std::stoull(argv[3]);
    std::string output_path = argv[4];

    if (strcmp(argv[1], "full_sort") == 0) {
        setup_plain_prot(true, output_path);
        full_sort<32, 128>(problem_size, num_parties);
        finalize_plain_prot();
    } else if (strcmp(argv[1], "merge_sorted") == 0) {
        setup_plain_prot(true, output_path);
        merge_sorted<32, 128>(problem_size, num_parties);
        finalize_plain_prot();
    } else {
        std::cout << "Unknown problem name" << std::endl;
    }
}
