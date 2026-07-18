#pragma once
#include "defs.h"
#include "position.h"
#include <cstdint>
#include <span>

namespace Generate {
inline constexpr uint8_t GenQuiets = 0;
inline constexpr uint8_t GenCaptures = 1;
inline constexpr uint8_t GenAll = 2;
} // namespace Generate

constexpr int TTMoveScore = 10000000;
constexpr int QueenPromoScore = 5000000;
constexpr int GoodCaptureBaseScore = 2000000;
constexpr int BadCaptureBaseScore = -2000000;
constexpr int KillerMoveScore = 100000;

template <uint8_t GenType, uint8_t Color>
inline void pawn_moves_color_impl(const Position &position,
                                  uint64_t check_filter,
                                  uint64_t empty_squares, Move *move_list,
                                  int &key) {

  constexpr uint64_t third_rank = Color ? Ranks[5] : Ranks[2];
  constexpr uint64_t seventh_rank = Color ? Ranks[1] : Ranks[6];
  constexpr int8_t dir = Color ? Directions::South : Directions::North;
  constexpr int8_t left = Color ? Directions::Southwest : Directions::Northwest;
  constexpr int8_t right = Color ? Directions::Southeast : Directions::Northeast;

  uint64_t our_promos = position.pieces_bb[PieceTypes::Pawn] &
                        position.colors_bb[Color] & seventh_rank;
  uint64_t our_non_promos = position.pieces_bb[PieceTypes::Pawn] &
                            position.colors_bb[Color] & (~seventh_rank);

  if constexpr (GenType != Generate::GenCaptures) {
    uint64_t move_1 = shift_pawns(our_non_promos, dir) & empty_squares;
    uint64_t move_2 =
        shift_pawns(move_1 & third_rank, dir) & empty_squares & check_filter;
    move_1 &= check_filter;

    while (move_1) {
      int to = pop_lsb(move_1);
      move_list[key++] = pack_move(to - (dir), to, MoveTypes::Normal);
    }
    while (move_2) {
      int to = pop_lsb(move_2);
      move_list[key++] = pack_move(to - (2 * dir), to, MoveTypes::Normal);
    }
  }

  if constexpr (GenType != Generate::GenQuiets) {
    uint64_t cap_left = shift_pawns(our_non_promos & ~Files[0], left) &
                        position.colors_bb[Color ^ 1] & check_filter;
    uint64_t cap_right = shift_pawns(our_non_promos & ~Files[7], right) &
                         position.colors_bb[Color ^ 1] & check_filter;

    while (cap_left) {
      int to = pop_lsb(cap_left);
      move_list[key++] = pack_move(to - (left), to, MoveTypes::Normal);
    }
    while (cap_right) {
      int to = pop_lsb(cap_right);
      move_list[key++] = pack_move(to - (right), to, MoveTypes::Normal);
    }

    if (position.ep_square != SquareNone) {
      uint64_t ep_captures =
          our_non_promos & PawnAttacks[Color ^ 1][position.ep_square];
      while (ep_captures) {
        int from = pop_lsb(ep_captures);
        move_list[key++] =
            pack_move(from, position.ep_square, MoveTypes::EnPassant);
      }
    }
  }

  uint64_t move_promo =
      shift_pawns(our_promos, dir) & empty_squares & check_filter;

  while (move_promo) {
    int to = pop_lsb(move_promo);
    for (int i = (GenType == Generate::GenCaptures ? 3 : 0);
         i < (GenType == Generate::GenQuiets ? 3 : 4); i++) {
      // if captures, only generate the queen promo
      // if notm generate all EXCEPT the queen promo

      move_list[key++] = pack_move_promo(to - (dir), to, i);
    }
  }

  if constexpr (GenType != Generate::GenQuiets) {
    uint64_t cap_left_promo = shift_pawns(our_promos & ~Files[0], left) &
                              position.colors_bb[Color ^ 1] & check_filter;
    uint64_t cap_right_promo = shift_pawns(our_promos & ~Files[7], right) &
                               position.colors_bb[Color ^ 1] & check_filter;

    while (cap_left_promo) {
      int to = pop_lsb(cap_left_promo);
      for (int i = 0; i < 4; i++) {
        move_list[key++] = pack_move_promo(to - (left), to, i);
      }
    }
    while (cap_right_promo) {
      int to = pop_lsb(cap_right_promo);
      for (int i = 0; i < 4; i++) {
        move_list[key++] = pack_move_promo(to - (right), to, i);
      }
    }
  }
}

template <uint8_t GenType>
inline void pawn_moves_impl(const Position &position, uint64_t check_filter,
                            uint64_t empty_squares, Move *move_list,
                            int &key) {
  if (position.color == Colors::Black) {
    pawn_moves_color_impl<GenType, Colors::Black>(
        position, check_filter, empty_squares, move_list, key);
  } else {
    pawn_moves_color_impl<GenType, Colors::White>(
        position, check_filter, empty_squares, move_list, key);
  }
}

inline void pawn_moves(const Position &position, uint64_t check_filter,
                       std::span<Move> move_list, int &key, int gen_type) {
  const uint64_t occupied =
      position.colors_bb[Colors::White] | position.colors_bb[Colors::Black];
  switch (gen_type) {
  case Generate::GenQuiets:
    pawn_moves_impl<Generate::GenQuiets>(position, check_filter, ~occupied,
                                         move_list.data(), key);
    break;
  case Generate::GenCaptures:
    pawn_moves_impl<Generate::GenCaptures>(position, check_filter, ~occupied,
                                           move_list.data(), key);
    break;
  default:
    pawn_moves_impl<Generate::GenAll>(position, check_filter, ~occupied,
                                      move_list.data(), key);
    break;
  }
}

template <uint8_t GenType, uint8_t Color>
inline int movegen_color_impl(const Position &position, Move *move_list,
                              uint64_t checkers) {

  constexpr int color = Color;
  constexpr int opp_color = Color ^ 1;
  const uint8_t king_pos = get_king_pos(position, color);
  int idx = 0;
  uint64_t stm_pieces = position.colors_bb[color],
           opp_pieces = position.colors_bb[opp_color];
  uint64_t occ = stm_pieces | opp_pieces;

  uint64_t targets = GenType == Generate::GenCaptures
                         ? opp_pieces
                         : (GenType == Generate::GenQuiets ? ~occ
                                                           : ~stm_pieces);
  uint64_t check_filter = ~0;

  uint64_t king = king_pos;
  uint64_t king_attacks = KingAttacks[king] & targets;
  while (king_attacks) {
    move_list[idx++] =
        pack_move(king_pos, pop_lsb(king_attacks), MoveTypes::Normal);
  }

  if (checkers) {
    if (checkers & (checkers - 1)) {
      return idx;
    }

    check_filter = BetweenBBs[king][get_lsb(checkers)];
  }

  pawn_moves_color_impl<GenType, Color>(position, check_filter, ~occ,
                                        move_list, idx);

  uint64_t knights = position.pieces_bb[PieceTypes::Knight] & stm_pieces;
  while (knights) {
    int from = pop_lsb(knights);
    uint64_t to = KnightAttacks[from] & targets & check_filter;
    while (to) {
      move_list[idx++] = pack_move(from, pop_lsb(to), MoveTypes::Normal);
    }
  }

  uint64_t diagonals = (position.pieces_bb[PieceTypes::Bishop] |
                        position.pieces_bb[PieceTypes::Queen]) &
                       stm_pieces;
  while (diagonals) {
    int from = pop_lsb(diagonals);
    uint64_t to = get_bishop_attacks(from, occ) & targets & check_filter;
    while (to) {
      move_list[idx++] = pack_move(from, pop_lsb(to), MoveTypes::Normal);
    }
  }

  uint64_t orthogonals = (position.pieces_bb[PieceTypes::Rook] |
                          position.pieces_bb[PieceTypes::Queen]) &
                         stm_pieces;
  while (orthogonals) {
    int from = pop_lsb(orthogonals);
    uint64_t to = get_rook_attacks(from, occ) & targets & check_filter;
    while (to) {
      move_list[idx++] = pack_move(from, pop_lsb(to), MoveTypes::Normal);
    }
  }

  if (checkers ||
      GenType ==
          Generate::GenCaptures) { // If we're in check there's no point in
                                   // seeing if we can castle (can be optimized)
    return idx;
  }
  // Requirements: the king and rook cannot have moved, all squares between them
  // must be empty, and the king can not go through check.
  // (It can't end up in check either, but that gets filtered just like any
  // illegal move.)

  for (int side : {Sides::Queenside, Sides::Kingside}) {

    if (position.castling_squares[color][side] == SquareNone) {
      continue;
    }

    int rook_target = 56 * color + 3 + 2 * side;
    int king_target = 56 * color + 2 + 4 * side;

    uint64_t castle_bb =
        BetweenBBs[position.castling_squares[color][side]][rook_target];
    castle_bb |= BetweenBBs[king_pos][king_target];
    castle_bb &=
        ~(1ull << king_pos) & ~(1ull << position.castling_squares[color][side]);

    if (occ & castle_bb) {
      continue;
    }
    bool invalid = false;

    if (king_target != king_pos) {
      int dir = (king_target > king_pos) ? 1 : -1;

      for (int i = king_pos + dir; i != king_target; i += dir) {
        if (attacks_square(position, i, opp_color)) {
          invalid = true;
          break;
        }
      }
    }

    if (!invalid) {
      move_list[idx++] =
          pack_move(king_pos, position.castling_squares[color][side],
                    MoveTypes::Castling);
    }
  }

  return idx;
}

template <uint8_t GenType>
inline int movegen_impl(const Position &position, Move *move_list,
                        uint64_t checkers) {
  if (position.color == Colors::Black) {
    return movegen_color_impl<GenType, Colors::Black>(position, move_list,
                                                      checkers);
  }
  return movegen_color_impl<GenType, Colors::White>(position, move_list,
                                                    checkers);
}

template <uint8_t GenType>
inline int movegen_dispatch(const Position &position,
                            Move *move_list, uint64_t checkers) {
  return movegen_impl<GenType>(position, move_list, checkers);
}

inline int movegen(const Position &position, std::span<Move> move_list,
                   uint64_t checkers, int gen_type) {
  switch (gen_type) {
  case Generate::GenQuiets:
    return movegen_dispatch<Generate::GenQuiets>(position, move_list.data(),
                                                 checkers);
  case Generate::GenCaptures:
    return movegen_dispatch<Generate::GenCaptures>(position, move_list.data(),
                                                   checkers);
  default:
    return movegen_dispatch<Generate::GenAll>(position, move_list.data(),
                                              checkers);
  }
}

template <uint8_t Color>
inline bool is_legal_fast_color(const Position &position, Move move) {
  const int from = extract_from(move);
  int to = extract_to(move);
  constexpr int opp_color = Color ^ 1;
  const uint64_t occupied =
      position.colors_bb[Colors::White] | position.colors_bb[Colors::Black];
  const uint64_t from_bb = 1ull << from;

  if (position.board[from] >= Pieces::WKing) {
    if (extract_type(move) == MoveTypes::Castling) {
      to = 56 * Color + 2 + (to > from) * 4;
    }
    const uint64_t king_occ = occupied ^ from_bb;
    const uint64_t enemy = position.colors_bb[opp_color];
    const uint64_t bishops = position.pieces_bb[PieceTypes::Bishop] |
                             position.pieces_bb[PieceTypes::Queen];
    const uint64_t rooks = position.pieces_bb[PieceTypes::Rook] |
                           position.pieces_bb[PieceTypes::Queen];
    const uint64_t attackers =
        (PawnAttacks[Color][to] & position.pieces_bb[PieceTypes::Pawn]) |
        (KnightAttacks[to] & position.pieces_bb[PieceTypes::Knight]) |
        (get_bishop_attacks(to, king_occ) & bishops) |
        (get_rook_attacks(to, king_occ) & rooks) |
        (KingAttacks[to] & position.pieces_bb[PieceTypes::King]);
    return !(attackers & enemy);
  }

  const int king_pos = get_king_pos(position, Color);
  uint64_t occ = occupied ^ from_bb;
  const uint64_t to_bb = 1ull << to;

  if (extract_type(move) == MoveTypes::EnPassant) {
    const int cap_square =
        to + (Color ? Directions::North : Directions::South);
    occ ^= to_bb;
    occ ^= 1ull << cap_square;
  } else if (!position.board[to]) {
    occ ^= to_bb;
  }

  const uint64_t bishops = position.pieces_bb[PieceTypes::Bishop] |
                           position.pieces_bb[PieceTypes::Queen];
  const uint64_t rooks = position.pieces_bb[PieceTypes::Rook] |
                         position.pieces_bb[PieceTypes::Queen];
  const uint64_t slider_attackers =
      (get_bishop_attacks(king_pos, occ) & bishops) |
      (get_rook_attacks(king_pos, occ) & rooks);

  return !(slider_attackers & position.colors_bb[opp_color] & ~to_bb);
}

inline bool is_legal_fast_unchecked(const Position &position, Move move) {
  if (position.color == Colors::Black) {
    return is_legal_fast_color<Colors::Black>(position, move);
  }
  return is_legal_fast_color<Colors::White>(position, move);
}

inline bool is_legal_fast(const Position &position, Move move) {
  if (move == MoveNone) {
    return false;
  }
  return is_legal_fast_unchecked(position, move);
}

// Not to be used in performance critical areas
int legal_movegen(const Position &position, std::span<Move> move_list) {
  uint64_t checkers = attacks_square(
      position, get_king_pos(position, position.color), position.color ^ 1);
  std::array<Move, ListSize> pseudo_list;
  int pseudo_nmoves =
      movegen_dispatch<Generate::GenAll>(position, pseudo_list.data(),
                                         checkers);

  int legal_nmoves = 0;
  for (int i = 0; i < pseudo_nmoves; i++) {
    if (is_legal_fast_unchecked(position, pseudo_list[i]))
      move_list[legal_nmoves++] = pseudo_list[i];
  }

  return legal_nmoves;
}

template <uint8_t Color>
inline bool SEE_color(const Position &position, Move move, int threshold) {

  int stm = Color, from = extract_from(move), to = extract_to(move);

  int gain = SeeValues[get_piece_type(position.board[to])] - threshold;
  if (gain < 0) {
    // If taking the piece isn't good enough return
    return false;
  }

  gain -= SeeValues[get_piece_type(position.board[from])];
  if (gain >= 0) {
    return true;
  }

  // Store bishops and rooks here to more quickly determine later new revealed
  // attackers
  uint64_t bishops = position.pieces_bb[PieceTypes::Bishop] |
                     position.pieces_bb[PieceTypes::Queen];
  uint64_t rooks = position.pieces_bb[PieceTypes::Rook] |
                   position.pieces_bb[PieceTypes::Queen];

  uint64_t occ =
      (position.colors_bb[Colors::White] | position.colors_bb[Colors::Black]) -
      (1ull << from);

  uint64_t all_attackers = attacks_square(position, to, occ);

  while (true) {
    stm ^= 1;

    all_attackers &= occ;
    uint64_t stm_attackers = all_attackers & position.colors_bb[stm];

    if (!stm_attackers) {
      return stm != Color;
    }

    // find cheapest attacker
    int attackerType = PieceTypes::PieceNone;

    for (int pt = PieceTypes::Pawn; pt <= PieceTypes::King; pt++) {
      uint64_t match = stm_attackers & position.pieces_bb[pt];
      if (match) {
        occ -= get_lsb_bb(match);
        attackerType = pt;
        break;
      }
    }

    // A new slider behind might be revealed
    if (attackerType == PieceTypes::Pawn ||
        attackerType == PieceTypes::Bishop ||
        attackerType == PieceTypes::Queen) {
      all_attackers |= get_bishop_attacks(to, occ) & bishops;
    }
    if (attackerType == PieceTypes::Rook || attackerType == PieceTypes::Queen) {
      all_attackers |= get_rook_attacks(to, occ) & rooks;
    }

    gain = -gain - SeeValues[attackerType] - 1;
    if (gain >= 0) {
      return stm == Color;
    }
  }

  return true;
}

inline bool SEE(const Position &position, Move move, int threshold) {
  if (position.color == Colors::Black) {
    return SEE_color<Colors::Black>(position, move, threshold);
  }
  return SEE_color<Colors::White>(position, move, threshold);
}

void score_moves(Position &position, ThreadInfo &thread_info,
                 MoveInfo &scored_moves, Move tt_move, int len) {

  // score the moves

  int ply = thread_info.search_ply;

  int their_last =
      ply < 1
          ? MoveNone
          : extract_to(
                thread_info.game_hist[thread_info.game_ply - 1].played_move);
  int their_piece =
      ply < 1 ? Pieces::Blank
              : thread_info.game_hist[thread_info.game_ply - 1].piece_moved;

  int our_last =
      ply < 2
          ? MoveNone
          : extract_to(
                thread_info.game_hist[thread_info.game_ply - 2].played_move);
  int our_piece =
      ply < 2 ? Pieces::Blank
              : thread_info.game_hist[thread_info.game_ply - 2].piece_moved;

  for (int idx = 0; idx < len; idx++) {
    Move move = scored_moves.moves[idx];
    if (move == tt_move) {
      scored_moves.scores[idx] = TTMoveScore;
      // TT move score;
    }

    else if (extract_promo(move) == Promos::Queen) {
      // Queen promo score
      scored_moves.scores[idx] = QueenPromoScore;
    }

    else if (is_cap(position, move)) {
      // Capture score
      int from = extract_from(move), to = extract_to(move);
      int from_piece = position.board[from], to_piece = position.board[to];

      scored_moves.scores[idx] = GoodCaptureBaseScore +
                                 SeeValues[get_piece_type(to_piece)] * 100 -
                                 SeeValues[get_piece_type(from_piece)] / 100 -
                                 TTMoveScore * !SEE(position, move, -107);

      scored_moves.scores[idx] +=
          thread_info.CapHistScores[from_piece][to];

    }

    else if (move == thread_info.KillerMoves[thread_info.search_ply]) {
      // Killer move score
      scored_moves.scores[idx] = KillerMoveScore;
    }

    else {
      // Normal moves are scored using history
      int piece = position.board[extract_from(move)], to = extract_to(move);
      scored_moves.scores[idx] = thread_info.HistoryScores[piece][to];

      if (ply > 0 && their_last != MoveNone) {
        scored_moves.scores[idx] +=
            thread_info.ContHistScores[their_piece][their_last][piece][to];
      }
      if (ply > 1 && our_last != MoveNone) {
        scored_moves.scores[idx] +=
            thread_info.ContHistScores[our_piece][our_last][piece][to];
      }
    }
  }
}

Move get_next_move(Move *moves, int *scores, int start_idx, int len) {
  // Performs a selection sort
  int best_idx = start_idx, best_score = scores[start_idx];
  for (int i = start_idx + 1; i < len; i++) {
    if (scores[i] > best_score) {
      best_score = scores[i];
      best_idx = i;
    }
  }
  std::swap(moves[start_idx], moves[best_idx]);
  std::swap(scores[start_idx], scores[best_idx]);

  return moves[start_idx];
}

inline Move get_next_move(std::span<Move> moves, std::span<int> scores,
                          int start_idx, int len) {
  return get_next_move(moves.data(), scores.data(), start_idx, len);
}
