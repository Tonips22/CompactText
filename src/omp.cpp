/*****************************************************************
 CompactText — versión OpenMP optimizada  
 Multi-archivo + cronómetro + separadores exactos + fusión de diccionarios
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
// separadores exactos (whitespace: ' ', '\t', '\n', etc.)
static void split_tokens_and_separators(const string& txt,
                                        vector<string>& tokens,
                                        vector<string>& seps) {
    size_t n = txt.size(), pos = 0;
    while (pos < n) {
        // 1) Token = secuencia de NO-whitespace
        size_t start = pos;
        while (pos < n && !isspace(static_cast<unsigned char>(txt[pos]))) ++pos;
        tokens.emplace_back(txt.substr(start, pos - start));
        // 2) Separador = secuencia de whitespace
        start = pos;
        while (pos < n && isspace(static_cast<unsigned char>(txt[pos]))) ++pos;
        seps.emplace_back(txt.substr(start, pos - start));
    }
}

// ==============================================================
//                    ENCODE   (un archivo)
// ==============================================================  
void encode_file_omp(const string &path) {
    double t0 = omp_get_wtime();

    ifstream ifs(path);
    if (!ifs) {
        #pragma omp critical
        cerr << "Cannot open " << path << "\n";
        return;
    }
    // Leer todo el archivo en memoria
    string text((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());

    // 1) Tokenizar con separadores exactos
    vector<string> tokens, seps;
    tokens.reserve(1024);
    seps.reserve(1024);
    split_tokens_and_separators(text, tokens, seps);
    uint32 count = static_cast<uint32>(tokens.size());

    // 2) MAP: cada hilo construye su diccionario local (sin locks)
    int T = omp_get_max_threads();
    vector<unordered_map<string,uint32>> local_dicts(T);
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        auto &ld = local_dicts[tid];
        ld.reserve(count / T);
        #pragma omp for schedule(static)
        for (uint32 i = 0; i < count; ++i) {
            ld.emplace(tokens[i], 0);
        }
    }

    // 3) REDUCE: fusionar diccionarios locales en uno global
    unordered_map<string,uint32> dict;
    dict.reserve(count / 2);
    uint32 next_id = 1;
    for (int t = 0; t < T; ++t) {
        for (auto &kv : local_dicts[t]) {
            if (dict.emplace(kv.first, next_id).second)
                ++next_id;
        }
    }

    // 4) ASSIGN: asignar IDs en paralelo (lectura concurrente del dict)
    vector<uint32> ids(count);
    #pragma omp parallel for schedule(static)
    for (uint32 i = 0; i < count; ++i) {
        ids[i] = dict[tokens[i]];
    }

    string base = stem_of(path);

    // 5) Escribir vocab.bin
    {
        ofstream vout(base + "_vocab.bin", ios::binary);
        uint32 vs = static_cast<uint32>(dict.size());
        vout.write(reinterpret_cast<const char*>(&vs), sizeof(vs));
        for (const auto &kv : dict) {
            uint32 len = static_cast<uint32>(kv.first.size());
            vout.write(reinterpret_cast<const char*>(&kv.second), sizeof(kv.second));
            vout.write(reinterpret_cast<const char*>(&len), sizeof(len));
            vout.write(kv.first.data(), len);
        }
    }

    // 6) Escribir texto.bin: [count][ (ID, sep_len, sep_bytes)* ]
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

    double secs = omp_get_wtime() - t0;
    #pragma omp critical
    cout << "[OMP] Encoded \"" << path << "\" tokens=" << count
         << " unique=" << dict.size()
         << " threads=" << T
         << " | " << secs << " s\n";
}

// ==============================================================
//                    DECODE   (un archivo base)
// ==============================================================  
void decode_file_omp(const string &stem) {
    double t0 = omp_get_wtime();

    // 1) Leer vocab.bin
    ifstream vfin(stem + "_vocab.bin", ios::binary);
    if (!vfin) {
        #pragma omp critical
        cerr << stem << "_vocab.bin not found\n";
        return;
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

    // 2) Leer texto.bin y reconstruir exactamente
    ifstream fin(stem + "_texto.bin", ios::binary);
    if (!fin) {
        #pragma omp critical
        cerr << stem << "_texto.bin not found\n";
        return;
    }
    uint32 count;
    fin.read(reinterpret_cast<char*>(&count), sizeof(count));

    ofstream out(stem + "_recon.txt");
    for (uint32 i = 0; i < count; ++i) {
        uint32 id, slen;
        fin.read(reinterpret_cast<char*>(&id),   sizeof(id));
        fin.read(reinterpret_cast<char*>(&slen), sizeof(slen));
        string sep(slen, '\0');
        fin.read(sep.data(), slen);
        out << vocab[id] << sep;
    }

    double secs = omp_get_wtime() - t0;
    #pragma omp critical
    cout << "[OMP] Decoded " << count << " tokens from \"" << stem
         << "\" | " << secs << " s\n";
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

    if (mode == "encode") {
        double t0 = omp_get_wtime();
        #pragma omp parallel for schedule(dynamic)
        for (int i = 2; i < argc; ++i)
            encode_file_omp(argv[i]);
        
        double total = omp_get_wtime() - t0;
        cout << "=== OMP encode wall time: " << total << " s ===\n";

    } else if (mode == "decode") {
        double t0 = omp_get_wtime();
        #pragma omp parallel for schedule(dynamic)
        for (int i = 2; i < argc; ++i)
            decode_file_omp(argv[i]);
        
        double total = omp_get_wtime() - t0;
        cout << "=== OMP decode total time: " << total << " s ===\n";

    } else {
        help();
        return 1;
    }
    return 0;
}
