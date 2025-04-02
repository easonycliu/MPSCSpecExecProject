#!/bin/bash

tool=
tool_args=

input_size=
mem_limit=
workload=

batch_size=1

current_time=$(date +%Y%m%d_%H%M%S)
current_date=$(date +%Y%m%d)

project_dir=$(realpath .)
log_dir=${project_dir}/logs/${current_date}/log_${current_time}
playground_dir=${project_dir}/logs/playground

tool_cmd=
log_file=/dev/null

OSPREY=${project_dir}/install/tools/osprey
MAGE=${project_dir}/install/tools/mage
MPSPDZ_UTILS=${project_dir}/install/tools/mpspdz_utils
PLANNER=${project_dir}/install/tools/planner
EXAMPLE_INPUT=${project_dir}/install/tools/example_input

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

sync
echo 3 | tee /proc/sys/vm/drop_caches

log_file=${log_dir}/${workload}.log
mkdir -p ${log_dir}
touch ${log_file}
echo Tool: ${tool} | tee -a ${log_file}
echo Tool args: ${tool_args} | tee -a ${log_file}
echo Input size: ${input_size} | tee -a ${log_file}
echo Batch size: ${batch_size} | tee -a ${log_file}
echo Memory limit: ${mem_limit}M | tee -a ${log_file}

if [ "${tool}" == "osprey" ]; then
	tool_cmd="$OSPREY ${tool_args} --trace-filebase=${workload}_${input_size}_garbler"
elif [ "${tool}" == "baseline" ]; then
	tool_cmd=""
else
	echo "Unknown tool" ${tool}
	exit
fi

if [ "${mem_limit}" != "" ]; then
	cgcreate -g memory:/osprey
	cgset -r memory.high="${mem_limit}M" osprey
	tool_cmd="cgexec -g memory:osprey ${tool_cmd}"
fi

mkdir -p ${playground_dir}
pushd ${playground_dir}

${EXAMPLE_INPUT} ${workload} ${input_size} 1

echo expected output is $(od -An -w -i ${workload}_${input_size}_0.expected | tail -n 2)
${tool_cmd} ${MPSPDZ_UTILS} ${workload} ${input_size} ${workload}_${input_size}_0_garbler.input ${workload}_${input_size}_0_garbler.output | tee -a ${log_file}
echo real output is $(od -An -w -i ${workload}_${input_size}_0_garbler.output | tail -n 2)

popd

if [ "${mem_limit}" != "" ]; then
	cgdelete memory:/osprey
fi

stty sane
echo ""
