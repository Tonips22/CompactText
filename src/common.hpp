#pragma once
#include <string>
#include <iostream>
#include <vector>
#include <regex>
#include <filesystem>


using namespace std;

double encode_file_seq (const string& path);
double decode_file_seq (const string& stem);
double encode_file_omp (const string& path);
double decode_file_omp (const string& stem);
int encode_file_mpi (const string& path);
int decode_file_mpi (const string& stem);
void help();

vector<string> tokenize(const string &text) {
    static const regex token_re("[A-Za-zÀ-ÖØ-öø-ÿ0-9'()\\[\\]{}\".,;:!?¿¡—\\-]+");
    vector<string> words;
    for (sregex_iterator it(text.begin(), text.end(), token_re);
         it != sregex_iterator(); ++it) {
        words.emplace_back(it->str());
    }
    return words;
}

string stem_of(const string& path)
{
    namespace fs = filesystem;
    fs::path p(path);
    return (p.parent_path() / p.stem()).string();   // «docs/messi»
}