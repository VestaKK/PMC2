#!/usr/bin/bash

#SBATCH --job-name=MTP_UWE
#SBATCH --time 00:05:00
#SBATCH --partition=sapphire

#SBATCH --nodes=8
#SBATCH --ntasks=64
#SBATCH --ntasks-per-node=8
#SBATCH --cpus-per-task=1

module load OpenMPI/4.1.4

echo "=========================="
if [ -z "$1" ]; then
    echo -e "\nUsage: sbatch $0 <requested_nodes>\n"
    exit 1
else
    MAX_NODES=$1
    echo "[*] Using provided nodes: $MAX_NODES"
fi

temp_file="temp.txt"

for dict in ./files/dict/*.txt; do
    for file in ./files/words/*.txt; do
        dict_base=$(basename "$dict")
        file_base=$(basename "$file")
        dict_raw="${dict_base%.*}"
        file_raw="${file_base%.*}"
        content=""
        for ((i=1; i<=$MAX_NODES; i*=2)); do 
            out_file="results/$dict_raw.$file_raw.$i.csv"
	        std_out="$((/usr/bin/time -v srun --ntasks $i --cpus-per-task=1 build/spellcheck $dict $file | sort -n > misc/$temp_file) 2>&1)"
            cat misc/headings.txt misc/$temp_file > $out_file
            rm misc/$temp_file
            peak_mem=$( echo "$std_out" | grep "Maximum resident set size" | awk '{print $NF}' )
            content+="$i "
            content+="$(md5sum results/word_list_misspelled.txt | tr -d '\n')"
            content+="$(echo " $peak_mem KB\n")"
        done
        (printf "$content") > "results/$dict_raw.$file_raw.hash.txt"
        echo "Done with $dict_raw $file_raw"
    done
done
