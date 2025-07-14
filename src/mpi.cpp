/*****************************************************************
 CompactText — versión MPI  
 Multi-archivo + cronómetro + separadores exactos
*****************************************************************/
#include <mpi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <filesystem>
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
//                  ENCODE SINGLE (secuencial)
// ==============================================================
static double encode_single(const string &path) {
    double t0 = MPI_Wtime();

    ifstream ifs(path);
    if (!ifs) {
        cerr << "Cannot open " << path << "\n";
        return 0.0;
    }
    // Leer todo el archivo
    string text((istreambuf_iterator<char>(ifs)), {});

    // Tokenizar con separadores
    vector<string> tokens, seps;
    tokens.reserve(1024);
    seps.reserve(1024);
    split_tokens_and_separators(text, tokens, seps);
    uint32 count = static_cast<uint32>(tokens.size());

    // Construir diccionario y vector de IDs
    unordered_map<string,uint32> dict;
    dict.reserve(count/2);
    vector<uint32> ids(count);
    uint32 next_id = 1;
    for (uint32 i = 0; i < count; ++i) {
        auto &w = tokens[i];
        auto it = dict.find(w);
        uint32 id;
        if (it == dict.end()) {
            id = next_id;
            dict[w] = next_id++;
        } else {
            id = it->second;
        }
        ids[i] = id;
    }

    string base = stem_of(path);

    // Escribir vocab.bin
    {
        ofstream vout(base + "_vocab.bin", ios::binary);
        uint32 vs = static_cast<uint32>(dict.size());
        vout.write((char*)&vs, sizeof(vs));
        for (auto &kv : dict) {
            uint32 len = static_cast<uint32>(kv.first.size());
            vout.write((char*)&kv.second, sizeof(kv.second));
            vout.write((char*)&len,    sizeof(len));
            vout.write(kv.first.data(), len);
        }
    }

    // Escribir texto.bin: [count][ (ID, seplen, sepbytes)* ]
    {
        ofstream tout(base + "_texto.bin", ios::binary);
        tout.write((char*)&count, sizeof(count));
        for (uint32 i = 0; i < count; ++i) {
            uint32 id   = ids[i];
            uint32 slen = static_cast<uint32>(seps[i].size());
            tout.write((char*)&id,   sizeof(id));
            tout.write((char*)&slen, sizeof(slen));
            tout.write(seps[i].data(), slen);
        }
    }

    return MPI_Wtime() - t0;
}

// ==============================================================
//                 DECODE SINGLE (secuencial)
// ==============================================================
static double decode_single(const string &stem) {
    double t0 = MPI_Wtime();

    // Leer vocab.bin
    ifstream vfin(stem + "_vocab.bin", ios::binary);
    if (!vfin) {
        cerr << stem << "_vocab.bin not found\n";
        return 0.0;
    }
    uint32 vs;
    vfin.read((char*)&vs, sizeof(vs));
    vector<string> vocab(vs + 1);
    for (uint32 i = 0; i < vs; ++i) {
        uint32 id,len;
        vfin.read((char*)&id,  sizeof(id));
        vfin.read((char*)&len, sizeof(len));
        string w(len, '\0');
        vfin.read(w.data(), len);
        vocab[id] = move(w);
    }

    // Leer texto.bin y reconstruir
    ifstream fin(stem + "_texto.bin", ios::binary);
    if (!fin) {
        cerr << stem << "_texto.bin not found\n";
        return 0.0;
    }
    uint32 count;
    fin.read((char*)&count, sizeof(count));

    ofstream out(stem + "_recon.txt");
    for (uint32 i = 0; i < count; ++i) {
        uint32 id, slen;
        fin.read((char*)&id,   sizeof(id));
        fin.read((char*)&slen, sizeof(slen));
        string sep(slen, '\0');
        fin.read(sep.data(), slen);
        out << vocab[id] << sep;
    }

    return MPI_Wtime() - t0;
}

// ==============================================================
//                           MAIN
// ==============================================================
void help() {
    cout << "Usage:\n"
         << "  compacttext_mpi encode <file1.txt> <file2.txt> …\n"
         << "  compacttext_mpi decode <file1>     <file2>     …\n"
         << "    (para decode NO pongas .txt, sólo la base)\n\n";
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    int rank,size;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

    if (argc < 3) {
        if (rank==0) help();
        MPI_Finalize();
        return 1;
    }
    string mode = argv[1];

    // 1) Rank0 construye lista “\n”-separada y la broadcast
    string blob;
    if (rank==0) {
        ostringstream oss;
        for (int i = 2; i < argc; ++i) {
            oss << argv[i];
            if (i+1<argc) oss << '\n';
        }
        blob = oss.str();
    }
    int len = blob.size();
    MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);
    blob.resize(len);
    MPI_Bcast(blob.data(), len, MPI_CHAR, 0, MPI_COMM_WORLD);

    // 2) Todos reconstruyen el vector<string> files
    vector<string> files;
    istringstream iss(blob);
    string line;
    while (getline(iss,line)) files.push_back(line);

    // 3) Round-robin: cada rank toma files[i] donde i%size==rank
    vector<string> myFiles;
    for (int i = rank; i < (int)files.size(); i += size)
        myFiles.push_back(files[i]);

    // 4) Ejecutar localmente
    double local_sum = 0.0;
    if (mode == "encode") {
        for (auto &f : myFiles) local_sum += encode_single(f);
    } else if (mode == "decode") {
        for (auto &s : myFiles) local_sum += decode_single(s);
    } else {
        if (rank==0) help();
        MPI_Finalize();
        return 1;
    }

    // 5) Reducir tiempos y mostrar en rank0
    double global_sum = 0.0;
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE,
               MPI_SUM, 0, MPI_COMM_WORLD);
    if (rank==0) {
        cout << "=== MPI " << mode
             << " total time: " << global_sum
             << " s (" << size << " processes) ===\n";
    }

    MPI_Finalize();
    return 0;
}
