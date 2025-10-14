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
    // Temp buffer for doing work
    filesize = text_len;

    // Internal data buffer
    data = (char*)calloc(text_len, sizeof(char));
    capitals = (char*)calloc(text_len, sizeof(char));
    memcpy(data, dict_text, (text_len)*sizeof(char));
    memcpy(capitals, dict_text, (text_len)*sizeof(char));

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

    // // Resize scratch buffer as necessary
    // if (s_len+1 > bsize) {
    //     while (bsize < s_len+1) {
    //     bsize *= RESIZE_FACTOR;
    //     buf = (char*)realloc(buf, bsize*sizeof(char*));
    //     }
    // }

    // Create new list for word
    if (map.count(s)) {
        map[s].push_back(s);
    } else {
        std::vector<const char*> new_list = {s};
        map[s] = new_list; 
    }

    // Insert the string with one character removed for every character in the string
    // Make sure we don't insert duplicates e.g. apple -> inserting aple and aple

    if (s_len < 2) return;

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
    if (isupper(s[0])) {
        std::string c = std::string(s, s_len);
        c[0] = tolower(s[0]);
        return (bool)dict.count(s) || (bool)dict.count(c);
    }
    return (bool)dict.count(s);
}

std::vector<const char*> Sym_Spell::candidates(const char* s, size_t s_len) {
    return candidates(s, s_len, nullptr);
}

std::vector<const char*> Sym_Spell::candidates(const char* s, size_t s_len, size_t *out_hash) {
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

    // printf("Checking for %s\n", s);
    // printf("--------------\n");
    // for (const char* word : out) {
    //     printf("%s\n", word);
    // }
    // printf("--------------\n");

    return out;
}

std::vector<const char*> Sym_Spell::candidates(const std::string &s) {
    return candidates(s.c_str(), s.length());
}

void Sym_Spell::remove_char_at(char* buf, const char* s, size_t s_len, size_t i) {
    // copy two halves of string
    for (size_t j=0; j<i; j++) {
        buf[j] = s[j];
    }
    for (size_t j=i; j<s_len; j++) {
        buf[j] = s[j+1];
    }    
    buf[s_len] = '\0';  
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

    if (s2[0] < 'a' && s2[0] > 'z') {
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

    for (size_t i = 1; i <= m; ++i) {
        if (s1[i - 1] == s2[0]) {
            dp[i][1] = dp[i - 1][0];
        } else if (s1[i - 1] >= 'A' && s1[i - 1] <= 'Z' &&
                    s1[i - 1] + 'a' - 'A' == s2[0]) {
            // e.ii
            dp[i][1] = dp[i - 1][0];
        } else {
            dp[i][1] = std::min({dp[i - 1][1] + 1, dp[i][0] + 1, dp[i - 1][0] + 1});
        }
    }
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 2; j <= n; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = std::min({dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + 1});
            }
        }
    }
    return dp[m][n];
}

void Sym_Spell::insert_capital(const char* s, size_t s_len) {
    if (dict.count(s)) return;

    // Insert word into dictionary
    dict.insert(s);
    std::string buf = s;
    buf.erase(0, 1);
    if (map.count(buf)) {
        map[buf].push_back(s);
    } else {
        map[buf] = std::vector<const char*>{s};
    }
}