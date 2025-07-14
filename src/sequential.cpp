#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <regex>
#include <cstdint>
#include <iterator>
#include <chrono>
#include "common.hpp"

using namespace std;
using uint32 = uint32_t;

double encode_file_seq (const string& path){
    string base = stem_of(path);  

    auto t0 = chrono::steady_clock::now();

    ifstream ifs(path);
    if (!ifs) {
        cerr << "Cannot open " << path << "\n";
        return 1;
    }
    string text((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    auto words = tokenize(text);

    unordered_map<string, uint32> dict;
    vector<uint32> ids;
    ids.reserve(words.size());
    uint32 next_id = 1;

    for (const auto &w : words) {
        auto it = dict.find(w);
        if (it == dict.end()) {
            dict[w] = next_id;
            ids.push_back(next_id);
            ++next_id;
        } else {
            ids.push_back(it->second);
        }
    }

    // --- Escribir vocab.bin ---
    {
        ofstream out(base + "_vocab.bin", ios::binary);
        uint32 size = dict.size();
        out.write(reinterpret_cast<const char *>(&size), sizeof(size));
        for (const auto &[word, id] : dict) {
            uint32 len = static_cast<uint32>(word.size());
            out.write(reinterpret_cast<const char *>(&id), sizeof(id));
            out.write(reinterpret_cast<const char *>(&len), sizeof(len));
            out.write(word.data(), len);
        }
    }

    // --- Escribir texto.bin ---
    {
        ofstream out(base + "_texto.bin", ios::binary);
        uint32 count = ids.size();
        out.write(reinterpret_cast<const char *>(&count), sizeof(count));
        out.write(reinterpret_cast<const char *>(ids.data()), sizeof(uint32) * count);
    }

    double secs = chrono::duration<double>(
                      chrono::steady_clock::now() - t0).count();

    cout << "Encoded " << words.size()
         << " words. Unique=" << dict.size()
         << " | time = " << secs << "s\n";
    return secs;
}

// ------------------------- DECODE -----------------------------
// Reconstruye el texto original usando vocab.bin y texto.bin
double decode_file_seq (const string& stem) {

    auto t0 = chrono::steady_clock::now();

    // Leer vocab.bin
    ifstream vocab(stem + "_vocab.bin", ios::binary);
    if (!vocab) {
        cerr << "vocab.bin not found\n";
        return 1;
    }
    uint32 vocab_size;
    vocab.read(reinterpret_cast<char *>(&vocab_size), sizeof(vocab_size));
    vector<string> vocab_vec(vocab_size + 1); // IDs comienzan en 1
    for (uint32 i = 0; i < vocab_size; ++i) {
        uint32 id, len;
        vocab.read(reinterpret_cast<char *>(&id), sizeof(id));
        vocab.read(reinterpret_cast<char *>(&len), sizeof(len));
        string w(len, '\0');
        vocab.read(w.data(), len);
        vocab_vec[id] = move(w);
    }

    // Leer texto.bin
    ifstream textbin(stem + "_texto.bin", ios::binary);
    if (!textbin) {
        cerr << "texto.bin not found\n";
        return 1;
    }
    uint32 count;
    textbin.read(reinterpret_cast<char *>(&count), sizeof(count));
    vector<uint32> ids(count);
    textbin.read(reinterpret_cast<char *>(ids.data()), sizeof(uint32) * count);

    // Escribir texto reconstruido
    ofstream out(stem + "_recon.txt");
    for (size_t i = 0; i < ids.size(); ++i) {
        out << vocab_vec[ids[i]];
        if (i + 1 < ids.size())
            out << ' ';
    }

    double secs = chrono::duration<double>(
                      chrono::steady_clock::now() - t0).count();

    cout << "Decoded " << ids.size()<< " words into " << stem
         << " | time=" << secs << " s\n";
    return secs;
}

void help() {
    cout << "\nUsage:\n"
              << "  compacttext_seq encode <input1.txt> <input2.txt> ...\n"
              << "  compacttext_seq decode <output1> <output2> ...\n\n";
}



int main(int argc, char* argv[]) {

    if (argc < 3) { help(); return 1; }
    string mode = argv[1];
    double secs = 0.0;

    if (mode == "encode") {
        for (int i = 2; i < argc; ++i)
            secs += encode_file_seq(argv[i]);
        cout << "=== SEQ encode total time: " << secs << " s ===\n";
    } else if (mode == "decode") {
        for (int i = 2; i < argc; ++i)
            secs += decode_file_seq(argv[i]);
        cout << "=== SEQ decode total time: " << secs << " s ===\n";
    } else help();

    return 0;
}