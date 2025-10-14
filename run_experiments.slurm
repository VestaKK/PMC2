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
    echo "[*] Using provided nodes: $MAX_NODES"
fi

echo "---------------------------"
echo "---------------------------"
srun --ntasks=1 --nodes=1 ./build/spellcheck $dict $file 
md5sum ./results/word_list_misspelled.txt
echo "---------------------------"
echo "---------------------------"
srun --ntasks=2 --nodes=2 ./build/spellcheck $dict $file
md5sum ./results/word_list_misspelled.txt
echo "---------------------------"
echo "---------------------------"
srun --ntasks=3 --nodes=3 ./build/spellcheck $dict $file
md5sum ./results/word_list_misspelled.txt
echo "---------------------------"
echo "---------------------------"
srun --ntasks=4 --nodes=4 ./build/spellcheck $dict $file
md5sum ./results/word_list_misspelled.txt
echo "---------------------------"
echo "---------------------------"
srun --ntasks=5 --nodes=5 ./build/spellcheck $dict $file
md5sum ./results/word_list_misspelled.txt
echo "---------------------------"
echo "---------------------------"
srun --ntasks=6 --nodes=6 ./build/spellcheck $dict $file
md5sum ./results/word_list_misspelled.txt
echo "---------------------------"
echo "---------------------------"
srun --ntasks=7 --nodes=7 ./build/spellcheck $dict $file
md5sum ./results/word_list_misspelled.txt
echo "---------------------------"
echo "---------------------------"
srun --ntasks=8 --nodes=8 ./build/spellcheck $dict $file
md5sum ./results/word_list_misspelled.txt
echo "---------------------------"
echo "---------------------------"
