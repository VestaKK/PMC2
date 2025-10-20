#!/usr/bin/bash

#SBATCH --job-name=MTP_UWE
#SBATCH --time 00:03:00
#SBATCH --partition=sapphire

#SBATCH --nodes=8
#SBATCH --ntasks=8
#SBATCH --tasks-per-node=1
#SBATCH --cpus-per-task=1

echo "=========================="
if [ -z "$1" ] || [ -z "$2" ]; then
    echo -e "\nUsage: sbatch $0 <dict_filename> <word_filename> <requested_threads>\n"
    exit 1
else
    dict=$1
    file=$2
    echo "[*] Using provided files: $dict $file "
fi