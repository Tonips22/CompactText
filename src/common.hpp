// common.hpp
#pragma once

#include <string>
#include <vector>
#include <filesystem>

using std::string;
using std::vector;

// ===== Prototipos comunes por versión =====

double encode_file_seq(const string& path);
double decode_file_seq(const string& stem);

double encode_file_omp(const string& path);
double decode_file_omp(const string& stem);

double encode_file_mpi(const string& path);
double decode_file_mpi(const string& stem);

void help();

// ===== Utilidad para obtener el "stem" de un path (sin extensión) =====
inline string stem_of(const string& path) {
    namespace fs = std::filesystem;
    fs::path p(path);
    return (p.parent_path() / p.stem()).string();
}