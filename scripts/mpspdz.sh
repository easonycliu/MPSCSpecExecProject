#!/bin/bash

tool=
tool_args=

input_size=
mem_limit=
workload=
thread_num=1

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
MPSPDZ_UTILS=${project_dir}/install/tools/pmpspdz_utils
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
		--thread_num=*)
			thread_num=$(echo $flag | awk -F = '{print $2}')
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
echo Thread number: ${thread_num} | tee -a ${log_file}

if [ "${tool}" == "osprey" ]; then
	tool_cmd="$OSPREY ${tool_args} --trace-filebase=${workload}_${input_size}_garbler"
elif [ "${tool}" == "baseline" ]; then
	tool_cmd=""
else
	echo "Unknown tool" ${tool}
	exit
fi

rss_log_file=${log_dir}/${workload}_${input_size}.rss
target_name="pmpspdz_utils"

touch ${rss_log_file}
echo "Timestamp,            RSS (MB)" > "$rss_log_file"

monitor_rss() {
	# Wait for the process to start
	while true; do
		PID=$(ps -a | grep "${target_name}\$" | sort | tail -n 1 | awk '{print $1}')
		echo "PID: $PID"
		if [ -n "$PID" ]; then
			echo "Monitoring process $target_name with PID $PID"
			break
		fi
		sleep 1
	done
	
	# Start monitoring RSS
	while kill -0 $PID 2>/dev/null; do
		RSS=$(ps -o rss= -p $PID 2>/dev/null)
		if [ -z "$RSS" ]; then
			echo "Process $PID has terminated."
			break
		fi
		TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")
		echo "$TIMESTAMP,  $(( RSS / 1024 ))" >> "$rss_log_file"
		sleep 1  # Adjust interval as needed
	done
}

if [ "${mem_limit}" != "" ]; then
	cgcreate -g memory:/osprey
	cgset -r memory.high="${mem_limit}M" osprey
	tool_cmd="cgexec -g memory:osprey ${tool_cmd}"
fi

mkdir -p ${playground_dir}
pushd ${playground_dir}

${EXAMPLE_INPUT} ${workload} ${input_size} 1

swapon /dev/sdb2
monitor_rss &

echo expected output is $(od -An -w -i ${workload}_${input_size}_0.expected | tail -n 2)
${tool_cmd} ${MPSPDZ_UTILS} ${workload} ${input_size} ${thread_num} ${workload}_${input_size}_0_garbler.input ${workload}_${input_size}_0_garbler.output | tee -a ${log_file}
echo real output is $(od -An -w -i ${workload}_${input_size}_0_garbler.output | tail -n 2)

popd

if [ "${mem_limit}" != "" ]; then
	cgdelete memory:/osprey
fi

stty sane
echo ""
