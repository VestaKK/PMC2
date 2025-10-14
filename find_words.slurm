#!/usr/bin/bash

#SBATCH --job-name=MTP_UWE
#SBATCH --time 00:30:00
#SBATCH --partition=sapphire

#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --tasks-per-node=1
#SBATCH --cpus-per-task=16

#### You only need to edit the above redacted lines to use this slurm file ####
#### However, feel free to use your own slurm file if you prefer ##############

echo "=========================="
if [ -z "$1" ] || [ -z "$2" ]; then
    echo -e "\nUsage: sbatch $0 <test_filename> <requested_threads>\n"
    exit 1
else
    file=$1
    MAX_THREADS=$2
    echo "[*] Using provided file: $file"
    echo "[*] Using provided threads: $MAX_THREADS"
fi
echo "---------------------------"

file_basename=$(basename "$file")
res_path="results"

if [ ! -d "$res_path" ]; then
    mkdir "$res_path"
fi


content=''
for i in $(seq 1 $MAX_THREADS)
do
    out_arg="$file_basename.parallel.$i"
    out_file="$res_path/$out_arg.time"

    stdout=$(OMP_NUM_THREADS=$i /usr/bin/time -v ./build/run $file $out_arg 2>&1)
    peak_mem=$( echo "$stdout" | grep "Maximum resident set size" | awk '{print $NF}' )
    echo -e "$stdout"
    echo "---------------------------"
    line=$(tail -n 1 $out_file | tr -d '\n')
    content="$content$line, $peak_mem, sapphire\n"
done

out_first_file="results/$file_basename.parallel.1.time"
header_first_file=$(head -n 1 $out_first_file | tr -d '\n' )
header="$header_first_file, peak_memory, partition\n"
out_content="$header$content"

final_results="$res_path/experiment.parallel.${MAX_THREADS}threads.$file_basename.csv"
echo -e "$out_content" | head -n -1 > "$final_results"

echo -e "\n[*] Experiment completed. Results saved in $final_results\n"
echo -e "$out_content"
echo "=========================="
