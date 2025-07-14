/*****************************************************************
 CompactText — versión OpenMP  
 Multi-archivo + cronómetro + separadores exactos
*****************************************************************/
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <filesystem>
#include <chrono>
#include <omp.h>
#include "common.hpp"

using namespace std;
using uint32 = uint32_t;
namespace fs = filesystem;

// ----------------------------------------------------------------
// Divide el texto en tokens (secuencias de no-whitespace) y 
// separadores (espacios, tabs, saltos de línea) EXACTOS
static void split_tokens_and_separators(const string& txt,
                                        vector<string>& tokens,
                                        vector<string>& seps) {
    size_t n = txt.size(), pos = 0;
    while (pos < n) {
        // 1) Token: secuencia de _no_-whitespace
        size_t start = pos;
        while (pos < n && !isspace((unsigned char)txt[pos])) ++pos;
        tokens.emplace_back(txt.substr(start, pos - start));
        // 2) Separador: secuencia de whitespace (incluye '\n', ' ', '\t', ...)
        start = pos;
        while (pos < n && isspace((unsigned char)txt[pos])) ++pos;
        seps.emplace_back(txt.substr(start, pos - start));
    }
}

// ==============================================================
//                    ENCODE   (un archivo)
// ==============================================================
double encode_file_omp(const string &path) {
    auto t0 = chrono::steady_clock::now();

    ifstream ifs(path);
    if (!ifs) {
        #pragma omp critical
        cerr << "Cannot open " << path << "\n";
        return 0.0;
    }
    // Leer todo el archivo en un string
    string text((istreambuf_iterator<char>(ifs)),
                istreambuf_iterator<char>());

    // 1) Tokenizar con separadores exactos
    vector<string> tokens, seps;
    tokens.reserve(1024);
    seps.reserve(1024);
    split_tokens_and_separators(text, tokens, seps);
    uint32 count = static_cast<uint32>(tokens.size());

    // 2) Construir diccionario y vector de IDs (zona crítica mínima)
    unordered_map<string, uint32> dict;
    dict.reserve(count / 2);
    vector<uint32> ids(count);
    uint32 next_id = 1;

    #pragma omp parallel for schedule(static)
    for (uint32 i = 0; i < count; ++i) {
        const string &w = tokens[i];
        uint32 id;
        #pragma omp critical(dict_section)
        {
            auto it = dict.find(w);
            if (it == dict.end()) {
                id = next_id;
                dict[w] = next_id++;
            } else {
                id = it->second;
            }
        }
        ids[i] = id;
    }

    string base = stem_of(path);

    // 3) Escribir vocab.bin
    {
        ofstream vout(base + "_vocab.bin", ios::binary);
        uint32 vs = static_cast<uint32>(dict.size());
        vout.write(reinterpret_cast<const char*>(&vs), sizeof(vs));
        for (auto &kv : dict) {
            uint32 len = static_cast<uint32>(kv.first.size());
            vout.write(reinterpret_cast<const char*>(&kv.second),
                       sizeof(kv.second));
            vout.write(reinterpret_cast<const char*>(&len), sizeof(len));
            vout.write(kv.first.data(), len);
        }
    }

    // 4) Escribir texto.bin: [count][ (ID, sep_len, sep_bytes)* ]
    {
        ofstream tout(base + "_texto.bin", ios::binary);
        tout.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (uint32 i = 0; i < count; ++i) {
            uint32 id   = ids[i];
            uint32 slen = static_cast<uint32>(seps[i].size());
            tout.write(reinterpret_cast<const char*>(&id), sizeof(id));
            tout.write(reinterpret_cast<const char*>(&slen), sizeof(slen));
            tout.write(seps[i].data(), slen);
        }
    }

    double secs = chrono::duration<double>(
                      chrono::steady_clock::now() - t0).count();
    #pragma omp critical
    cout << "[OMP] Encoded \"" << path << "\" tokens=" << count
         << " unique=" << dict.size()
         << " threads=" << omp_get_max_threads()
         << " | " << secs << " s\n";

    return secs;
}

// ==============================================================
//                    DECODE   (un archivo base)
// ==============================================================
double decode_file_omp(const string &stem) {
    auto t0 = chrono::steady_clock::now();

    // 1) Leer vocab.bin
    ifstream vfin(stem + "_vocab.bin", ios::binary);
    if (!vfin) {
        #pragma omp critical
        cerr << stem << "_vocab.bin not found\n";
        return 0.0;
    }
    uint32 vs;
    vfin.read(reinterpret_cast<char*>(&vs), sizeof(vs));
    vector<string> vocab(vs + 1);
    for (uint32 i = 0; i < vs; ++i) {
        uint32 id, len;
        vfin.read(reinterpret_cast<char*>(&id), sizeof(id));
        vfin.read(reinterpret_cast<char*>(&len), sizeof(len));
        string w(len, '\0');
        vfin.read(w.data(), len);
        vocab[id] = move(w);
    }

    // 2) Leer texto.bin y reconstruir
    ifstream fin(stem + "_texto.bin", ios::binary);
    if (!fin) {
        #pragma omp critical
        cerr << stem << "_texto.bin not found\n";
        return 0.0;
    }
    uint32 count;
    fin.read(reinterpret_cast<char*>(&count), sizeof(count));

    ofstream out(stem + "_recon.txt");
    for (uint32 i = 0; i < count; ++i) {
        uint32 id, slen;
        fin.read(reinterpret_cast<char*>(&id), sizeof(id));
        fin.read(reinterpret_cast<char*>(&slen), sizeof(slen));
        string sep(slen, '\0');
        fin.read(sep.data(), slen);
        out << vocab[id] << sep;
    }

    double secs = chrono::duration<double>(
                      chrono::steady_clock::now() - t0).count();
    #pragma omp critical
    cout << "[OMP] Decoded " << count << " tokens from \"" << stem << "\""
         << " | " << secs << " s\n";

    return secs;
}

// ==============================================================
//                            HELP
// ==============================================================
void help() {
    cout << "Usage:\n"
         << "  compacttext_omp encode <file1.txt> <file2.txt> …\n"
         << "  compacttext_omp decode <file1>     <file2>     …\n"
         << "         (para decode NO pongas .txt, sólo la base)\n\n";
}

// ==============================================================
//                            MAIN
// ==============================================================
int main(int argc, char* argv[]) {
    if (argc < 3) { help(); return 1; }
    string mode = argv[1];
    double total = 0.0;

    if (mode == "encode") {
        #pragma omp parallel for reduction(+:total) schedule(dynamic)
        for (int i = 2; i < argc; ++i)
            total += encode_file_omp(argv[i]);
        cout << "=== OMP encode total time: " << total << " s ===\n";

    } else if (mode == "decode") {
        #pragma omp parallel for reduction(+:total) schedule(dynamic)
        for (int i = 2; i < argc; ++i)
            total += decode_file_omp(argv[i]);
        cout << "=== OMP decode total time: " << total << " s ===\n";

    } else {
        help();
        return 1;
    }
    return 0;
}
