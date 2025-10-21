#!/bin/bash

echo "=========================="
if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ]; then
    echo -e "\nUsage: sbatch $0 <dict_filename> <word_filename> <requested_nodes>\n"
    exit 1
else
    dict=$1
    file=$2
    MAX_NODES=$3
    echo "[*] Using provided files: $dict $file"
    echo "[*] Using provided nodes: $MAX_NODES"
fi

dict_base=$(basename "$dict")
file_base=$(basename "$file")
dict_raw="${dict_base%.*}"
file_raw="${file_base%.*}"
temp_file="temp.txt"

content=""
for ((i=1; i<=$MAX_NODES; i*=2)); do 
    out_file="results/$dict_raw.$file_raw.$i.csv"
	std_out="$((/usr/bin/time -v mpirun -np $i build/spellcheck $dict $file | sort -n > misc/$temp_file) 2>&1)"
    cat misc/headings.txt misc/$temp_file > $out_file
    rm misc/$temp_file
    peak_mem=$( echo "$std_out" | grep "Maximum resident set size" | awk '{print $NF}' )
    content+="$i "
    content+="$(md5sum results/word_list_misspelled.txt | tr -d '\n')"
    content+="$(echo " $peak_mem KB\n")"
done

(printf "$content") > "results/$dict_raw.$file_raw.hash.txt"