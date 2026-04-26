CXX = clang++
PROJECT_DIR = $(shell pwd)
DEP_INSTALL_DIR = $(PROJECT_DIR)/install
DEP_BUILD_DIR = $(PROJECT_DIR)/build

TOOLS_SOURCE = $(wildcard $(PROJECT_DIR)/tools/*.cpp)
TOOLS_OBJECTS = $(addprefix $(DEP_BUILD_DIR)/tools/,$(foreach file,$(TOOLS_SOURCE),$(notdir $(basename $(file))).o))
TOOLS_EXECUTABLES = $(addprefix $(DEP_INSTALL_DIR)/tools/,$(foreach file,$(TOOLS_SOURCE),$(notdir $(basename $(file)))))

ifndef JOBS
JOBS := $(shell nproc)
endif

.PHONY: all clean install_deps tools yaml-cpp tfhe oblivious-SEAL osprey mage emp-tool libOTe MP-SPDZ show fastsudo

all: linux-headers yaml-cpp oblivious-SEAL osprey mage emp-tool libOTe MP-SPDZ tools

mage: yaml-cpp oblivious-SEAL osprey
	cd $(PROJECT_DIR)/mage && \
		make clean PROJECT_DIR=$(PROJECT_DIR) BINDIR=$(DEP_INSTALL_DIR)/mage && \
		make PROJECT_DIR=$(PROJECT_DIR) BINDIR=$(DEP_INSTALL_DIR)/mage -j$(JOBS) && \
		make lib PROJECT_DIR=$(PROJECT_DIR) BINDIR=$(DEP_INSTALL_DIR)/mage -j$(JOBS)

$(DEP_INSTALL_DIR)/mage: $(DEP_INSTALL_DIR)
	mkdir $@ || true

tools: $(DEP_INSTALL_DIR)/tools yaml-cpp osprey oblivious-SEAL mage emp-tool libOTe MP-SPDZ
	make -C $(PROJECT_DIR)/tools -j$(JOBS) PROJECT_DIR=$(PROJECT_DIR)

$(DEP_INSTALL_DIR)/tools: $(DEP_INSTALL_DIR)
	mkdir $@ || true

$(DEP_BUILD_DIR)/tools: $(DEP_BUILD_DIR)
	mkdir $@ || true

linux-headers: $(DEP_BUILD_DIR)
	mkdir -p $(DEP_BUILD_DIR)/linux-headers || true
	cd $(PROJECT_DIR)/linux && \
		make headers_install INSTALL_HDR_PATH=$(DEP_BUILD_DIR)/linux-headers

yaml-cpp: $(DEP_INSTALL_DIR)/yaml-cpp
	cd $(PROJECT_DIR)/yaml-cpp && \
		cmake -B $(DEP_BUILD_DIR)/yaml-cpp -DYAML_BUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=$(DEP_INSTALL_DIR)/$@ && \
		cmake --build $(DEP_BUILD_DIR)/yaml-cpp -j$(JOBS) && \
		cmake --install $(DEP_BUILD_DIR)/yaml-cpp

$(DEP_INSTALL_DIR)/yaml-cpp: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/yaml-cpp
	mkdir $@ || true

$(DEP_BUILD_DIR)/yaml-cpp: $(DEP_BUILD_DIR)
	mkdir $@ || true

tfhe: $(DEP_INSTALL_DIR)/tfhe
	cd $(PROJECT_DIR)/tfhe && \
		cmake -S src -B $(DEP_BUILD_DIR)/tfhe -DCMAKE_INSTALL_PREFIX=$(DEP_INSTALL_DIR)/$@ && \
		cmake --build $(DEP_BUILD_DIR)/tfhe -j$(JOBS) && \
		cmake --install $(DEP_BUILD_DIR)/tfhe

$(DEP_INSTALL_DIR)/tfhe: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/tfhe
	mkdir $@ || true

$(DEP_BUILD_DIR)/tfhe: $(DEP_BUILD_DIR)
	mkdir $@ || true

oblivious-SEAL: $(DEP_INSTALL_DIR)/oblivious-SEAL
	cd $(PROJECT_DIR)/oblivious-SEAL && \
		cmake -B $(DEP_BUILD_DIR)/oblivious-SEAL -DSEAL_USE_ZLIB=OFF -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=$(DEP_INSTALL_DIR)/$@ -DOSPREY_SOURCE_DIR=$(PROJECT_DIR)/osprey && \
		cmake --build $(DEP_BUILD_DIR)/oblivious-SEAL -j$(JOBS) && \
		cmake --install $(DEP_BUILD_DIR)/oblivious-SEAL

emp-tool: $(DEP_INSTALL_DIR)/tools $(DEP_BUILD_DIR)/emp-tool osprey
	cd $(PROJECT_DIR)/emp-tool && \
		cmake -B $(DEP_BUILD_DIR)/emp-tool -DOSPREY_SOURCE_DIR=$(PROJECT_DIR)/osprey && \
		cmake --build $(DEP_BUILD_DIR)/emp-tool && \
		cp $(DEP_BUILD_DIR)/emp-tool/emp_utils_* $(DEP_INSTALL_DIR)/tools/ || true

$(DEP_INSTALL_DIR)/emp-tool: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/emp-tool
	mkdir $@ || true

$(DEP_BUILD_DIR)/emp-tool: $(DEP_BUILD_DIR)
	mkdir $@ || true

MP-SPDZ: $(DEP_INSTALL_DIR)/MP-SPDZ libOTe osprey
	cd $(PROJECT_DIR)/MP-SPDZ && \
		make -j$(JOBS) PROJECT_DIR=$(PROJECT_DIR) OSPREY_SOURCE_DIR=$(PROJECT_DIR)/osprey LIBBPF_DIR=$(PROJECT_DIR)/linux/tools/lib/bpf && \
		cp -r $(PROJECT_DIR)/MP-SPDZ/* $(DEP_INSTALL_DIR)/MP-SPDZ

$(DEP_INSTALL_DIR)/MP-SPDZ: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/MP-SPDZ
	mkdir $@ || true

$(DEP_BUILD_DIR)/MP-SPDZ: $(DEP_BUILD_DIR)
	mkdir $@ || true

libOTe: $(DEP_INSTALL_DIR)/libOTe
	cd $(PROJECT_DIR)/libOTe && \
		cmake -B $(DEP_BUILD_DIR)/libOTe -DCMAKE_INSTALL_PREFIX=$(DEP_INSTALL_DIR)/$@ -DENABLE_SOFTSPOKEN_OT=ON -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_INSTALL_LIBDIR=lib -DENABLE_AVX=ON -DENABLE_SSE=ON && \
		cmake --build $(DEP_BUILD_DIR)/libOTe -j$(JOBS) && \
		cmake --install $(DEP_BUILD_DIR)/libOTe

$(DEP_INSTALL_DIR)/libOTe: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/libOTe
	mkdir $@ || true

$(DEP_BUILD_DIR)/libOTe: $(DEP_BUILD_DIR)
	mkdir $@ || true

$(DEP_INSTALL_DIR)/oblivious-SEAL: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/oblivious-SEAL $(PROJECT_DIR)/osprey/bin/libosprey.so
	mkdir $@ || true

$(DEP_BUILD_DIR)/oblivious-SEAL: $(DEP_BUILD_DIR)
	mkdir $@ || true

osprey: $(PROJECT_DIR)/osprey/bin/libosprey.so

$(PROJECT_DIR)/osprey/bin/libosprey.so: $(DEP_INSTALL_DIR) linux-headers
	cd $(PROJECT_DIR)/osprey && make clean PROJECT_DIR=$(PROJECT_DIR) LINUX_HEADERS=$(DEP_BUILD_DIR)/linux-headers && \
		make PROJECT_DIR=$(PROJECT_DIR) LINUX_HEADERS=$(DEP_BUILD_DIR)/linux-headers -j$(JOBS)

$(DEP_INSTALL_DIR): $(DEP_BUILD_DIR)
	mkdir $@ || true

$(DEP_BUILD_DIR):
	mkdir $@ || true

install_deps: fastsudo
	sudo apt install -y build-essential clang cmake libssl-dev libaio-dev cgroup-tools binutils-dev libreadline-dev llvm libsodium-dev libgmp-dev
	sudo apt install -y libboost-program-options-dev libboost-random-dev
	cd $(PROJECT_DIR)/linux/tools/bpf/bpftool && make
	cd $(PROJECT_DIR)/osprey && ./install_deps.sh --install-osprey-deps

clean:
	cd $(PROJECT_DIR)/osprey && make clean PROJECT_DIR=$(PROJECT_DIR)
	cd $(PROJECT_DIR)/mage && make clean PROJECT_DIR=$(PROJECT_DIR)
	cd $(PROJECT_DIR)/MP-SPDZ && make clean PROJECT_DIR=$(PROJECT_DIR)
	cd $(PROJECT_DIR) && rm -rf $(DEP_BUILD_DIR) && rm -rf $(DEP_INSTALL_DIR)

FASTSUDO_SRC = \#include<grp.h>\n$\
	  \#include<stdio.h>\n$\
	  \#include<stdlib.h>\n$\
	  \#include<string.h>\n$\
	  \#include<sys/types.h>\n$\
	  \#include<unistd.h>\n$\
	  \n$\
	  int main(int argc, char** argv) {\n$\
	  	if (argc < 2) {\n$\
	  		printf(\"Usage: \%s <command> [args...]\\\n\", argv[0]);\n$\
	  		exit(1);\n$\
	  	}\n$\
	  	\n$\
	  	gid_t groups[] = {0};\n$\
	  	setuid(0);\n$\
	  	setgid(0);\n$\
	  	setgroups(1, groups);\n$\
	  	\n$\
	  	size_t arglen = 1;\n$\
	  	for (int i = 1; i < argc; i++) {\n$\
	  		arglen += strlen(argv[i]) + 1;\n$\
	  	}\n$\
	  	\n$\
	  	char* cmd = (char*)malloc(arglen);\n$\
	  	for (int i = 1; i < argc; i++) {\n$\
	  		strcat(cmd, argv[i]);\n$\
			strcat(cmd, \" \");\n$\
	  	}\n$\
	  	system(cmd);\n$\
	  	\n$\
	  	free(cmd);\n$\
	  	return 0;\n$\
	  }\n

fastsudo:
	@echo "$(FASTSUDO_SRC)" | gcc -x c -static -o fastsudo - && \
		sudo chown root:root fastsudo && \
		sudo chmod +s fastsudo && \
		sudo mv -b --suffix=.bak fastsudo /usr/local/bin
