#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <regex>
#include <cstdint>
#include <iterator>
#include <chrono>

using namespace std;
using uint32 = uint32_t;

void help() {
    cout << "\nUsage:\n"
              << "  compacttext_seq encode <input.txt>\n"
              << "  compacttext_seq decode <output.txt>\n\n";
}

// ------------------------- UTILIDADES -------------------------
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

// ------------------------- ENCODE -----------------------------
// Lee un .txt, crea diccionario e IDs y genera vocab.bin + texto.bin
int encode(const string &input_path) {
    auto t0 = chrono::steady_clock::now();

    ifstream ifs(input_path);
    if (!ifs) {
        cerr << "Cannot open " << input_path << "\n";
        return 1;
    }
    string text((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    auto words = tokenize(text);

    unordered_map<string, uint32> dict;
    vector<uint32> ids;
    ids.reserve(words.size());
    uint32 next_id = 1;

    for (const auto &w : words) {
        auto it = dict.find(w);
        if (it == dict.end()) {
            dict[w] = next_id;
            ids.push_back(next_id);
            ++next_id;
        } else {
            ids.push_back(it->second);
        }
    }

    // --- Escribir vocab.bin ---
    {
        ofstream out("vocab.bin", ios::binary);
        uint32 size = dict.size();
        out.write(reinterpret_cast<const char *>(&size), sizeof(size));
        for (const auto &[word, id] : dict) {
            uint32 len = static_cast<uint32>(word.size());
            out.write(reinterpret_cast<const char *>(&id), sizeof(id));
            out.write(reinterpret_cast<const char *>(&len), sizeof(len));
            out.write(word.data(), len);
        }
    }

    // --- Escribir texto.bin ---
    {
        ofstream out("texto.bin", ios::binary);
        uint32 count = ids.size();
        out.write(reinterpret_cast<const char *>(&count), sizeof(count));
        out.write(reinterpret_cast<const char *>(ids.data()), sizeof(uint32) * count);
    }

    auto t1 = chrono::steady_clock::now();
    auto duration = chrono::duration_cast<chrono::nanoseconds>(t1 - t0).count();
    cout << "Encoding took " << duration << " ns\n";

    cout << "Encoded " << words.size()
         << " words. Unique=" << dict.size()
         << " | time = " << duration << "ns\n";
    return 0;
}

// ------------------------- DECODE -----------------------------
// Reconstruye el texto original usando vocab.bin y texto.bin
int decode(const string &output_path) {

    auto t0 = chrono::steady_clock::now();

    // Leer vocab.bin
    ifstream vocab("vocab.bin", ios::binary);
    if (!vocab) {
        cerr << "vocab.bin not found\n";
        return 1;
    }
    uint32 vocab_size;
    vocab.read(reinterpret_cast<char *>(&vocab_size), sizeof(vocab_size));
    vector<string> vocab_vec(vocab_size + 1); // IDs comienzan en 1
    for (uint32 i = 0; i < vocab_size; ++i) {
        uint32 id, len;
        vocab.read(reinterpret_cast<char *>(&id), sizeof(id));
        vocab.read(reinterpret_cast<char *>(&len), sizeof(len));
        string w(len, '\0');
        vocab.read(w.data(), len);
        vocab_vec[id] = move(w);
    }

    // Leer texto.bin
    ifstream textbin("texto.bin", ios::binary);
    if (!textbin) {
        cerr << "texto.bin not found\n";
        return 1;
    }
    uint32 count;
    textbin.read(reinterpret_cast<char *>(&count), sizeof(count));
    vector<uint32> ids(count);
    textbin.read(reinterpret_cast<char *>(ids.data()), sizeof(uint32) * count);

    // Escribir texto reconstruido
    ofstream out(output_path);
    for (size_t i = 0; i < ids.size(); ++i) {
        out << vocab_vec[ids[i]];
        if (i + 1 < ids.size())
            out << ' ';
    }

    auto t1 = chrono::steady_clock::now();
    auto duration = chrono::duration_cast<chrono::nanoseconds>(t1 - t0).count();
    cout << "Decoding took " << duration << " ns\n";

    cout << "Decoded " << ids.size()<< " words into " << output_path
         << " | time=" << duration << " ns\n";
    return 0;
}

// ------------------------- MAIN -------------------------------
int main(int argc, char *argv[]) {
    if (argc < 2) {
        help();
        return 1;
    }
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
