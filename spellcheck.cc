#include <cstring>
#include <vector>
#include "symspell.h"
#include "mpi.h"
#include <string.h>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>

void read_partition(
  const char* filename, int rank, int size, 
  char** data, char** begin, size_t* list_size) {

  MPI_File handle;
  int access_mode = MPI_MODE_RDONLY;
  if (MPI_File_open(MPI_COMM_WORLD, filename, access_mode, MPI_INFO_NULL, &handle)) {
      printf("[MPI process %d] Failure in opening the file.\n", rank);
      MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
  }
  // printf("[MPI process %d] File opened successfully.\n", rank);


  MPI_Status status;
  MPI_Offset text_len;

  MPI_File_get_size(handle, &text_len);
  int partition = text_len/size;
  int chunk_size= 2*partition;

  if (rank == size-1) {
    int left_over_bytes = text_len - (size*partition);
    chunk_size += left_over_bytes;
  }

  char* dict_text = (char*)malloc(sizeof(char)*chunk_size);

  if (rank == 0) {
    MPI_File_read_at(handle, 0, dict_text, chunk_size, MPI_BYTE, &status);
  } else {
    MPI_File_read_at(handle, (rank-1)*partition, dict_text, chunk_size, MPI_BYTE, &status);
  }

  if(MPI_File_close(&handle) != MPI_SUCCESS) {
      printf("[MPI process %d] Failure in closing the file.\n", rank);
      MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
  }

  // printf("[MPI process %d] File closed successfully.\n", rank);

  size_t start;
  size_t end;
  if (rank == 0) {
    start = 0;
    end = partition-1;
  } else { 
    start = partition;
    end = chunk_size-1;
  }

  // Find the beginning of the first word in our list
  if (rank != 0) {
    start--;
    while(dict_text[start] != '\n') {
      start--;
    }
    start++;
  }

  // Find the end of the last word in our list
  while(dict_text[end] != '\n') {
    end--;
  }

  // Calculate final total buffer
  *list_size = end - start + 1;
  *begin = &dict_text[start];
  *data = dict_text;
}

Sym_Spell sym_spell_partition(const char* filename, int rank, int size) {
  char* data;
  char* begin;
  size_t list_len;
  read_partition(filename, rank, size, &data, &begin, &list_len);
  Sym_Spell smp = Sym_Spell(begin, list_len);
  free(data);
  return smp;
}

Word_List word_list_partition(const char* filename, int rank, int size) {
  char* data;
  char* begin;
  size_t list_len;
  read_partition(filename, rank, size, &data, &begin, &list_len);

  // Create Word List
  Word_List word_list = {
    .data = (char*)calloc(list_len, sizeof(char)),
    .data_len = list_len,
    .lengths = std::vector<int>(),
  };
  memcpy(word_list.data, begin, (list_len)*sizeof(char));
  free(data);

  size_t str_len = 0;
  for (size_t i=0; i<list_len; i++) {
      // End of a  word
      if (word_list.data[i] == '\n') {
          word_list.data[i] = '\0';
          word_list.lengths.push_back(str_len);
          str_len = 0;
          continue;
      }
      str_len++;
  }

  return word_list;
}

using namespace std::chrono;

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <dictionary> <word_list>" << std::endl;
    return 1;
  }


  auto start = high_resolution_clock::now();  // Start timing

  MPI_Init(&argc, &argv);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  Sym_Spell sym = sym_spell_partition(argv[1], rank, size);
  int count = sym.dict.size();
  int word_count;

  auto setup_time = high_resolution_clock::now();
  {
    auto time = setup_time;
    auto duration = duration_cast<milliseconds>(time - start).count();

    // Convert duration to minutes, seconds, and milliseconds

    auto minutes = duration / 60000;
    auto seconds = (duration % 60000) / 1000;
    auto milliseconds = duration % 1000;
    std::cout << '[' << rank << ']' << " Preprocessing time: " << minutes << " mins " << seconds << " secs "
        << milliseconds << " ms"
        << "; " << duration << std::endl;
  }

  std::cout << '[' << rank << ']' << " Set Size: " << sym.dict.size() <<std::endl;
  std::cout << '[' << rank << ']' << " Map Size: " << sym.map.size() <<std::endl;
  std::cout << '[' << rank << ']' << " File Partion Size: " << 2*sym.filesize <<std::endl;

  if (rank == 0) {
    MPI_Reduce(&count, &word_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    // printf("%d\n", word_count);
  } else {
    MPI_Reduce(&count, NULL, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
  }

  Word_List word_list = word_list_partition(argv[2], rank, size);
  
  int word_list_lengths[size] = {};
  int word_list_counts[size] = {};

  // Amount of bytes in word list array
  int word_list_size = word_list.data_len;

  // Number of word offset entries == number of words
  int word_list_count = word_list.lengths.size();

  MPI_Allgather(&word_list_size, 1, MPI_INT, word_list_lengths, 1, MPI_INT, MPI_COMM_WORLD);
  MPI_Allgather(&word_list_count, 1, MPI_INT, word_list_counts, 1, MPI_INT, MPI_COMM_WORLD);

  // Calculate the max allocation so that we only have to make 1 allocation
  int max_list_length = word_list_size;
  int max_list_count = word_list_count;
  for (int i=0; i<size; i++) {
    max_list_length = std::max(word_list_lengths[i], max_list_length);
    max_list_count = std::max(word_list_counts[i], max_list_count);
  }

  // capturing other people's broadcasted candidate words
  char* other_words = (char*)calloc(max_list_length, sizeof(char));
  int* other_lengths = (int*)calloc(max_list_count, sizeof(int));

  // just to make the coding easier
  char* curr_words;
  int* curr_lengths;

  // do local checks against this node's dictionary, then reduce it into the global word_check
  bool* local_word_check = (bool*)calloc(max_list_count, sizeof(bool));
  bool* global_word_check = (bool*)calloc(max_list_count, sizeof(bool));

  // measuring the size of the potential buffer
  // each word contains "size" integers of byte offsets
  int* local_byte_counts = (int*)malloc((max_list_count*size)*sizeof(int));
  int* global_byte_counts = (int*)malloc((max_list_count*size)*sizeof(int));

  // sendiing and receiving candidate words for each node
  char* send_buffer = (char*)malloc(max_list_length*sizeof(char)); 
  char* recv_buffer = (char*)malloc(max_list_length*sizeof(char)); 

  // Buffer of lines to print
  int* candidate_counts;
  int misspelt_words;
  std::vector<char> lines = std::vector<char>();

  // Fill out lines dictionary
  for (int i=0; i<size; i++) {
    memset(global_byte_counts, 0, (max_list_count*size)*sizeof(int));
    memset(local_byte_counts, 0, (max_list_count*size)*sizeof(int));
    memset(local_word_check, 0, (max_list_count)*sizeof(bool));
    memset(global_word_check, 0, (max_list_count)*sizeof(bool));

    if (rank == i) {
      MPI_Bcast(word_list.data, word_list.data_len, MPI_CHAR, i, MPI_COMM_WORLD);
      MPI_Bcast(&word_list.lengths.front(), word_list.lengths.size(), MPI_INT, i, MPI_COMM_WORLD);
      curr_words = word_list.data;
      curr_lengths = &word_list.lengths.front();
      // int accum = 0;
      // for (int j=0; j<10; j++) {
      //   printf("%d %s\n", i, &curr_words[accum]);
      //   accum += curr_lengths[j] + 1;
      // }
    } else {
      MPI_Bcast(other_words, word_list_lengths[i], MPI_CHAR, i, MPI_COMM_WORLD);
      MPI_Bcast(other_lengths, word_list_counts[i], MPI_INT, i, MPI_COMM_WORLD);
      curr_words = other_words;
      curr_lengths = other_lengths;
    }


    int num_words = word_list_counts[i];

    int accum = 0;
    for (int j=0; j<num_words; j++) {
      local_word_check[j] = sym.check(&curr_words[accum], curr_lengths[j]);
      accum += curr_lengths[j] + 1;
    }

    MPI_Allreduce(local_word_check, global_word_check, num_words, MPI_CXX_BOOL, MPI_LOR, MPI_COMM_WORLD);
    
    // Map stored words to candidate strings
    accum = 0;
    auto map = std::unordered_map<int, std::vector<const char*>>();
    for (int j=0; j<num_words; j++) {

      // Only if the word doesn't exist anywhere

      if (!global_word_check[j]) {
        // Create buffer for candidate strings

        // {words, thing, hi}
        map[j] = sym.candidates(&curr_words[accum], curr_lengths[j]);

        // {words, thing, hi}
        for (const char* c : map[j]) {

          // include the null byte
          // buffer -> "words\0thing\0hi\0"
          int word_len = strlen(c) + 1;

          // rank 3 has 21 bytes to write for this word -> [0, 0, 0, 21]
          local_byte_counts[j*size + rank] += word_len * sizeof(char); 
        } 
      }
      accum += curr_lengths[j] + 1;
    }

    // essentially just gathering the byte counts from each node
    MPI_Allreduce(local_byte_counts, global_byte_counts, size*num_words, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    // count number of misspelt words

    // get the total amount of data to write
    int misspelt_word_count=0;
    int total_write = 0;
    for (int j=0; j<num_words; j++) {
      int accum=0;

      // only write if the word isn't in the dictionary
      if (global_word_check[j]) continue;
      misspelt_word_count++;

      // add byte counts for each node
      for (int k=0; k<size; k++) {
        accum += global_byte_counts[j*size + k];
      }
      // printf("%d\n", accum);
      total_write += accum;
    }


    // printf("%d\n", total_write);

    // reallocate buffers as necessary
    send_buffer = (char*)realloc(send_buffer, total_write*sizeof(char));
    recv_buffer = (char*)realloc(recv_buffer, total_write*sizeof(char));
    memset(send_buffer, 0, total_write*sizeof(char));
    memset(recv_buffer, 0, total_write*sizeof(char));
    
    // Write words for a candidate at the correct dispacement relative to other nodes
    int bytes_written = 0;
    for (int j=0; j<num_words; j++) {
      if (global_word_check[j]) continue;

      for (int k=0; k<size; k++) {
        int bytes_to_write = global_byte_counts[j*size + k];
        if (rank == k && bytes_to_write > 0) {

          int o = bytes_written;
          int accum = 0;
          auto candidates = map[j];
          for (const char* c : candidates) {

            // include null byte
            int num_bytes = sizeof(char)*(strlen(c) + 1);
            memcpy(&send_buffer[o], c, num_bytes);
            o += num_bytes;
            accum += num_bytes;
          }
        }
        bytes_written += bytes_to_write;
      }      
    };

    // Combine the results on the host
    MPI_Reduce(send_buffer, recv_buffer, total_write, MPI_CHAR, MPI_SUM, i, MPI_COMM_WORLD);

    if (rank != i) continue;

    // This is the number of misspelt words in our local candidates list 
    misspelt_words = misspelt_word_count;
    // printf("%d(miss): %d\n", rank, misspelt_word_count);

    candidate_counts = (int*)calloc(misspelt_words, sizeof(int));

    int offset=0;
    accum=0;
    int index_misspelt=0;
    std::vector<std::string> candidates = std::vector<std::string>();
    for (int j=0; j<num_words; j++) {
      int word_len = curr_lengths[j];
      
      if (global_word_check[j]) {
        accum += word_len + 1;
        continue;
      }

      // "word"
      for (int k=0; k<word_len; k++) {
        lines.push_back(curr_words[accum + k]);
      }

      // "word:"
      lines.push_back(':');

      int list_size = 0;
      for (int k=0; k<size; k++) {
        list_size += global_byte_counts[j*size + k];  
      }

      if (list_size == 0) {
        // "word:\n"
        lines.push_back('\n');
        index_misspelt++;
        offset += list_size;
        accum += word_len + 1;
        continue;
      }

      // "list of candidates\0"
      char* o = &recv_buffer[offset];
      int str_len = 0;
      std::string previous = "";
      for (int k=offset; k<offset + list_size; k++) {
        if (recv_buffer[k] == '\0') { 
          candidates.push_back(std::string(o, str_len));
          o += str_len + 1;
          str_len = 0;
          continue;
        }
        str_len++;
      }
 
      lines.push_back(' ');
      std::sort(candidates.begin(), candidates.end());
      int dup_count = 0;
      std::string last = "";
      for (std::string s : candidates) { 
          if (last != s)  {
            lines.insert(lines.end(), s.begin(), s.end());
            lines.push_back(' ');
          } else {
            dup_count++;
          }
          last = s;
      }
      lines.back() = '\n';
      candidate_counts[index_misspelt] = candidates.size() - dup_count;

      // "word: list of candidates\n" 
      index_misspelt++;
      offset += list_size;
      accum += word_len + 1;
      candidates.clear();
    }
  }

  auto parallel_processing_time = high_resolution_clock::now();
  {
    auto time = parallel_processing_time;
    auto duration = duration_cast<milliseconds>(time - setup_time).count();

    // Convert duration to minutes, seconds, and milliseconds

    auto minutes = duration / 60000;
    auto seconds = (duration % 60000) / 1000;
    auto milliseconds = duration % 1000;
    std::cout << '[' << rank << ']' << " Parallel distribution time: " << minutes << " mins " << seconds << " secs "
        << milliseconds << " ms"
        << "; " << duration << std::endl;
  }

  const char* out_filename = "results/word_list_misspelled.txt";

  int lines_size = lines.size();
  int line_size_at_rank[size];
  int misspelt_words_at_rank[size];

  int max_line_size=0; 
  int max_words=0;

  MPI_Gather(&lines_size, 1, MPI_INT, &line_size_at_rank, 1, MPI_INT, 0, MPI_COMM_WORLD); 
  MPI_Gather(&misspelt_words, 1, MPI_INT, &misspelt_words_at_rank, 1, MPI_INT, 0, MPI_COMM_WORLD);

  for (int i=0; i<size; i++) {
    max_line_size = std::max(max_line_size, line_size_at_rank[i]);
    max_words = std::max(max_words, misspelt_words_at_rank[i]);
  }

  char* file_lines;
  int* count_words;

  if (rank == 0) {
    file_lines = (char*) calloc(max_line_size, sizeof(char));
    count_words = (int*) calloc(max_words, sizeof(int));
  } else {
    file_lines = nullptr;
    count_words = nullptr;
  }

  std::vector<Line> output_lines = std::vector<Line>();
  if (rank == 0) {
    int last = 0;
    int index=0;
    for(int curr=0; curr < (int)lines.size(); curr++) {
      if (lines[curr] == '\n') {  
        Line line = {
          .word_count = candidate_counts[index],
          .line = std::string(&lines[last], curr - last + 1),
        };
        output_lines.push_back(line);
        last = curr + 1;
        index++;
      }
    }
    for (int i=1; i<size; i++) {
        MPI_Recv(file_lines, line_size_at_rank[i], MPI_CHAR, i, 0, MPI_COMM_WORLD, NULL);
        MPI_Recv(count_words, misspelt_words_at_rank[i], MPI_INT, i, 0, MPI_COMM_WORLD, NULL);
        int last = 0;
        int index=0;
        for(int curr=0; curr <= line_size_at_rank[i]; curr++) {
          if (file_lines[curr] == '\n') {  
            Line line = {
              .word_count = count_words[index],
              .line = std::string(&file_lines[last], curr - last + 1),
            };
            output_lines.push_back(line);
            last = curr + 1;
            index++;
          }
        }
    }

    std::stable_sort(output_lines.begin(), output_lines.end(), [](const Line &lhs, const Line &rhs) {
      return lhs.word_count < rhs.word_count;
    });
    
    std::ofstream out("results/word_list_misspelled.txt");
    for (Line line : output_lines) {
		if (line.line == "\n") continue; // dumb bug ig
      out << line.line;
    }
    out.close();

  } else {
    MPI_Send(&lines.front(), lines.size(), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
    MPI_Send(candidate_counts, misspelt_words, MPI_INT, 0, 0, MPI_COMM_WORLD);
  }

  // Gather time
  auto gather_time = high_resolution_clock::now();
  {
    auto time = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(gather_time - parallel_processing_time).count();

    // Convert duration to minutes, seconds, and milliseconds

    auto minutes = duration / 60000;
    auto seconds = (duration % 60000) / 1000;
    auto milliseconds = duration % 1000;
    std::cout << '[' << rank << ']' << " Gather time: " << minutes << " mins " << seconds << " secs "
        << milliseconds << " ms"
        << "; " << duration << std::endl;
  }

  // Show wall time
  {
    auto time = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(time - start).count();

    // Convert duration to minutes, seconds, and milliseconds

    auto minutes = duration / 60000;
    auto seconds = (duration % 60000) / 1000;
    auto milliseconds = duration % 1000;
    std::cout << '[' << rank << ']' << " Total time: " << minutes << " mins " << seconds << " secs "
        << milliseconds << " ms"
        << "; " << duration << std::endl;
  }

  free(file_lines);
  free(count_words);
  free(candidate_counts);
  free(send_buffer);
  free(local_word_check);
  free(global_word_check);
  free(other_words);
  free(other_lengths);
  free(global_byte_counts);
  free(local_byte_counts);
  MPI_Finalize();
}
