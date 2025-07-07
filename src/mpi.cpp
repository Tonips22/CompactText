/*----------------------------------------------------------------
 CompactText — versión MPI corregida (mantiene orden de palabras)
 ---------------------------------------------------------------
  ▸ ENCODE: cada proceso lee el texto, se queda con los índices
    globales idx donde (idx % size == rank) y la palabra.
  ▸ Rank 0 fusiona diccionarios, asigna IDs globales, difunde el mapa
    y finalmente recoloca cada ID en su posición original usando los
    índices recibidos de cada proceso.
  ▸ DECODIFICAR: lo hace sólo el rank 0 (igual que versión secuencial).
----------------------------------------------------------------*/

#include <mpi.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <regex>
#include <cstdint>
#include <iterator>
#include <algorithm>

using namespace std;
using uint32 = uint32_t;
using uint64 = uint64_t; // para los índices globales

// ------------------ Utilidades comunes ------------------
static const regex token_re("[A-Za-zÀ-ÖØ-öø-ÿ0-9'()\\[\\]{}\".,;:!?¿¡—-]+");

vector<string> tokenize(const string &text) {
    vector<string> words;
    for (sregex_iterator it(text.begin(), text.end(), token_re);
         it != sregex_iterator(); ++it) {
        words.emplace_back(it->str());
    }
    return words;
}

// ------------------ ENCODE (paralelo) ------------------
int encode_mpi(const string &input_path) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // 1. Todos los procesos leen el texto completo (solo‑lectura)
    ifstream ifs(input_path);
    if (!ifs) {
        if (rank == 0) cerr << "Cannot open " << input_path << "\n";
        return 1;
    }
    string text((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    auto words = tokenize(text);
    const uint64 N = words.size();

    // 2. Round‑robin: cada proceso obtiene pares (idx, palabra)
    vector<uint64> local_indices;
    local_indices.reserve((N + size - 1) / size);
    vector<string> local_words;
    local_words.reserve(local_indices.capacity());

    for (uint64 idx = rank; idx < N; idx += size) {
        local_indices.push_back(idx);
        local_words.push_back(words[idx]);
    }

    // 3. Diccionario local
    unordered_map<string, uint32> local_dict;
    for (const auto &w : local_words)
        local_dict.emplace(w, 0);

    // 4. Enviar palabras únicas a Rank 0
    vector<string> local_unique;
    local_unique.reserve(local_dict.size());
    for (const auto &p : local_dict) local_unique.push_back(p.first);

    auto join_words = [](const vector<string> &v) {
        ostringstream oss;
        for (size_t i = 0; i < v.size(); ++i) {
            oss << v[i];
            if (i + 1 < v.size()) oss << '\n';
        }
        return oss.str();
    };
    string local_blob = join_words(local_unique);
    int local_len = static_cast<int>(local_blob.size());

    // 4a. Rank 0 recibe tamaños de blobs
    vector<int> all_lens(size);
    MPI_Gather(&local_len, 1, MPI_INT,
               all_lens.data(), 1, MPI_INT,
               0, MPI_COMM_WORLD);

    // 4b. Rank 0 reserva buffer y desplazamientos
    vector<int> displs(size, 0);
    int total_chars = 0;
    if (rank == 0) {
        for (int i = 0; i < size; ++i) {
            displs[i] = total_chars;
            total_chars += all_lens[i];
        }
    }
    vector<char> recv_buf(total_chars);

    MPI_Gatherv(local_blob.data(), local_len, MPI_CHAR,
                recv_buf.data(), all_lens.data(), displs.data(), MPI_CHAR,
                0, MPI_COMM_WORLD);

    // 5. Rank 0 fusiona y asigna IDs globales
    unordered_map<string, uint32> global_dict;
    uint32 next_id = 1;
    if (rank == 0) {
        auto split_lines = [](const string &s) {
            vector<string> out;
            istringstream iss(s);
            string line;
            while (getline(iss, line)) out.emplace_back(move(line));
            return out;
        };
        // primero las suyas
        for (const auto &w : local_unique) global_dict[w] = next_id++;
        // ahora las de los demás
        for (int src = 1; src < size; ++src) {
            int start = displs[src];
            int len   = all_lens[src];
            string blob(recv_buf.data() + start, len);
            for (const auto &w : split_lines(blob))
                if (global_dict.emplace(w, next_id).second) ++next_id;
        }
    }

    // 6. Difunde el diccionario completo (texto "word id\n")
    string dict_blob;
    if (rank == 0) {
        ostringstream oss;
        for (const auto &[w, id] : global_dict) oss << w << ' ' << id << '\n';
        dict_blob = oss.str();
    }
    int dict_len = (rank == 0) ? static_cast<int>(dict_blob.size()) : 0;
    MPI_Bcast(&dict_len, 1, MPI_INT, 0, MPI_COMM_WORLD);
    vector<char> dict_buf(dict_len);
    if (rank == 0) copy(dict_blob.begin(), dict_blob.end(), dict_buf.begin());
    MPI_Bcast(dict_buf.data(), dict_len, MPI_CHAR, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        istringstream iss(string(dict_buf.data(), dict_len));
        string w; uint32 id;
        while (iss >> w >> id) global_dict[w] = id;
    }

    // 7. Codificar palabras locales + conservar índice
    vector<uint32> local_ids;
    local_ids.reserve(local_words.size());
    for (const auto &w : local_words) local_ids.push_back(global_dict[w]);

    int local_count = static_cast<int>(local_ids.size());
    vector<int> all_counts(size);
    MPI_Gather(&local_count, 1, MPI_INT, all_counts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> disp(size, 0);
    int total_items = 0;
    if (rank == 0) {
        for (int i = 0; i < size; ++i) {
            disp[i] = total_items;
            total_items += all_counts[i];
        }
    }

    // 8. Reunir índices globales (uint64) y IDs (uint32)
    vector<uint64> global_indices(total_items);
    vector<uint32> global_ids_raw(total_items);

    vector<uint64> local_idx64(local_indices.begin(), local_indices.end());

    MPI_Gatherv(local_idx64.data(), local_count, MPI_UNSIGNED_LONG_LONG,
                global_indices.data(), all_counts.data(), disp.data(), MPI_UNSIGNED_LONG_LONG,
                0, MPI_COMM_WORLD);

    MPI_Gatherv(local_ids.data(), local_count, MPI_UNSIGNED,
                global_ids_raw.data(), all_counts.data(), disp.data(), MPI_UNSIGNED,
                0, MPI_COMM_WORLD);

    // 9. Rank 0 reconstruye orden original y escribe binarios
    if (rank == 0) {
        vector<uint32> ordered_ids(N);
        for (int i = 0; i < total_items; ++i)
            ordered_ids[global_indices[i]] = global_ids_raw[i];

        // vocab.bin
        ofstream vout("vocab.bin", ios::binary);
        uint32 vocab_size = global_dict.size();
        vout.write(reinterpret_cast<const char*>(&vocab_size), sizeof(vocab_size));
        for (const auto &[word, id] : global_dict) {
            uint32 len = word.size();
            vout.write(reinterpret_cast<const char*>(&id), sizeof(id));
            vout.write(reinterpret_cast<const char*>(&len), sizeof(len));
            vout.write(word.data(), len);
        }
        // texto.bin
        ofstream tout("texto.bin", ios::binary);
        uint32 count = ordered_ids.size();
        tout.write(reinterpret_cast<const char*>(&count), sizeof(count));
        tout.write(reinterpret_cast<const char*>(ordered_ids.data()), sizeof(uint32)*count);

        cout << "[MPI] Encoded " << N << " words. Unique=" << vocab_size
                  << ", processes=" << size << "\n";
    }

    return 0;
}

// ------------------ DECODE (solo rank 0) ------------------
int decode_mpi(const string &output_path) {
    int rank; MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank != 0) return 0; // otros ranks terminan

    ifstream vocab("vocab.bin", ios::binary);
    if (!vocab) { cerr << "vocab.bin not found\n"; return 1; }
    uint32 vocab_size; vocab.read(reinterpret_cast<char*>(&vocab_size), sizeof(vocab_size));
    vector<string> vocab_vec(vocab_size + 1);
    for (uint32 i = 0; i < vocab_size; ++i) {
        uint32 id, len; vocab.read(reinterpret_cast<char*>(&id), sizeof(id));
        vocab.read(reinterpret_cast<char*>(&len), sizeof(len));
        string w(len, '\0'); vocab.read(w.data(), len);
        vocab_vec[id] = move(w);
    }

    ifstream textbin("texto.bin", ios::binary);
    if (!textbin) { cerr << "texto.bin not found\n"; return 1; }
    uint32 count; textbin.read(reinterpret_cast<char*>(&count), sizeof(count));
    vector<uint32> ids(count);
    textbin.read(reinterpret_cast<char*>(ids.data()), sizeof(uint32)*count);

    ofstream out(output_path);
    for (size_t i = 0; i < ids.size(); ++i) {
        out << vocab_vec[ids[i]];
        if (i + 1 < ids.size()) out << ' ';
    }

    cout << "Decoded " << ids.size() << " words into " << output_path << "\n";
    return 0;
}

// ------------------ Ayuda ------------------
void help(int rank = 0) {
    if (rank == 0)
        cout << "Usage:\n  compacttext_mpi encode <input.txt>\n  compacttext_mpi decode <output.txt>\n";
}

// ------------------ main ------------------
int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank; MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (argc < 2) { help(rank); MPI_Finalize(); return 1; }

    string mode = argv[1];
    int ret = 0;
    if (mode == "encode" && argc == 3) {
        ret = encode_mpi(argv[2]);
    } else if (mode == "decode") {
        string out = (argc == 3) ? argv[2] : "reconstruido.txt";
        ret = decode_mpi(out);
    } else {
        help(rank);
        ret = 1;
    }

    MPI_Finalize();
    return ret;
}
