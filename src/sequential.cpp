// sequential.cpp
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <chrono>
#include "common.hpp"

using namespace std;
using uint32 = uint32_t;

// Divide el texto en tokens (secuencias de no-whitespace) y separadores (whitespace)
static void split_tokens_and_separators(const string& txt, vector<string>& tokens, vector<string>& seps) {
    size_t n = txt.size();
    size_t pos = 0;
    while (pos < n) {
        // Token: no-whitespace
        size_t start = pos;
        while (pos < n && !std::isspace(static_cast<unsigned char>(txt[pos]))) {
            ++pos;
        }
        tokens.emplace_back(txt.substr(start, pos - start));
        // Separador: whitespace
        start = pos;
        while (pos < n && std::isspace(static_cast<unsigned char>(txt[pos]))) {
            ++pos;
        }
        seps.emplace_back(txt.substr(start, pos - start));
    }
}

// --- ENCODE secuencial con separadores exactos ---
double encode_file_seq(const string& path) {
    string base = stem_of(path);
    auto t0 = std::chrono::steady_clock::now();

    std::ifstream ifs(path);
    if (!ifs) {
        std::cerr << "Cannot open " << path << "\n";
        return 0.0;
    }
    string text((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    // Tokenización
    vector<string> tokens, seps;
    tokens.reserve(1024);
    seps.reserve(1024);
    split_tokens_and_separators(text, tokens, seps);
    uint32 count = static_cast<uint32>(tokens.size());

    // Construir diccionario de tokens
    unordered_map<string, uint32> dict;
    dict.reserve(count / 2);
    vector<uint32> ids(count);
    uint32 next_id = 1;
    for (uint32 i = 0; i < count; ++i) {
        const string& w = tokens[i];
        auto it = dict.find(w);
        if (it == dict.end()) {
            dict[w] = next_id;
            ids[i] = next_id++;
        } else {
            ids[i] = it->second;
        }
    }

    // Escribir vocab.bin
    {
        std::ofstream vout(base + "_vocab.bin", std::ios::binary);
        uint32 vs = static_cast<uint32>(dict.size());
        vout.write(reinterpret_cast<const char*>(&vs), sizeof(vs));
        for (auto& kv : dict) {
            uint32 len = static_cast<uint32>(kv.first.size());
            vout.write(reinterpret_cast<const char*>(&kv.second), sizeof(kv.second));
            vout.write(reinterpret_cast<const char*>(&len), sizeof(len));
            vout.write(kv.first.data(), len);
        }
    }

    // Escribir texto.bin: [count][ (ID, sep_len, sep_bytes)* ]
    {
        std::ofstream tout(base + "_texto.bin", std::ios::binary);
        tout.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (uint32 i = 0; i < count; ++i) {
            uint32 id = ids[i];
            tout.write(reinterpret_cast<const char*>(&id), sizeof(id));
            uint32 slen = static_cast<uint32>(seps[i].size());
            tout.write(reinterpret_cast<const char*>(&slen), sizeof(slen));
            tout.write(seps[i].data(), slen);
        }
    }

    double secs = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0).count();
    std::cout << "Encoded " << count << " tokens. Unique=" << dict.size()
              << " | time=" << secs << " s\n";
    return secs;
}

// --- DECODE secuencial recobrando separadores exactos ---
double decode_file_seq(const string& stem) {
    auto t0 = std::chrono::steady_clock::now();

    // Leer vocab.bin
    std::ifstream vfin(stem + "_vocab.bin", std::ios::binary);
    uint32 vs;
    vfin.read(reinterpret_cast<char*>(&vs), sizeof(vs));
    vector<string> vocab(vs + 1);
    for (uint32 i = 0; i < vs; ++i) {
        uint32 id, len;
        vfin.read(reinterpret_cast<char*>(&id), sizeof(id));
        vfin.read(reinterpret_cast<char*>(&len), sizeof(len));
        string w(len, '\0');
        vfin.read(w.data(), len);
        vocab[id] = std::move(w);
    }

    // Leer texto.bin y reconstruir
    std::ifstream fin(stem + "_texto.bin", std::ios::binary);
    uint32 count;
    fin.read(reinterpret_cast<char*>(&count), sizeof(count));
    std::ofstream out(stem + "_recon.txt");
    for (uint32 i = 0; i < count; ++i) {
        uint32 id, slen;
        fin.read(reinterpret_cast<char*>(&id), sizeof(id));
        fin.read(reinterpret_cast<char*>(&slen), sizeof(slen));
        string sep(slen, '\0');
        fin.read(sep.data(), slen);
        out << vocab[id] << sep;
    }

    double secs = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0).count();
    std::cout << "Decoded " << count << " tokens into " << stem << "_recon.txt"
              << " | time=" << secs << " s\n";
    return secs;
}

// --- HELP y MAIN sin cambios ---
void help() {
    std::cout << "\nUsage:\n"
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
        std::cout << "=== SEQ encode total time: " << secs << " s ===\n";
    } else if (mode == "decode") {
        for (int i = 2; i < argc; ++i)
            secs += decode_file_seq(argv[i]);
        std::cout << "=== SEQ decode total time: " << secs << " s ===\n";
    } else help();
    return 0;
}
