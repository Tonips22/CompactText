/*****************************************************************
 CompactText — versión MPI
 Procesa MÚLTIPLES archivos en paralelo (uno o varios por proceso)
*****************************************************************/
#include <mpi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <regex>
#include <filesystem>
#include <iterator>
#include <sstream>
#include <cstdint>
#include "common.hpp"

using namespace std;

using uint32 = uint32_t;
namespace fs = filesystem;

/* =======================================================
    ENCODE  (un archivo, secuencial)
    Devuelve segundos empleados
   =======================================================*/
double encode_single(const string &path)
{
    double t0 = MPI_Wtime();

    ifstream ifs(path);
    if (!ifs) {
        cerr << "Cannot open " << path << "\n";
        return 0.0;
    }
    string text((istreambuf_iterator<char>(ifs)), {});
    auto words = tokenize(text);

    unordered_map<string, uint32> dict;
    dict.reserve(words.size()/2);
    vector<uint32> ids;
    ids.reserve(words.size());
    uint32 next = 1;

    for (auto &w: words) {
        auto it = dict.find(w);
        uint32 id;
        if (it == dict.end()) {
            id = next;
            dict[w] = next++;
        } else id = it->second;
        ids.push_back(id);
    }

    string base = stem_of(path);
    // vocab
    {
        ofstream vout(base + "_vocab.bin", ios::binary);
        uint32 vs = dict.size();
        vout.write((char*)&vs, sizeof(vs));
        for (auto &kv: dict) {
            uint32 len = kv.first.size();
            vout.write((char*)&kv.second, sizeof(kv.second));
            vout.write((char*)&len, sizeof(len));
            vout.write(kv.first.data(), len);
        }
    }
    // texto
    {
        ofstream tout(base + "_texto.bin", ios::binary);
        uint32 cnt = ids.size();
        tout.write((char*)&cnt, sizeof(cnt));
        tout.write((char*)ids.data(), sizeof(uint32)*cnt);
    }

    return MPI_Wtime() - t0;
}

/* =======================================================
                 DECODE  (un archivo base, secuencial)
   =======================================================*/
double decode_single(const string &stem)
{
    double t0 = MPI_Wtime();

    ifstream vocab(stem + "_vocab.bin", ios::binary);
    if (!vocab) {
        cerr << stem << "_vocab.bin not found\n";
        return 0.0;
    }
    uint32 vs; vocab.read((char*)&vs, sizeof(vs));
    vector<string> vv(vs+1);
    for (uint32 i=0;i<vs;++i){
        uint32 id,len; vocab.read((char*)&id,sizeof(id));
        vocab.read((char*)&len,sizeof(len));
        string w(len,'\0'); vocab.read(w.data(),len);
        vv[id]=move(w);
    }

    ifstream tb(stem + "_texto.bin", ios::binary);
    if (!tb){ cerr << stem << "_texto.bin not found\n"; return 0.0; }
    uint32 cnt; tb.read((char*)&cnt,sizeof(cnt));
    vector<uint32> ids(cnt);
    tb.read((char*)ids.data(), sizeof(uint32)*cnt);

    ofstream out(stem + "_recon.txt");
    for(size_t i=0;i<ids.size();++i){
        out << vv[ids[i]];
        if(i+1<ids.size()) out << ' ';
    }
    return MPI_Wtime() - t0;
}

/* ----------------- ayuda ----------------- */
void help(){
    cout << "Usage:\n"
              << "  compacttext_mpi encode <file1.txt> <file2.txt> …\n"
              << "  compacttext_mpi decode <file1>     <file2>     …\n"
              << "        (para decode NO pongas .txt, solo la base)\n\n";
}

/* =======================================================
                         MAIN
   =======================================================*/
int main(int argc, char* argv[])
{
    MPI_Init(&argc,&argv);

    int rank,size; MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

    if (argc < 3) { if(rank==0) help(); MPI_Finalize(); return 1; }
    string mode = argv[1];

    /* 1) Rank 0 concatena la lista de archivos */
    string blob;
    if(rank==0){
        ostringstream oss;
        for(int i=2;i<argc;++i){ oss << argv[i]; if(i+1<argc) oss << '\n'; }
        blob = oss.str();
    }
    int len = blob.size();
    MPI_Bcast(&len,1,MPI_INT,0,MPI_COMM_WORLD);
    blob.resize(len);
    MPI_Bcast(blob.data(), len, MPI_CHAR, 0, MPI_COMM_WORLD);

    /* 2) Todos reconstruyen la lista */
    vector<string> files;
    istringstream iss(blob); string line;
    while(getline(iss,line)) files.push_back(line);
    int numF = files.size();

    /* 3) Reparto round‑robin */
    vector<string> myFiles;
    for(int i=rank;i<numF;i+=size) myFiles.push_back(files[i]);

    /* 4) Ejecutar */
    double local_sum = 0.0;
    if (mode == "encode") {
        for(auto &f: myFiles) local_sum += encode_single(f);
    } else if (mode == "decode") {
        for(auto &s: myFiles) local_sum += decode_single(s);
    } else {
        if(rank==0) help();
        MPI_Finalize();
        return 1;
    }

    /* 5) Reducir y mostrar */
    double global_sum = 0.0;
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if(rank==0){
        cout << "=== MPI " << mode << " total time: " << global_sum << " s === (" << size << " processes) ===\n";
    }

    MPI_Finalize();
    return 0;
}
