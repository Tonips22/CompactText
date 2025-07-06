#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <regex>
#include <cstdint>
#include <iterator>
#include <omp.h>

using namespace std;
using uint32 = uint32_t;

/* ------------------------------------------------------------
   CompactText — Versión OpenMP (memoria compartida)
   ------------------------------------------------------------
   Solo paralelizamos la fase ENCODE, que es la costosa.
   DECODER hereda exactamente el mismo algoritmo secuencial
   porque reconstruir es I/O‑bound y no gana casi nada.
----------------------------------------------------------------*/

// ---------- Utils ----------
// Tokeniza un texto devolviendo palabras, números y algunos signos de puntuación
vector<string> tokenize(const string &text) {
    static const regex token_re("[A-Za-zÀ-ÖØ-öø-ÿ0-9'()\\[\\]{}\".,;:!?¿¡—\\-]+");
    vector<string> words;
    for (sregex_iterator it(text.begin(), text.end(), token_re);
         it != sregex_iterator(); ++it) {
        words.emplace_back(it->str());
    }
    return words;
}

// ---------- Encode (paralelo) ----------
int encode(const string &input_path) {
    // Leer texto completo
    ifstream ifs(input_path);
    if (!ifs) {
        cerr << "Cannot open " << input_path << "\n";
        return 1;
    }
    string text((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());

    // Tokenizar (aún secuencial; suele ser rápido)
    auto words = tokenize(text);
    const size_t N = words.size();

    // Estructuras compartidas
    unordered_map<string, uint32> dict;
    dict.reserve(N / 2);
    vector<uint32> ids(N);

    uint32 next_id = 1;

    // Paralelizar recorrido de palabras
    #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < N; ++i) {
            const string &w = words[i];
            uint32 id;
            // Zona crítica pequeña (insertar / leer del diccionario)
            #pragma omp critical(dict_section)
            {
                auto it = dict.find(w);
                if (it == dict.end()) {
                    id = next_id;
                    dict[w] = next_id;
                    ++next_id;
                } else {
                    id = it->second;
                }
            }
            ids[i] = id;
        }

    // ---- Guardar vocab.bin ----
    {
        ofstream out("vocab.bin", ios::binary);
        uint32 size = dict.size();
        out.write(reinterpret_cast<const char *>(&size), sizeof(size));
        for (const auto &[word, id] : dict) {
            uint32 len = word.size();
            out.write(reinterpret_cast<const char *>(&id), sizeof(id));
            out.write(reinterpret_cast<const char *>(&len), sizeof(len));
            out.write(word.data(), len);
        }
    }

    // ---- Guardar texto.bin ----
    {
        ofstream out("texto.bin", ios::binary);
        uint32 count = ids.size();
        out.write(reinterpret_cast<const char *>(&count), sizeof(count));
        out.write(reinterpret_cast<const char *>(ids.data()), sizeof(uint32) * count);
    }

    cout << "[OMP] Encoded " << N << " words. Unique=" << dict.size()
              << ", threads=" << omp_get_max_threads() << "\n";
    return 0;
}

// ---------- Decode (mismo que secuencial) ----------
int decode(const string &output_path) {
    ifstream vocab("vocab.bin", ios::binary);
    if (!vocab) {
        cerr << "vocab.bin not found\n";
        return 1;
    }
    uint32 vocab_size; vocab.read(reinterpret_cast<char *>(&vocab_size), sizeof(vocab_size));
    vector<string> vocab_vec(vocab_size + 1);
    for (uint32 i = 0; i < vocab_size; ++i) {
        uint32 id, len; vocab.read(reinterpret_cast<char *>(&id), sizeof(id));
        vocab.read(reinterpret_cast<char *>(&len), sizeof(len));
        string w(len, '\0'); vocab.read(w.data(), len);
        vocab_vec[id] = move(w);
    }

    ifstream textbin("texto.bin", ios::binary);
    if (!textbin) { cerr << "texto.bin not found\n"; return 1; }
    uint32 count; textbin.read(reinterpret_cast<char *>(&count), sizeof(count));
    vector<uint32> ids(count);
    textbin.read(reinterpret_cast<char *>(ids.data()), sizeof(uint32) * count);

    ofstream out(output_path);
    for (size_t i = 0; i < ids.size(); ++i) {
        out << vocab_vec[ids[i]];
        if (i + 1 < ids.size()) out << ' ';
    }
    cout << "Decoded " << ids.size() << " words into " << output_path << "\n";
    return 0;
}

// ---------- Ayuda ----------
void help() {
    cout << "\nUsage:\n"
              << "  compacttext_omp encode <input.txt>\n"
              << "  compacttext_omp decode <output.txt>\n\n";
}

// ---------- main ----------
int main(int argc, char *argv[]) {
    if (argc < 2) { help(); return 1; }
    string mode = argv[1];
    if (mode == "encode" && argc == 3) {
        return encode(argv[2]);
    } else if (mode == "decode") {
        string out = (argc == 3) ? argv[2] : "reconstruido.txt";
        return decode(out);
    } else {
        help();
        return 1;
    }
}
