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
constexpr int OpeningTotalMaterial = 6200;
constexpr int MiddlegameTotalMaterial = 4400;
constexpr bool ResultIsWhitePerspective = true;
constexpr std::array<int, 5> PieceValues{100, 320, 330, 500, 950};

constexpr int SmallSacMin = 90;
constexpr int SmallSacCompMin = 180;
constexpr int MediumSacMin = 280;
constexpr int MediumSacCompMin = 320;
constexpr int LargeSacMin = 480;
constexpr int LargeSacCompMin = 420;
constexpr int HugeSacMin = 880;
constexpr int HugeSacCompMin = 520;

constexpr double DecisiveWin = 0.99;
constexpr double DecisiveLoss = 0.01;
constexpr double DrawnHigh = 0.75;
constexpr double DrawnLow = 0.25;

constexpr int MaxEvalSanity = 4000;

struct ParsedLine {
    std::size_t fen_end{};
    int eval{};
    double result{};
};

struct MaterialScore {
    int balance{};
    int total{};
    int minors_white{};
    int minors_black{};
    int majors_white{};
    int majors_black{};
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
    score.minors_white = board.material_count[Colors::White][1] + board.material_count[Colors::White][2];
    score.minors_black = board.material_count[Colors::Black][1] + board.material_count[Colors::Black][2];
    score.majors_white = board.material_count[Colors::White][3] + board.material_count[Colors::White][4];
    score.majors_black = board.material_count[Colors::Black][3] + board.material_count[Colors::Black][4];
    return score;
}

int phase_bonus(int total_material) {
    if (total_material >= OpeningTotalMaterial) {
        return -80;
    }
    if (total_material >= MiddlegameTotalMaterial) {
        return -40;
    }
    return 0;
}

bool is_tiered_sacrifice(int winner_eval, int winner_material, int total_material) {
    if (winner_eval < SmallSacCompMin) {
        return false;
    }
    if (winner_eval > MaxEvalSanity) {
        return false;
    }

    const int deficit = -winner_material;
    const int phase_adj = phase_bonus(total_material);

    if (deficit >= HugeSacMin && winner_eval >= HugeSacCompMin + phase_adj) {
        return true;
    }
    if (deficit >= LargeSacMin && winner_eval >= LargeSacCompMin + phase_adj) {
        return true;
    }
    if (deficit >= MediumSacMin && winner_eval >= MediumSacCompMin + phase_adj) {
        return true;
    }
    if (deficit >= SmallSacMin && winner_eval >= SmallSacCompMin + phase_adj && total_material >= OpeningTotalMaterial) {
        return true;
    }
    return false;
}

bool is_initiative_position(int side_eval, int side_material, int total_material,
                            const MaterialScore& mat, bool white_to_move) {
    if (total_material < MiddlegameTotalMaterial) {
        return false;
    }

    const int active_minors = white_to_move ? mat.minors_white : mat.minors_black;
    const int passive_minors = white_to_move ? mat.minors_black : mat.minors_white;

    if (active_minors < passive_minors) {
        return false;
    }

    const int deficit = -side_material;
    if (deficit < 40) {
        return false;
    }
    if (side_eval < 150) {
        return false;
    }
    if (side_eval > MaxEvalSanity) {
        return false;
    }
    return (side_eval - side_material) >= 260;
}

bool is_durable_compensation(int side_eval, int side_material, double side_result) {
    if (side_result < DrawnLow || side_result > DrawnHigh) {
        return false;
    }
    if (side_material >= -120) {
        return false;
    }
    if (side_eval < 0) {
        return false;
    }
    return (side_eval - side_material) >= 350;
}

bool keep_position(int eval, int material, double side_result, int total_material,
                   const MaterialScore& mat, bool white_to_move) {
    if (side_result >= DecisiveWin) {
        if (is_tiered_sacrifice(eval, material, total_material)) {
            return true;
        }
        return is_initiative_position(eval, material, total_material, mat, white_to_move);
    }

    if (side_result <= DecisiveLoss) {
        MaterialScore flipped = mat;
        std::swap(flipped.minors_white, flipped.minors_black);
        std::swap(flipped.majors_white, flipped.majors_black);
        if (is_tiered_sacrifice(-eval, -material, total_material)) {
            return true;
        }
        return is_initiative_position(-eval, -material, total_material, flipped, !white_to_move);
    }

    if (is_durable_compensation(eval, material, side_result)) {
        return true;
    }
    if (is_durable_compensation(-eval, -material, 1.0 - side_result)) {
        return true;
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

        if (keep_position(side_eval, side_material, side_result, material.total, material, white_to_move)) {
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
