// omp.cpp — versión completa: encode + decode multiproceso con OpenMP

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <regex>
#include <omp.h>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std;
using u32 = uint32_t;

// Regex para tokenizar (palabras + signos)
static const regex token_re("[A-Za-zÀ-ÖØ-öø-ÿ0-9'()\\[\\]{}\".,;:!?¿¡—\\-]+");

vector<string> tokenize(const string &text) {
    vector<string> tokens;
    auto words_begin = sregex_iterator(text.begin(), text.end(), token_re);
    auto words_end = sregex_iterator();
    for (auto it = words_begin; it != words_end; ++it)
        tokens.push_back(it->str());
    return tokens;
}

void write_vocab(const string &filename, const vector<string> &vocab) {
    ofstream out(filename, ios::binary);
    for (const auto &word : vocab) {
        u32 len = word.size();
        out.write((char*)&len, sizeof(len));
        out.write(word.data(), len);
    }
}

vector<string> read_vocab(const string &filename) {
    vector<string> vocab;
    ifstream in(filename, ios::binary);
    while (in) {
        u32 len;
        in.read((char*)&len, sizeof(len));
        if (!in) break;
        string word(len, '\0');
        in.read(&word[0], len);
        vocab.push_back(word);
    }
    return vocab;
}

void write_ids(const string &filename, const vector<u32> &ids) {
    ofstream out(filename, ios::binary);
    for (u32 id : ids)
        out.write((char*)&id, sizeof(id));
}

vector<u32> read_ids(const string &filename) {
    vector<u32> ids;
    ifstream in(filename, ios::binary);
    u32 id;
    while (in.read((char*)&id, sizeof(id)))
        ids.push_back(id);
    return ids;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Uso:\n"
             << "  ./compacttext_omp encode archivo1.txt [archivo2.txt ...]\n"
             << "  ./compacttext_omp decode archivo1.bin [archivo2.bin ...]\n";
        return 1;
    }

    string mode = argv[1];
    vector<string> files(argv + 2, argv + argc);
    int n_files = files.size();

    if (mode == "encode") {
        vector<vector<string>> all_tokens(n_files);
        vector<unordered_set<string>> local_words(n_files);

        double t0 = omp_get_wtime();

        // Leer y tokenizar cada archivo en paralelo
        #pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < n_files; ++i) {
            ifstream in(files[i]);
            stringstream ss;
            ss << in.rdbuf();
            auto tokens = tokenize(ss.str());
            all_tokens[i] = tokens;
            for (const auto &w : tokens)
                local_words[i].insert(w);
        }

        // Construir diccionario global
        unordered_map<string, u32> global_dict;
        vector<string> vocab;
        {
            unordered_set<string> all_words;
            for (const auto &s : local_words)
                all_words.insert(s.begin(), s.end());

            u32 id = 0;
            for (const auto &w : all_words) {
                global_dict[w] = id++;
                vocab.push_back(w);
            }
        }

        write_vocab("vocab.bin", vocab);

        // Codificar cada archivo
        #pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < n_files; ++i) {
            vector<u32> ids;
            for (const auto &w : all_tokens[i])
                ids.push_back(global_dict[w]);

            string outname = fs::path(files[i]).stem().string() + ".bin";
            write_ids(outname, ids);

            #pragma omp critical
            cout << "[OMP] Codificato " << files[i] << " → " << outname << "\n";
        }

        double t = omp_get_wtime() - t0;
        cout << "\n[OMP] Encode completato in " << t << " secondi.\n";
    }

    else if (mode == "decode") {
        vector<string> vocab = read_vocab("vocab.bin");

        double t0 = omp_get_wtime();

        #pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < n_files; ++i) {
            auto ids = read_ids(files[i]);
            string outname = fs::path(files[i]).stem().string() + "_decoded.txt";

            ofstream out(outname);
            for (u32 id : ids)
                out << vocab[id] << ' ';

            #pragma omp critical
            cout << "[OMP] Decodificato " << files[i] << " → " << outname << "\n";
        }

        double t = omp_get_wtime() - t0;
        cout << "\n[OMP] Decode completato in " << t << " secondi.\n";
    }

    else {
        cerr << "Modo non riconosciuto: " << mode << endl;
        return 2;
    }

    return 0;
}

