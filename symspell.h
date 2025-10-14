#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <cstddef>

#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME 16777619u
#define SIZE_TEMP 5
#define RESIZE_FACTOR 3

size_t fnv_hash(size_t prev_hash, char const* letter);

struct String_Hasher {
  size_t operator()(const std::string& s) const {
    return fnv_hash(FNV_OFFSET_BASIS, s.c_str());
  }
};

struct Line {
  int word_count;
  std::string line;
};

struct Sym_Spell {
  std::unordered_set<std::string> dict;
  std::unordered_map<std::string, std::vector<const char*>, String_Hasher> map;
  char* data;
  char* capitals;
  size_t filesize;

  // Spec Change

  Sym_Spell(const char* dict_text, size_t text_len);
  ~Sym_Spell();
  void insert(const char* s, size_t s_len);
  bool check(const char* s, size_t s_len);
  std::vector<const char*> candidates(const char* s, size_t s_len);
  std::vector<const char*> candidates(const std::string &s);
  std::vector<const char*> candidates(const char* s, size_t s_len, size_t *out_hash);

  private:
    void remove_char_at(char* buf, const char* s, size_t s_len, size_t i);
    size_t edit_distance(const std::string& s1, const std::string& s2);
    void insert_capital(const char*s, size_t s_len);
};

struct Word_List {
  char* data;
  size_t data_len;
  std::vector<int> lengths;
};