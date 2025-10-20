.PHONY: clean run sha

CC = mpic++
CCFLAGS = -std=c++17 -O3
BUILD := build
FILES := files
BIN := spellcheck
NUM_NODES := 8

all: $(BUILD)/$(BIN)

$(BUILD)/$(BIN): spellcheck.cc symspell.cc
	$(CC) $(CCFLAGS) $^ -o $@

clean: 
	rm -rf $(BUILD)/*

sha: $(BUILD)/$(BIN)
	time -v mpirun -np $(NUM_NODES) $(BUILD)/$(BIN) $(FILES)/dict626623.txt $(FILES)/words658976.txt | sort -n > misc/out.txt
	cat misc/headings.txt misc/out.txt > results/thing.csv
	rm misc/out.txt
	md5sum results/word_list_misspelled.txt