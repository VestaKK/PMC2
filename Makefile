.PHONY: clean run sha

CC = mpic++
CCFLAGS = -std=c++17 -O3
BUILD := build
FILES := files
BIN := spellcheck
NUM_NODES := 3

all: $(BUILD)/$(BIN)

$(BUILD)/$(BIN): spellcheck.cc symspell.cc
	$(CC) $(CCFLAGS) $^ -o $@

clean: 
	rm -rf $(BUILD)/*

sha: $(BUILD)/$(BIN)
	mpirun -np $(NUM_NODES) $(BUILD)/$(BIN) $(FILES)/dict626623.txt $(FILES)/words658976.txt
	md5sum results/word_list_misspelled.txt