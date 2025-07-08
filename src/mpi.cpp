#include <mpi.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std;
using u32 = uint32_t;

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
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0) {
            cerr << "Uso:\n"
                 << "  mpirun -np <num_procs> ./compacttext_mpi encode archivo1.txt [archivo2.txt ...]\n"
                 << "  mpirun -np <num_procs> ./compacttext_mpi decode archivo1.bin [archivo2.bin ...]\n";
        }
        MPI_Finalize();
        return 1;
    }

    string mode = argv[1];
    vector<string> files(argv + 2, argv + argc);
    int n_files = files.size();

    if (mode == "encode") {
        // Distribuir archivos entre procesos
        vector<string> my_files;
        for (int i = 0; i < n_files; ++i) {
            if (i % size == rank)
                my_files.push_back(files[i]);
        }

        vector<vector<string>> all_tokens_local(my_files.size());
        unordered_set<string> local_words;

        double t0 = MPI_Wtime();

        // Leer y tokenizar localmente
        for (size_t i = 0; i < my_files.size(); ++i) {
            ifstream in(my_files[i]);
            stringstream ss;
            ss << in.rdbuf();
            auto tokens = tokenize(ss.str());
            all_tokens_local[i] = tokens;
            for (const auto &w : tokens)
                local_words.insert(w);
        }

        // Recolectar todos los conjuntos de palabras únicas al rank 0
        // Serializar local_words en un string para enviar
        string local_words_serial;
        for (const auto &w : local_words) {
            local_words_serial += w + "\n";
        }
        int local_size = local_words_serial.size();

        // Enviar tamaños y datos al rank 0
        vector<int> recv_sizes(size);
        MPI_Gather(&local_size, 1, MPI_INT, recv_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

        vector<int> displs(size, 0);
        int total_size = 0;
        if (rank == 0) {
            for (int i = 0; i < size; ++i) {
                displs[i] = total_size;
                total_size += recv_sizes[i];
            }
        }

        vector<char> recv_buf(total_size);
        MPI_Gatherv(local_words_serial.data(), local_size, MPI_CHAR,
                    recv_buf.data(), recv_sizes.data(), displs.data(), MPI_CHAR,
                    0, MPI_COMM_WORLD);

        unordered_map<string, u32> global_dict;
        vector<string> vocab;

        if (rank == 0) {
            // Construir conjunto global de palabras únicas
            unordered_set<string> all_words;
            int pos = 0;
            for (int i = 0; i < size; ++i) {
                int sz = recv_sizes[i];
                string part(&recv_buf[displs[i]], sz);
                istringstream iss(part);
                string w;
                while (getline(iss, w)) {
                    if (!w.empty())
                        all_words.insert(w);
                }
            }
            u32 id = 0;
            for (const auto &w : all_words) {
                global_dict[w] = id++;
                vocab.push_back(w);
            }

            // Guardar vocab
            write_vocab("vocab.bin", vocab);
        }

        // Enviar tamaño vocab al resto
        int vocab_size = vocab.size();
        MPI_Bcast(&vocab_size, 1, MPI_INT, 0, MPI_COMM_WORLD);

        // Enviar vocab palabra por palabra
        for (int i = 0; i < vocab_size; ++i) {
            int len = 0;
            if (rank == 0) len = vocab[i].size();
            MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);
            char *buffer = new char[len];
            if (rank == 0)
                memcpy(buffer, vocab[i].data(), len);
            MPI_Bcast(buffer, len, MPI_CHAR, 0, MPI_COMM_WORLD);
            if (rank != 0)
                vocab.push_back(string(buffer, len));
            delete[] buffer;
        }

        // Reconstruir mapa global_dict en los demás procesos
        if (rank != 0) {
            for (int i = 0; i < vocab_size; ++i) {
                global_dict[vocab[i]] = i;
            }
        }

        // Codificar localmente y guardar binarios
        for (size_t i = 0; i < my_files.size(); ++i) {
            vector<u32> ids;
            for (const auto &w : all_tokens_local[i])
                ids.push_back(global_dict[w]);

            string outname = fs::path(my_files[i]).stem().string() + ".bin";
            write_ids(outname, ids);

            cout << "[MPI rank " << rank << "] Codificato " << my_files[i]
                 << " → " << outname << "\n";
        }

        double t = MPI_Wtime() - t0;
        if (rank == 0)
            cout << "[MPI] Encode completato in " << t << " secondi.\n";
    }

    else if (mode == "decode") {
        vector<string> vocab;
        if (rank == 0) {
            vocab = read_vocab("vocab.bin");
        }

        // Broadcast vocab a todos
        int vocab_size = 0;
        if (rank == 0) vocab_size = vocab.size();
        MPI_Bcast(&vocab_size, 1, MPI_INT, 0, MPI_COMM_WORLD);

        if (rank != 0) vocab.resize(vocab_size);

        for (int i = 0; i < vocab_size; ++i) {
            int len = 0;
            if (rank == 0) len = vocab[i].size();
            MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);
            char *buffer = new char[len];
            if (rank == 0) memcpy(buffer, vocab[i].data(), len);
            MPI_Bcast(buffer, len, MPI_CHAR, 0, MPI_COMM_WORLD);
            if (rank != 0) vocab[i] = string(buffer, len);
            delete[] buffer;
        }

        // Distribuir archivos para decodificar entre procesos
        vector<string> my_files;
        for (int i = 0; i < n_files; ++i) {
            if (i % size == rank)
                my_files.push_back(files[i]);
        }

        double t0 = MPI_Wtime();

        for (const auto &filename : my_files) {
            auto ids = read_ids(filename);
            string outname = fs::path(filename).stem().string() + "_decoded.txt";
            ofstream out(outname);
            for (u32 id : ids)
                out << vocab[id] << ' ';

            cout << "[MPI rank " << rank << "] Decodificato " << filename
                 << " → " << outname << "\n";
        }

        double t = MPI_Wtime() - t0;
        if (rank == 0)
            cout << "[MPI] Decode completato in " << t << " secondi.\n";
    }

    else {
        if (rank == 0)
            cerr << "Modo non riconosciuto: " << mode << endl;
        MPI_Finalize();
        return 2;
    }

    MPI_Finalize();
    return 0;
}

