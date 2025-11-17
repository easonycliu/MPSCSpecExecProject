CXX = clang++

ifndef PROJECT_DIR
$(error PROJECT_DIR is not set)
endif

DEP_INSTALL_DIR = $(PROJECT_DIR)/install
DEP_BUILD_DIR = $(PROJECT_DIR)/build

TOOLS_SOURCE = $(wildcard $(PROJECT_DIR)/tools/*.cpp)
TOOLS_OBJECTS = $(addprefix $(DEP_BUILD_DIR)/tools/,$(foreach file,$(TOOLS_SOURCE),$(notdir $(basename $(file))).o))
TOOLS_EXECUTABLES = $(addprefix $(DEP_INSTALL_DIR)/tools/,$(foreach file,$(TOOLS_SOURCE),$(notdir $(basename $(file)))))

ifndef JOBS
JOBS := $(shell nproc)
endif

.PHONY: all

all: $(TOOLS_EXECUTABLES)

$(DEP_INSTALL_DIR)/tools/%: $(DEP_BUILD_DIR)/tools/%.o $(DEP_INSTALL_DIR)/tools
	$(CXX) $< -pthread -laio -lssl -lcrypto -lboost_program_options -lboost_random -lboost_system -lgmp \
		-Wl,-rpath,$(PROJECT_DIR)/linux/tools/lib/bpf \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/yaml-cpp/lib \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/tfhe/lib \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/mage/lib \
		-Wl,-rpath,$(PROJECT_DIR)/osprey/bin \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/oblivious-SEAL/lib \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/emp-tool/lib \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/libOTe \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/MP-SPDZ \
		-L$(PROJECT_DIR)/linux/tools/lib/bpf -l:libbpf.so \
		-L$(PROJECT_DIR)/install/yaml-cpp/lib -l:libyaml-cpp.so \
		-L$(PROJECT_DIR)/install/tfhe/lib -l:libtfhe-spqlios-fma.so \
		-L$(PROJECT_DIR)/install/mage/lib -l:libmage.so \
		-L$(PROJECT_DIR)/osprey/bin -l:libosprey.so \
		-L$(PROJECT_DIR)/install/oblivious-SEAL/lib -l:libseal.so.4.1.1 \
		-L$(PROJECT_DIR)/install/emp-tool/lib -l:libemp-tool.so \
		-L$(PROJECT_DIR)/install/libOTe/lib -l:libcryptoTools.a -l:liblibOTe.a \
		-L$(PROJECT_DIR)/install/MP-SPDZ -l:libSPDZ.so -l:libFHE.so \
		-o $@

$(DEP_BUILD_DIR)/tools/%.o: $(PROJECT_DIR)/tools/%.cpp $(DEP_BUILD_DIR)/tools
	$(CXX) -std=c++20 -Ofast -g -fPIE -march=native -maes -mrdseed -ggdb3 -pthread -Wno-deprecated-declarations \
		-DBOOST_ALL_NO_LIB -DBOOST_SYSTEM_DYN_LINK -DEMP_CIRCUIT_PATH=$(DEP_INSTALL_DIR)/emp-tool/include/emp-tool/circuits/files/ -DEMP_USE_RANDOM_DEVICE -DCKKS -DUSE_NTL -DUSE_GF2N_LONG '-DPREP_DIR="Player-Data/"' '-DSSL_DIR="Player-Data/"' -MMD -MP -c \
		-I$(PROJECT_DIR)/install/yaml-cpp/include \
		-I$(PROJECT_DIR)/install/tfhe/include \
		-I$(PROJECT_DIR)/install/oblivious-SEAL/include/SEAL-4.1 \
		-I$(PROJECT_DIR)/mage/src \
		-I$(PROJECT_DIR)/osprey \
		-I$(PROJECT_DIR)/osprey/bin/include \
		-I$(PROJECT_DIR)/install/emp-tool/include \
		-I$(PROJECT_DIR)/install/emp-ot/include \
		-I$(PROJECT_DIR)/install/emp-sh2pc/include \
		-I$(PROJECT_DIR)/install/libOTe/include \
		-I$(PROJECT_DIR)/install/MP-SPDZ/local/include \
		-I$(PROJECT_DIR)/install/MP-SPDZ/deps \
		-I$(PROJECT_DIR)/install/MP-SPDZ \
		-c $< -o $@

$(DEP_INSTALL_DIR)/tools: $(DEP_INSTALL_DIR)
	mkdir $@ || true

$(DEP_BUILD_DIR)/tools: $(DEP_BUILD_DIR)
	mkdir $@ || true

clean:
	rm -rf $(DEP_BUILD_DIR)/tools $(DEP_INSTALL_DIR)/tools

