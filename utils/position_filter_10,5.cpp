#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

#include "base.h"

namespace {
constexpr const char* Separator = " | ";
constexpr std::size_t SeparatorSize = 3;
constexpr std::size_t FlushBytes = 1 << 20;
constexpr int MinTotalMaterial = 3000;
constexpr bool ResultIsWhitePerspective = true;
constexpr std::array<int, 5> PieceValues{100, 300, 300, 500, 900};

struct ParsedLine {
    std::size_t fen_end{};
    int eval{};
    double result{};
};

struct MaterialScore {
    int balance{};
    int total{};
};

bool trailing_space_only(const char* p) {
    while (*p != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*p))) {
            return false;
        }
        ++p;
    }
    return true;
}

bool parse_int(const std::string& text, int& value) {
    char* end = nullptr;
    errno = 0;

    const long parsed = std::strtol(text.c_str(), &end, 10);

    if (end == text.c_str() || errno == ERANGE) {
        return false;
    }

    if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
        return false;
    }

    if (!trailing_space_only(end)) {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

bool parse_double(const std::string& text, double& value) {
    char* end = nullptr;
    errno = 0;

    const double parsed = std::strtod(text.c_str(), &end);

    if (end == text.c_str() || errno == ERANGE || !std::isfinite(parsed)) {
        return false;
    }

    if (!trailing_space_only(end)) {
        return false;
    }

    if (parsed < 0.0 || parsed > 1.0) {
        return false;
    }

    value = parsed;
    return true;
}

bool parse_line(const std::string& line, ParsedLine& parsed) {
    const std::size_t first = line.find(Separator);

    if (first == std::string::npos) {
        return false;
    }

    const std::size_t eval_begin = first + SeparatorSize;
    const std::size_t second = line.find(Separator, eval_begin);

    if (second == std::string::npos) {
        return false;
    }

    const std::size_t result_begin = second + SeparatorSize;

    int eval = 0;
    double result = 0.0;

    if (!parse_int(line.substr(eval_begin, second - eval_begin), eval)) {
        return false;
    }

    if (!parse_double(line.substr(result_begin), result)) {
        return false;
    }

    parsed.fen_end = first;
    parsed.eval = eval;
    parsed.result = result;
    return true;
}

MaterialScore material_score(const board_info& board) {
    MaterialScore score;

    for (std::size_t i = 0; i < PieceValues.size(); ++i) {
        const int white = board.material_count[Colors::White][i];
        const int black = board.material_count[Colors::Black][i];

        score.balance += (white - black) * PieceValues[i];
        score.total += (white + black) * PieceValues[i];
    }

    return score;
}

bool is_dynamic_sacrifice(int winner_eval, int winner_material) {
    const int compensation = winner_eval - winner_material;
    return (winner_material < 100) && (compensation > 400);
}

bool keep_position(int eval, int material, double side_result) {
    if (side_result >= 0.99) {
        return is_dynamic_sacrifice(eval, material);
    }

    if (side_result <= 0.01) {
        return is_dynamic_sacrifice(-eval, -material);
    }

    return false;
}

void flush_buffer(std::ofstream& fout, std::string& buffer) {
    if (!buffer.empty()) {
        fout.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        buffer.clear();
    }
}
}

int filter(const std::string& input, const std::string& output) {
    std::ifstream fin(input);
    std::ofstream fout(output, std::ios::trunc);

    if (!fin) {
        std::cerr << "Could not open input file: " << input << '\n';
        return 1;
    }

    if (!fout) {
        std::cerr << "Could not open output file: " << output << '\n';
        return 1;
    }

    std::string line;
    std::string buffer;
    buffer.reserve(FlushBytes);

    std::uint64_t total_lines = 0;
    std::uint64_t filtered_lines = 0;
    std::uint64_t skipped_lines = 0;

    while (std::getline(fin, line)) {
        ++total_lines;

        ParsedLine parsed;

        if (!parse_line(line, parsed)) {
            ++skipped_lines;
            continue;
        }

        board_info board{};

        const char saved = line[parsed.fen_end];
        line[parsed.fen_end] = '\0';
        const auto color = setfromfen(&board, line.c_str());
        line[parsed.fen_end] = saved;

        const bool white_to_move = color == Colors::White;
        const int perspective = white_to_move ? 1 : -1;

        const MaterialScore material = material_score(board);

        if (material.total < MinTotalMaterial) {
            continue;
        }

        const int side_eval = parsed.eval * perspective;
        const int side_material = material.balance * perspective;

        const double side_result = ResultIsWhitePerspective
            ? (white_to_move ? parsed.result : 1.0 - parsed.result)
            : parsed.result;

        if (keep_position(side_eval, side_material, side_result)) {
            ++filtered_lines;
            buffer.append(line);
            buffer.push_back('\n');

            if (buffer.size() >= FlushBytes) {
                flush_buffer(fout, buffer);
            }
        }
    }

    flush_buffer(fout, buffer);

    if (!fout) {
        std::cerr << "Write failed: " << output << '\n';
        return 1;
    }

    std::cout << total_lines << " positions read in, "
              << filtered_lines << " filtered, "
              << skipped_lines << " skipped\n";

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " input output\n";
        return 1;
    }

    const auto start = std::chrono::steady_clock::now();
    const int result = filter(argv[1], argv[2]);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;
    std::cout << elapsed.count() << " seconds\n";

    return result;
}
