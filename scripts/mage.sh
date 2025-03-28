#!/bin/bash

tool=
tool_args=

input_size=
mem_limit=
workload=

batch_size=1

current_time=$(date +%Y%m%d_%H%M%S)
current_date=$(date +%Y%m%d)

project_dir=$(git rev-parse --show-toplevel)
log_dir=${project_dir}/logs/20250327/log_20250327_155316
playground_dir=${log_dir}

tool_cmd=
log_file=/dev/null

OSPREY=${project_dir}/install/tools/osprey
MAGE=${project_dir}/install/tools/mage
PLANNER=${project_dir}/install/tools/planner
EXAMPLE_INPUT=${project_dir}/install/tools/example_input

SUDO=
if [ $(id -u) -ne 0 ]; then
	SUDO=sudo
fi

for flag in "$@"; do
	case $flag in
		--tool=*)
			tool=$(echo $flag | awk -F = '{print $2}')
			;;
		--tool_args=*)
			value_index=$(( $(echo $flag | grep -bo = | awk -F : '{print $1}' | head -n 1) + 1 ))
			tool_args=$(echo ${flag:$value_index})
			;;
		--input_size=*)
			input_size=$(echo $flag | awk -F = '{print $2}')
			;;
		--mem_limit=*)
			mem_limit=$(echo $flag | awk -F = '{print $2}')
			;;
		--workload=*)
			workload=$(echo $flag | awk -F = '{print $2}')
			;;
		*)
			echo "Unknown command-line flag" $flag
	esac
done

$SUDO sync
echo 3 | $SUDO tee /proc/sys/vm/drop_caches

log_file=${log_dir}/${workload}.log
mkdir -p ${log_dir}
touch ${log_file}
echo Tool: ${tool} | tee -a ${log_file}
echo Tool args: ${tool_args} | tee -a ${log_file}
echo Input size: ${input_size} | tee -a ${log_file}
echo Batch size: ${batch_size} | tee -a ${log_file}
echo Memory limit: ${mem_limit}M | tee -a ${log_file}

page_shift=21
num_pages=1048576
if [ "${tool}" == "osprey" ]; then
	tool_cmd="$OSPREY ${tool_args}"
elif [ "${tool}" == "mage" ]; then
	tool_cmd=""
	if [ "${mem_limit}" == "" ]; then
		echo "Error: Memory limit is required for MAGE"
		exit
	fi
	num_pages=$(( ( ( mem_limit << 20 ) >> page_shift ) - 48 ))
elif [ "${tool}" == "baseline" ]; then
	tool_cmd=""
else
	echo "Unknown tool" ${tool}
	exit
fi

if [ "${mem_limit}" != "" ]; then
	$SUDO cgcreate -g memory:/osprey
	$SUDO cgset -r memory.high="${mem_limit}M" osprey
	tool_cmd="cgexec -g memory:osprey ${tool_cmd}"
fi

mkdir -p ${playground_dir}
pushd ${playground_dir}

cat <<EOF >config.yaml
page_shift: ${page_shift}
num_pages: ${num_pages}
prefetch_buffer_size: 16
prefetch_lookahead: 100

parties:
    # Evaluator
    - workers:
        - internal_host: localhost
          internal_port: 56000
          external_host: localhost
          external_port: 54323
          storage_path: evaluator_swapfile_1

    # Garbler
    - workers:
        - internal_host: localhost
          internal_port: 50000
          external_host: localhost
          external_port: 54322
          storage_path: garbler_swapfile_1
EOF

${EXAMPLE_INPUT} ${workload} ${input_size} 1

echo expected output is $(od -An -w -i ${workload}_${input_size}_0.expected | tail -n 2)

$SUDO $PLANNER ${workload} halfgates config.yaml 0 0 ${input_size} | tee -a ${log_file}

if [ "${tool}" == "osprey" -o "${tool}" == "baseline" ]; then
	${SUDO} ${tool_cmd} --trace-filebase=${workload}_${input_size}_evaluator ${MAGE} halfgates config.yaml 0 0 ${workload}_${input_size} &
	sleep 1
	${SUDO} ${tool_cmd} --trace-filebase=${workload}_${input_size}_garbler ${MAGE} halfgates config.yaml 1 0 ${workload}_${input_size}
elif [ "${tool}" == "mage" ]; then
	$SUDO ${MAGE} halfgates config.yaml 0 0 ${workload}_${input_size} | tee -a ${log_file} &
	sleep 1
	$SUDO ${MAGE} halfgates config.yaml 1 0 ${workload}_${input_size} | tee -a ${log_file}
fi

$SUDO mv ${workload}_${input_size}_0.output ${workload}_${input_size}_0_garbler.output

echo real output is $(od -An -w -i ${workload}_${input_size}_0_garbler.output | tail -n 2)

popd

if [ "${mem_limit}" != "" ]; then
	$SUDO cgdelete memory:/osprey
fi

stty sane
echo ""
