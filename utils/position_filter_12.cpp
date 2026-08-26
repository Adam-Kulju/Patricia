#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {
constexpr std::size_t InputBufferBytes = 4u << 20;
constexpr std::size_t OutputBufferBytes = 4u << 20;

constexpr int PawnValue = 100;
constexpr int KnightValue = 320;
constexpr int BishopValue = 330;
constexpr int RookValue = 500;
constexpr int QueenValue = 950;

constexpr int MinTotalMaterial = 3000;
constexpr int OpeningTotalMaterial = 6200;
constexpr int MiddlegameTotalMaterial = 4400;

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

constexpr std::int64_t MaxEvalSanity = 4000;
constexpr bool ResultUsesWhitePerspective = true;

struct ParsedLine {
    std::size_t fen_end{};
    int evaluation{};
    double result{};
};

struct MaterialScore {
    int balance{};
    int total{};
    int white_minors{};
    int black_minors{};
};

bool is_ascii_space(unsigned char character) {
    return character == ' ' || (character >= '\t' && character <= '\r');
}

void trim_range(const char*& begin, const char*& end) {
    while (begin < end && is_ascii_space(static_cast<unsigned char>(*begin))) {
        ++begin;
    }

    while (end > begin && is_ascii_space(static_cast<unsigned char>(end[-1]))) {
        --end;
    }
}

bool parse_integer(const char* begin, const char* end, int& value) {
    trim_range(begin, end);

    if (begin == end) {
        return false;
    }

    if (*begin == '+') {
        ++begin;
        if (begin == end) {
            return false;
        }
    }

    int parsed = 0;
    const auto conversion = std::from_chars(begin, end, parsed);

    if (conversion.ec != std::errc{} || conversion.ptr != end) {
        return false;
    }

    value = parsed;
    return true;
}

bool parse_probability(const char* begin, const char* end, double& value) {
    trim_range(begin, end);

    if (begin == end) {
        return false;
    }

    if (*begin == '+') {
        ++begin;
        if (begin == end) {
            return false;
        }
    }

    double parsed = 0.0;
    const auto conversion = std::from_chars(
        begin,
        end,
        parsed,
        std::chars_format::general
    );

    if (conversion.ec != std::errc{} || conversion.ptr != end) {
        return false;
    }

    if (!std::isfinite(parsed) || parsed < 0.0 || parsed > 1.0) {
        return false;
    }

    value = parsed;
    return true;
}

bool parse_line(const char* line, std::size_t length, ParsedLine& parsed) {
    const char* line_end = line + length;

    const void* first_match = std::memchr(line, '|', length);
    if (first_match == nullptr) {
        return false;
    }

    const char* first_pipe = static_cast<const char*>(first_match);
    const char* evaluation_begin = first_pipe + 1;

    const void* second_match = std::memchr(
        evaluation_begin,
        '|',
        static_cast<std::size_t>(line_end - evaluation_begin)
    );

    if (second_match == nullptr) {
        return false;
    }

    const char* second_pipe = static_cast<const char*>(second_match);
    const char* fen_end = first_pipe;

    while (fen_end > line &&
           is_ascii_space(static_cast<unsigned char>(fen_end[-1]))) {
        --fen_end;
    }

    if (fen_end == line) {
        return false;
    }

    int evaluation = 0;
    double result = 0.0;

    if (!parse_integer(evaluation_begin, second_pipe, evaluation)) {
        return false;
    }

    if (!parse_probability(second_pipe + 1, line_end, result)) {
        return false;
    }

    parsed.fen_end = static_cast<std::size_t>(fen_end - line);
    parsed.evaluation = evaluation;
    parsed.result = result;
    return true;
}

bool extract_position_data(
    const char* fen,
    std::size_t fen_length,
    MaterialScore& material,
    bool& white_to_move
) {
    const char* cursor = fen;
    const char* end = fen + fen_length;

    while (cursor < end &&
           is_ascii_space(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }

    const char* board_begin = cursor;
    MaterialScore score;

    while (cursor < end &&
           !is_ascii_space(static_cast<unsigned char>(*cursor))) {
        const unsigned char symbol =
            static_cast<unsigned char>(*cursor++);
        const unsigned char lower =
            static_cast<unsigned char>(symbol | 0x20u);

        int value = 0;

        switch (lower) {
            case 'p':
                value = PawnValue;
                break;
            case 'n':
                value = KnightValue;
                break;
            case 'b':
                value = BishopValue;
                break;
            case 'r':
                value = RookValue;
                break;
            case 'q':
                value = QueenValue;
                break;
            case 'k':
                continue;
            default:
                if (symbol == '/' || (symbol >= '1' && symbol <= '8')) {
                    continue;
                }
                return false;
        }

        const bool white_piece = symbol >= 'A' && symbol <= 'Z';

        score.total += value;
        score.balance += white_piece ? value : -value;

        if (lower == 'n' || lower == 'b') {
            if (white_piece) {
                ++score.white_minors;
            } else {
                ++score.black_minors;
            }
        }
    }

    if (cursor == board_begin || cursor == end) {
        return false;
    }

    while (cursor < end &&
           is_ascii_space(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }

    if (cursor == end) {
        return false;
    }

    if (*cursor == 'w') {
        white_to_move = true;
    } else if (*cursor == 'b') {
        white_to_move = false;
    } else {
        return false;
    }

    ++cursor;

    if (cursor < end &&
        !is_ascii_space(static_cast<unsigned char>(*cursor))) {
        return false;
    }

    material = score;
    return true;
}

bool matches_result_window(double result) {
    return result <= DecisiveLoss ||
           result >= DecisiveWin ||
           (result >= DrawnLow && result <= DrawnHigh);
}

bool result_can_be_kept(double result) {
    if (matches_result_window(result)) {
        return true;
    }

    if (ResultUsesWhitePerspective) {
        return matches_result_window(1.0 - result);
    }

    return false;
}

int phase_adjustment(int total_material) {
    if (total_material >= OpeningTotalMaterial) {
        return -80;
    }

    if (total_material >= MiddlegameTotalMaterial) {
        return -40;
    }

    return 0;
}

bool is_tiered_sacrifice(
    std::int64_t winner_evaluation,
    std::int64_t winner_material,
    int total_material
) {
    if (winner_evaluation < SmallSacCompMin ||
        winner_evaluation > MaxEvalSanity) {
        return false;
    }

    const std::int64_t deficit = -winner_material;
    int required_evaluation = 0;

    if (deficit >= HugeSacMin) {
        required_evaluation = HugeSacCompMin;
    } else if (deficit >= LargeSacMin) {
        required_evaluation = LargeSacCompMin;
    } else if (deficit >= MediumSacMin) {
        required_evaluation = MediumSacCompMin;
    } else if (deficit >= SmallSacMin &&
               total_material >= OpeningTotalMaterial) {
        required_evaluation = SmallSacCompMin;
    } else {
        return false;
    }

    required_evaluation += phase_adjustment(total_material);
    return winner_evaluation >= required_evaluation;
}

bool is_initiative_position(
    std::int64_t winner_evaluation,
    std::int64_t winner_material,
    int total_material,
    const MaterialScore& material,
    bool winner_is_white
) {
    if (total_material < MiddlegameTotalMaterial) {
        return false;
    }

    const int active_minors = winner_is_white
        ? material.white_minors
        : material.black_minors;

    const int passive_minors = winner_is_white
        ? material.black_minors
        : material.white_minors;

    if (active_minors < passive_minors) {
        return false;
    }

    if (winner_material > -40) {
        return false;
    }

    if (winner_evaluation < 150 ||
        winner_evaluation > MaxEvalSanity) {
        return false;
    }

    return winner_evaluation - winner_material >= 260;
}

bool has_winning_compensation(
    std::int64_t winner_evaluation,
    std::int64_t winner_material,
    int total_material,
    const MaterialScore& material,
    bool winner_is_white
) {
    if (is_tiered_sacrifice(
            winner_evaluation,
            winner_material,
            total_material)) {
        return true;
    }

    return is_initiative_position(
        winner_evaluation,
        winner_material,
        total_material,
        material,
        winner_is_white
    );
}

bool has_durable_compensation(
    std::int64_t side_evaluation,
    std::int64_t side_material
) {
    return side_material < -120 &&
           side_evaluation >= 0 &&
           side_evaluation - side_material >= 350;
}

bool keep_position(
    std::int64_t side_evaluation,
    std::int64_t side_material,
    double side_result,
    int total_material,
    const MaterialScore& material,
    bool side_is_white
) {
    if (side_result >= DecisiveWin) {
        return has_winning_compensation(
            side_evaluation,
            side_material,
            total_material,
            material,
            side_is_white
        );
    }

    if (side_result <= DecisiveLoss) {
        return has_winning_compensation(
            -side_evaluation,
            -side_material,
            total_material,
            material,
            !side_is_white
        );
    }

    if (side_result < DrawnLow || side_result > DrawnHigh) {
        return false;
    }

    return has_durable_compensation(
               side_evaluation,
               side_material
           ) ||
           has_durable_compensation(
               -side_evaluation,
               -side_material
           );
}

class BufferedLineReader {
public:
    explicit BufferedLineReader(const std::string& path)
        : file_(std::fopen(path.c_str(), "rb")),
          buffer_(InputBufferBytes) {
        if (file_ != nullptr) {
            std::setvbuf(file_, nullptr, _IONBF, 0);
        }
    }

    ~BufferedLineReader() {
        if (file_ != nullptr) {
            std::fclose(file_);
        }
    }

    bool is_open() const {
        return file_ != nullptr;
    }

    bool failed() const {
        return failed_;
    }

    bool next(const char*& line, std::size_t& length) {
        for (;;) {
            if (begin_ < end_) {
                const void* newline_match = std::memchr(
                    buffer_.data() + begin_,
                    '\n',
                    end_ - begin_
                );

                if (newline_match != nullptr) {
                    const char* newline =
                        static_cast<const char*>(newline_match);

                    line = buffer_.data() + begin_;
                    length = static_cast<std::size_t>(newline - line);
                    begin_ = static_cast<std::size_t>(
                        newline - buffer_.data()
                    ) + 1;

                    return true;
                }
            }

            if (finished_) {
                if (begin_ < end_) {
                    line = buffer_.data() + begin_;
                    length = end_ - begin_;
                    begin_ = end_;
                    return true;
                }

                return false;
            }

            if (begin_ != 0) {
                const std::size_t remaining = end_ - begin_;

                std::memmove(
                    buffer_.data(),
                    buffer_.data() + begin_,
                    remaining
                );

                begin_ = 0;
                end_ = remaining;
            }

            if (end_ == buffer_.size()) {
                buffer_.resize(buffer_.size() * 2);
            }

            const std::size_t available = buffer_.size() - end_;
            const std::size_t bytes_read = std::fread(
                buffer_.data() + end_,
                1,
                available,
                file_
            );

            end_ += bytes_read;

            if (bytes_read < available) {
                if (std::ferror(file_) != 0) {
                    failed_ = true;
                    finished_ = true;
                } else if (std::feof(file_) != 0) {
                    finished_ = true;
                } else if (bytes_read == 0) {
                    finished_ = true;
                }
            }
        }
    }

private:
    std::FILE* file_{};
    std::vector<char> buffer_;
    std::size_t begin_{};
    std::size_t end_{};
    bool finished_{};
    bool failed_{};
};

class BufferedOutput {
public:
    explicit BufferedOutput(const std::string& path)
        : file_(std::fopen(path.c_str(), "wb")) {
        buffer_.reserve(OutputBufferBytes);

        if (file_ != nullptr) {
            std::setvbuf(file_, nullptr, _IONBF, 0);
        }
    }

    ~BufferedOutput() {
        if (file_ != nullptr) {
            if (!failed_) {
                flush();
            }

            std::fclose(file_);
        }
    }

    bool is_open() const {
        return file_ != nullptr;
    }

    bool append_line(const char* line, std::size_t length) {
        if (failed_ || file_ == nullptr) {
            return false;
        }

        if (length + 1 > OutputBufferBytes) {
            if (!flush()) {
                return false;
            }

            if (!write_bytes(line, length)) {
                return false;
            }

            return write_bytes("\n", 1);
        }

        if (buffer_.size() + length + 1 > OutputBufferBytes) {
            if (!flush()) {
                return false;
            }
        }

        buffer_.append(line, length);
        buffer_.push_back('\n');
        return true;
    }

    bool close() {
        if (file_ == nullptr) {
            return !failed_;
        }

        bool success = flush();

        if (std::fclose(file_) != 0) {
            success = false;
        }

        file_ = nullptr;
        failed_ = !success;
        return success;
    }

private:
    bool write_bytes(const char* data, std::size_t length) {
        if (length == 0) {
            return true;
        }

        if (std::fwrite(data, 1, length, file_) != length) {
            failed_ = true;
            return false;
        }

        return true;
    }

    bool flush() {
        if (failed_ || file_ == nullptr) {
            return false;
        }

        if (buffer_.empty()) {
            return true;
        }

        if (!write_bytes(buffer_.data(), buffer_.size())) {
            return false;
        }

        buffer_.clear();
        return true;
    }

    std::FILE* file_{};
    std::string buffer_;
    bool failed_{};
};
}

int filter(const std::string& input, const std::string& output) {
    BufferedLineReader reader(input);

    if (!reader.is_open()) {
        std::cerr << "Could not open input file: " << input << '\n';
        return 1;
    }

    BufferedOutput writer(output);

    if (!writer.is_open()) {
        std::cerr << "Could not open output file: " << output << '\n';
        return 1;
    }

    std::uint64_t total_lines = 0;
    std::uint64_t filtered_lines = 0;
    std::uint64_t skipped_lines = 0;

    const char* line = nullptr;
    std::size_t line_length = 0;

    while (reader.next(line, line_length)) {
        ++total_lines;

        ParsedLine parsed;

        if (!parse_line(line, line_length, parsed)) {
            ++skipped_lines;
            continue;
        }

        if (!result_can_be_kept(parsed.result)) {
            continue;
        }

        MaterialScore material;
        bool white_to_move = false;

        if (!extract_position_data(
                line,
                parsed.fen_end,
                material,
                white_to_move)) {
            ++skipped_lines;
            continue;
        }

        if (material.total < MinTotalMaterial) {
            continue;
        }

        const std::int64_t side_evaluation = white_to_move
            ? static_cast<std::int64_t>(parsed.evaluation)
            : -static_cast<std::int64_t>(parsed.evaluation);

        const std::int64_t side_material = white_to_move
            ? static_cast<std::int64_t>(material.balance)
            : -static_cast<std::int64_t>(material.balance);

        double side_result = parsed.result;

        if (ResultUsesWhitePerspective && !white_to_move) {
            side_result = 1.0 - side_result;
        }

        if (!keep_position(
                side_evaluation,
                side_material,
                side_result,
                material.total,
                material,
                white_to_move)) {
            continue;
        }

        if (!writer.append_line(line, line_length)) {
            std::cerr << "Write failed: " << output << '\n';
            return 1;
        }

        ++filtered_lines;
    }

    if (reader.failed()) {
        writer.close();
        std::cerr << "Read failed: " << input << '\n';
        return 1;
    }

    if (!writer.close()) {
        std::cerr << "Write failed: " << output << '\n';
        return 1;
    }

    std::cout << total_lines << " positions read in, "
              << filtered_lines << " filtered, "
              << skipped_lines << " skipped\n";

    return 0;
}

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

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
