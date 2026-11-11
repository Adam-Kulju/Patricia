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
constexpr int KnightPromoCheckScore = 1900000;
constexpr int BadCaptureBaseScore = -2000000;
constexpr int UnderPromoScore = -3000000;
constexpr int KillerMoveScore = 100000;
constexpr int SacrificeCheckScore = 80000;
constexpr int KingZoneSacBonus = 600000;
constexpr int CaptureCheckBonus = 65536;
constexpr int CaptureKingZoneBonus = 16384;
constexpr int QuietCheckBonus = 16384;
constexpr int QuietKingZoneBonus = 4096;

template <uint8_t GenType, uint8_t Color>
inline void pawn_moves_color_impl(const Position &position,
                                  uint64_t check_filter, uint64_t empty_squares,
                                  Move *move_list, int &key) {

  constexpr uint64_t third_rank = Color ? Ranks[5] : Ranks[2];
  constexpr uint64_t seventh_rank = Color ? Ranks[1] : Ranks[6];
  constexpr int8_t dir = Color ? Directions::South : Directions::North;
  constexpr int8_t left = Color ? Directions::Southwest : Directions::Northwest;
  constexpr int8_t right =
      Color ? Directions::Southeast : Directions::Northeast;

  // if captures, only generate the queen promo
  // if not, generate all EXCEPT the queen promo
  constexpr int promo_start =
      (GenType == Generate::GenCaptures) ? Promos::Queen : Promos::Knight;
  constexpr int promo_end =
      (GenType == Generate::GenQuiets) ? Promos::Queen : Promos::Queen + 1;

  const uint64_t our_pawns =
      position.pieces_bb[PieceTypes::Pawn] & position.colors_bb[Color];
  const uint64_t opp_pieces = position.colors_bb[Color ^ 1];
  const uint64_t our_promos = our_pawns & seventh_rank;
  const uint64_t our_non_promos = our_pawns ^ our_promos;

  if constexpr (GenType != Generate::GenCaptures) {
    uint64_t move_1 = shift_pawns(our_non_promos, dir) & empty_squares;
    uint64_t move_2 =
        shift_pawns(move_1 & third_rank, dir) & empty_squares & check_filter;
    move_1 &= check_filter;

    while (move_1) {
      int to = pop_lsb(move_1);
      move_list[key++] = pack_move(to - dir, to, MoveTypes::Normal);
    }
    while (move_2) {
      int to = pop_lsb(move_2);
      move_list[key++] = pack_move(to - 2 * dir, to, MoveTypes::Normal);
    }
  }

  if constexpr (GenType != Generate::GenQuiets) {
    const uint64_t cap_targets = opp_pieces & check_filter;
    uint64_t cap_left =
        shift_pawns(our_non_promos & ~Files[0], left) & cap_targets;
    uint64_t cap_right =
        shift_pawns(our_non_promos & ~Files[7], right) & cap_targets;

    while (cap_left) {
      int to = pop_lsb(cap_left);
      move_list[key++] = pack_move(to - left, to, MoveTypes::Normal);
    }
    while (cap_right) {
      int to = pop_lsb(cap_right);
      move_list[key++] = pack_move(to - right, to, MoveTypes::Normal);
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

  if (!our_promos) {
    return;
  }

  uint64_t move_promo =
      shift_pawns(our_promos, dir) & empty_squares & check_filter;

  while (move_promo) {
    int to = pop_lsb(move_promo);
    int from = to - dir;
    for (int i = promo_start; i < promo_end; i++) {
      move_list[key++] = pack_move_promo(from, to, i);
    }
  }

  if constexpr (GenType != Generate::GenQuiets) {
    const uint64_t cap_targets = opp_pieces & check_filter;
    uint64_t cap_left_promo =
        shift_pawns(our_promos & ~Files[0], left) & cap_targets;
    uint64_t cap_right_promo =
        shift_pawns(our_promos & ~Files[7], right) & cap_targets;

    while (cap_left_promo) {
      int to = pop_lsb(cap_left_promo);
      int from = to - left;
      for (int i = Promos::Knight; i <= Promos::Queen; i++) {
        move_list[key++] = pack_move_promo(from, to, i);
      }
    }
    while (cap_right_promo) {
      int to = pop_lsb(cap_right_promo);
      int from = to - right;
      for (int i = Promos::Knight; i <= Promos::Queen; i++) {
        move_list[key++] = pack_move_promo(from, to, i);
      }
    }
  }
}

template <uint8_t GenType>
inline void pawn_moves_impl(const Position &position, uint64_t check_filter,
                            uint64_t empty_squares, Move *move_list, int &key) {
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
  constexpr int back_rank = 56 * Color;
  const int king_pos = get_king_pos(position, color);
  int idx = 0;
  const uint64_t stm_pieces = position.colors_bb[color],
                 opp_pieces = position.colors_bb[opp_color];
  const uint64_t occ = stm_pieces | opp_pieces;

  const uint64_t targets =
      GenType == Generate::GenCaptures
          ? opp_pieces
          : (GenType == Generate::GenQuiets ? ~occ : ~stm_pieces);
  uint64_t check_filter = ~0ull;

  uint64_t king_attacks = KingAttacks[king_pos] & targets;
  while (king_attacks) {
    move_list[idx++] =
        pack_move(king_pos, pop_lsb(king_attacks), MoveTypes::Normal);
  }

  if (checkers) {
    if (checkers & (checkers - 1)) {
      return idx;
    }

    check_filter = BetweenBBs[king_pos][get_lsb(checkers)];
  }

  pawn_moves_color_impl<GenType, Color>(position, check_filter, ~occ, move_list,
                                        idx);

  const uint64_t piece_targets = targets & check_filter;

  uint64_t knights = position.pieces_bb[PieceTypes::Knight] & stm_pieces;
  while (knights) {
    int from = pop_lsb(knights);
    uint64_t to = KnightAttacks[from] & piece_targets;
    while (to) {
      move_list[idx++] = pack_move(from, pop_lsb(to), MoveTypes::Normal);
    }
  }

  uint64_t diagonals = (position.pieces_bb[PieceTypes::Bishop] |
                        position.pieces_bb[PieceTypes::Queen]) &
                       stm_pieces;
  while (diagonals) {
    int from = pop_lsb(diagonals);
    uint64_t to = get_bishop_attacks(from, occ) & piece_targets;
    while (to) {
      move_list[idx++] = pack_move(from, pop_lsb(to), MoveTypes::Normal);
    }
  }

  uint64_t orthogonals = (position.pieces_bb[PieceTypes::Rook] |
                          position.pieces_bb[PieceTypes::Queen]) &
                         stm_pieces;
  while (orthogonals) {
    int from = pop_lsb(orthogonals);
    uint64_t to = get_rook_attacks(from, occ) & piece_targets;
    while (to) {
      move_list[idx++] = pack_move(from, pop_lsb(to), MoveTypes::Normal);
    }
  }

  if constexpr (GenType == Generate::GenCaptures) {
    return idx;
  }

  if (checkers) {
    return idx;
  }

  const uint64_t king_bb = 1ull << king_pos;

  for (int side = Sides::Queenside; side <= Sides::Kingside; side++) {

    const int rook_from = position.castling_squares[color][side];
    if (rook_from == SquareNone) {
      continue;
    }

    const int rook_target = back_rank + 3 + 2 * side;
    const int king_target = back_rank + 2 + 4 * side;

    uint64_t castle_bb = BetweenBBs[rook_from][rook_target] |
                         BetweenBBs[king_pos][king_target];
    castle_bb &= ~king_bb & ~(1ull << rook_from);

    if (occ & castle_bb) {
      continue;
    }
    bool invalid = false;

    if (king_target != king_pos) {
      const int dir = (king_target > king_pos) ? 1 : -1;

      for (int i = king_pos + dir; i != king_target + dir; i += dir) {
        if (attacks_square(position, i, opp_color)) {
          invalid = true;
          break;
        }
      }
    }

    if (!invalid) {
      move_list[idx++] =
          pack_move(king_pos, rook_from, MoveTypes::Castling);
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
inline int movegen_dispatch(const Position &position, Move *move_list,
                            uint64_t checkers) {
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
  const uint64_t bishops = position.pieces_bb[PieceTypes::Bishop] |
                           position.pieces_bb[PieceTypes::Queen];
  const uint64_t rooks = position.pieces_bb[PieceTypes::Rook] |
                         position.pieces_bb[PieceTypes::Queen];

  if (position.board[from] >= Pieces::WKing) {
    uint64_t king_occ = occupied ^ from_bb;
    if (extract_type(move) == MoveTypes::Castling) {
      king_occ ^= 1ull << to;
      to = 56 * Color + 2 + (to > from) * 4;
    }
    const uint64_t enemy = position.colors_bb[opp_color];

    if (PawnAttacks[Color][to] & position.pieces_bb[PieceTypes::Pawn] & enemy) {
      return false;
    }
    if (KnightAttacks[to] & position.pieces_bb[PieceTypes::Knight] & enemy) {
      return false;
    }
    if (KingAttacks[to] & position.pieces_bb[PieceTypes::King] & enemy) {
      return false;
    }
    if (get_bishop_attacks(to, king_occ) & bishops & enemy) {
      return false;
    }
    return !(get_rook_attacks(to, king_occ) & rooks & enemy);
  }

  const int king_pos = get_king_pos(position, Color);
  const uint64_t to_bb = 1ull << to;
  uint64_t occ = (occupied ^ from_bb) | to_bb;
  uint64_t enemy = position.colors_bb[opp_color] & ~to_bb;

  if (extract_type(move) == MoveTypes::EnPassant) {
    const uint64_t cap_bb =
        1ull << (to + (Color ? Directions::North : Directions::South));
    occ ^= cap_bb;
    enemy &= ~cap_bb;
  }

  if (get_bishop_attacks(king_pos, occ) & bishops & enemy) {
    return false;
  }
  return !(get_rook_attacks(king_pos, occ) & rooks & enemy);
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
inline int legal_movegen(const Position &position, std::span<Move> move_list) {
  uint64_t checkers = attacks_square(
      position, get_king_pos(position, position.color), position.color ^ 1);
  std::array<Move, ListSize> pseudo_list;
  int pseudo_nmoves = movegen_dispatch<Generate::GenAll>(
      position, pseudo_list.data(), checkers);

  int legal_nmoves = 0;
  for (int i = 0; i < pseudo_nmoves; i++) {
    if (is_legal_fast_unchecked(position, pseudo_list[i]))
      move_list[legal_nmoves++] = pseudo_list[i];
  }

  return legal_nmoves;
}

template <uint8_t Color>
inline bool SEE_color(const Position &position, Move move, int threshold) {

  int stm = Color;
  const int from = extract_from(move), to = extract_to(move);

  int attacker_type = get_piece_type(position.board[from]);
  int gain = SeeValues[get_piece_type(position.board[to])] - threshold;

  uint64_t occ =
      (position.colors_bb[Colors::White] | position.colors_bb[Colors::Black]) ^
      (1ull << from);

  if (extract_type(move) == MoveTypes::EnPassant) {
    gain = SeeValues[PieceTypes::Pawn] - threshold;
    occ ^= 1ull << (to + (Color ? Directions::North : Directions::South));
  } else if (attacker_type == PieceTypes::Pawn && (to < 8 || to >= 56)) {
    attacker_type = extract_promo(move) + PieceTypes::Knight;
    gain += SeeValues[attacker_type] - SeeValues[PieceTypes::Pawn];
  }

  if (gain < 0) {
    return false;
  }

  gain -= SeeValues[attacker_type];
  if (gain >= 0) {
    return true;
  }

  const uint64_t bishops = position.pieces_bb[PieceTypes::Bishop] |
                           position.pieces_bb[PieceTypes::Queen];
  const uint64_t rooks = position.pieces_bb[PieceTypes::Rook] |
                         position.pieces_bb[PieceTypes::Queen];

  uint64_t all_attackers = attacks_square(position, to, occ);

  while (true) {
    stm ^= 1;

    all_attackers &= occ;
    const uint64_t stm_attackers = all_attackers & position.colors_bb[stm];

    if (!stm_attackers) {
      return stm != Color;
    }

    uint64_t match;
    int attackerType;

    if ((match = stm_attackers & position.pieces_bb[PieceTypes::Pawn])) {
      attackerType = PieceTypes::Pawn;
    } else if ((match =
                    stm_attackers & position.pieces_bb[PieceTypes::Knight])) {
      attackerType = PieceTypes::Knight;
    } else if ((match =
                    stm_attackers & position.pieces_bb[PieceTypes::Bishop])) {
      attackerType = PieceTypes::Bishop;
    } else if ((match = stm_attackers & position.pieces_bb[PieceTypes::Rook])) {
      attackerType = PieceTypes::Rook;
    } else if ((match =
                    stm_attackers & position.pieces_bb[PieceTypes::Queen])) {
      attackerType = PieceTypes::Queen;
    } else {
      if (all_attackers & position.colors_bb[stm ^ 1]) {
        return stm != Color;
      }
      match = stm_attackers;
      attackerType = PieceTypes::King;
    }

    occ ^= get_lsb_bb(match);

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

inline Move get_next_move(Move *moves, int *scores, int start_idx, int len) {
  // Performs a selection sort
  int best_idx = start_idx, best_score = scores[start_idx];
  for (int i = start_idx + 1; i < len; i++) {
    if (scores[i] > best_score) {
      best_score = scores[i];
      best_idx = i;
    }
  }
  if (best_idx != start_idx) {
    scores[best_idx] = scores[start_idx];
    scores[start_idx] = best_score;
    std::swap(moves[start_idx], moves[best_idx]);
  }

  return moves[start_idx];
}

inline Move get_next_move(std::span<Move> moves, std::span<int> scores,
                          int start_idx, int len) {
  return get_next_move(moves.data(), scores.data(), start_idx, len);
}
