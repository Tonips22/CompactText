/*****************************************************************
 CompactText — versión OpenMP  (multi-archivo + cronómetro)
*****************************************************************/
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <regex>
#include <cstdint>
#include <iterator>
#include <filesystem>
#include <chrono>
#include <omp.h>
#include "common.hpp"

using namespace std;

using uint32 = uint32_t;
namespace fs = filesystem;

/* ==============================================================
                    ENCODE   (un archivo)
   ==============================================================*/
double encode_file_omp(const string &path)
{
    auto t0 = chrono::steady_clock::now();

    ifstream ifs(path);
    if (!ifs) {
        #pragma omp critical
        cerr << "Cannot open " << path << "\n";
        return 0.0;
    }
    string text((istreambuf_iterator<char>(ifs)), {});

    auto words = tokenize(text);
    const size_t N = words.size();

    unordered_map<string, uint32> dict;
    dict.reserve(N / 2);
    vector<uint32> ids(N);
    uint32 next_id = 1;

    /* ---- paraleliza sobre las palabras ---- */
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < N; ++i) {
        const string &w = words[i];
        uint32 id;

        /* zona crítica sólo para el diccionario */
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

    /* ---- vocab.bin ---- */
    {
        ofstream vout(base + "_vocab.bin", ios::binary);
        uint32 size = dict.size();
        vout.write((char*)&size, sizeof(size));
        for (const auto &[word, id] : dict) {
            uint32 len = word.size();
            vout.write((char*)&id , sizeof(id ));
            vout.write((char*)&len, sizeof(len));
            vout.write(word.data(), len);
        }
    }

    /* ---- texto.bin ---- */
    {
        ofstream tout(base + "_texto.bin", ios::binary);
        uint32 count = ids.size();
        tout.write((char*)&count, sizeof(count));
        tout.write((char*)ids.data(), sizeof(uint32)*count);
    }

    double secs = chrono::duration<double>(
                      chrono::steady_clock::now() - t0).count();

    #pragma omp critical
    cout << "[OMP] Encoded \"" << path << "\"  words=" << N
              << "  unique=" << dict.size()
              << "  threads=" << omp_get_max_threads()
              << "  | " << secs << " s\n";

    return secs;
}

/* ==============================================================
                    DECODE   (un archivo base)
   ==============================================================*/
double decode_file_omp(const string &stem)   // p.e. data/messi_example.txt
{
    auto t0 = chrono::steady_clock::now();

    ifstream vocab(stem + "_vocab.bin", ios::binary);
    if (!vocab) {
        #pragma omp critical
        cerr << stem << "_vocab.bin not found\n";
        return 0.0;
    }
    uint32 vs; vocab.read((char*)&vs, sizeof(vs));
    vector<string> vv(vs + 1);
    for (uint32 i = 0; i < vs; ++i) {
        uint32 id,len; vocab.read((char*)&id ,sizeof(id ));
        vocab.read((char*)&len,sizeof(len));
        string w(len,'\0'); vocab.read(w.data(),len);
        vv[id] = move(w);
    }

    ifstream tb(stem + "_texto.bin", ios::binary);
    if (!tb) {
        #pragma omp critical
        cerr << stem << "_texto.bin not found\n";
        return 0.0;
    }
    uint32 cnt; tb.read((char*)&cnt, sizeof(cnt));
    vector<uint32> ids(cnt);
    tb.read((char*)ids.data(), sizeof(uint32)*cnt);

    ofstream out(stem + "_recon.txt");
    for (size_t i = 0; i < ids.size(); ++i) {
        out << vv[ids[i]];
        if (i + 1 < ids.size()) out << ' ';
    }

    double secs = chrono::duration<double>(
                      chrono::steady_clock::now() - t0).count();

    #pragma omp critical
    cout << "[OMP] Decoded \"" << stem << "\"  words=" << ids.size()
              << "  | " << secs << " s\n";

    return secs;
}

/* ==============================================================
                            HELP
   ==============================================================*/
void help()
{
    cout << "Usage:\n"
              << "  compacttext_omp encode <file1.txt> <file2.txt> …\n"
              << "  compacttext_omp decode <file1>     <file2>     …\n"
              << "         (para decode NO pongas .txt, sólo la base)\n";
}
/* ==============================================================
                            MAIN
   ==============================================================*/

int main(int argc, char* argv[])
{
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
