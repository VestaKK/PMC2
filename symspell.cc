#include <stdio.h>
#include <assert.h>
#include <algorithm>
#include "symspell.h"
#include <cstring>
#include <iostream>

size_t fnv_hash(size_t prev_hash, char const* letter) {
  while (*letter) {
    prev_hash ^= *letter++;
    prev_hash *= FNV_PRIME;
  }
  return prev_hash % (UINT64_MAX / 2);
}

Sym_Spell::Sym_Spell(const char* dict_text, size_t text_len) {

    std::string s = std::string(dict_text, text_len);

    // Symspell objects
    dict = std::unordered_set<std::string>();
    map = std::unordered_map<std::string, std::vector<const char*>, String_Hasher>();
    filesize = text_len;

    // Internal data buffer
    data = (char*)calloc(text_len, sizeof(char));
    capitals = (char*)calloc(text_len, sizeof(char));
    memcpy(data, dict_text, (text_len)*sizeof(char));
    memcpy(capitals, data, (text_len)*sizeof(char));

    const char* c = data;
    char* d = capitals;
    size_t str_len = 0;
    for (size_t i=0; i<text_len; i++) {

        // End of a  word
        if (data[i] == '\n') {
            data[i] = '\0';
            capitals[i] = '\0';
            insert(c, str_len);

            // if (c[0] == 'a') {
            //     printf("%s %lu\n", c, str_len);
            // }

            if (islower(c[0])) {
                d[0] = toupper(c[0]);
                insert(d, str_len);
            }

            // shift pointer forward
            c += str_len + 1;
            d += str_len + 1;
            str_len = 0;
            continue;
        }
        str_len++;
    }
}

Sym_Spell::~Sym_Spell() {
    free(capitals);
    free(data);
}

void Sym_Spell::insert(const char* s, size_t s_len) {

    // std::string str =std::string(s, s_len);

    // if (str == "appyling") {
    //     std::cout << str << std::endl;
    // }
    if (dict.count(s)) return;

    // Insert word into dictionary
    dict.insert(s);

    // Create new list for word
    if (map.count(s)) {
        map[s].push_back(s);
    } else {
        std::vector<const char*> new_list = {s};
        map[s] = new_list; 
    }

    // Single character words don't need to be considered here
    if (s_len < 2) return;

    // Insert the string with one character removed for every character in the string
    // Make sure we don't insert duplicates e.g. apple -> inserting aple and aple
    std::string buf = std::string(s, s_len);
    char last = '\0';
    for (size_t i=0; i<s_len; i++) {
        if (last == s[i]) continue;
        buf.erase(i, 1);
        if (map.count(buf)) {
            map[buf].push_back(s);
        } else {
            std::vector<const char*> new_list = {s};
            map[buf] = new_list;
        }
        last = s[i];
        buf = s;
    }
}

bool Sym_Spell::check(const char* s, size_t s_len) {
    return (bool)dict.count(s);
}


std::vector<const char*> Sym_Spell::candidates(const char* s, size_t s_len) {
    assert(!check(s, s_len));

    auto out = std::vector<const char*>();

    // Check original word

    if (map.count(s)) {
        for (const char* word: map[s]) {
            out.push_back(word);
        }
    }

    char last = '\0';

    // Check word with deletions

    if (s_len < 2) return out;

    for (size_t i=0; i<s_len; i++) {

        // Deleting same character as the previous loop
        if (last == s[i]) continue;

        std::string buf = s;
        buf.erase(i, 1);

        // No potential mispelling here
        if (!map.count(buf)) {
            last = s[i];
            continue;
        }

        // Sort out candidate words
        for (const char* word : map[buf]) {
            if (edit_distance(s, word) != 1) continue; 
            out.push_back(word);
        }

        last = s[i];
        buf = s;
    }
    // Sort words in the list
    // std::sort(out.begin(), out.end(), [](const char* &lhs, const char* &rhs){
    //     return strcmp(lhs, rhs) <= 0;
    // });
    return out;
}

std::vector<const char*> Sym_Spell::candidates(const std::string &s) {
    return candidates(s.c_str(), s.length());
}

// copied from skeleton
size_t Sym_Spell::edit_distance(const std::string& s1, const std::string& s2) {
  const size_t m = s1.size();
  const size_t n = s2.size();

    thread_local std::vector<std::vector<size_t>> dp;

    // Resize the dp table if necessary
    if (dp.size() < m + 1) {
        dp.resize(m + 1);
    }
    for (size_t i = 0; i <= m; ++i) {
        if (dp[i].size() < n + 1) {
        dp[i].resize(n + 1);
        }
    }

    for (size_t i = 0; i <= m; ++i) {
        dp[i][0] = i;
    }
    for (size_t j = 0; j <= n; ++j) {
        dp[0][j] = j;
    }

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
        if (s1[i - 1] == s2[j - 1]) {
            dp[i][j] = dp[i - 1][j - 1];
        } else {
            dp[i][j] = std::min(
                {dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + 1});
        }
        }
    }
    return dp[m][n];
}