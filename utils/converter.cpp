#include <stdio.h>
#include <fstream>
#include <stdint.h>
#include <cstring>

using namespace  std;

struct bulletformat{
    uint64_t occ;
    uint8_t pieces[16];
    int16_t score;
    uint8_t result;
    uint8_t ksq;
    uint8_t opp_ksq;
};


string export_fen(int board[64]) { // From an internal board state, print
                                        // an FEN for datagen purposes.
  int pos = 56;
  string fen;
  int fenkey = 0;
  while (pos >= 0) {
    //printf("%i\n", pos);
    if (board[pos]) {

      switch (board[pos]) {
      case 1:
        fen.append("P");
        break;
      case 2:
        fen.append("N");
        break;
      case 3:
        fen.append("B");
        break;
      case 4:
        fen.append("R");
        break;
      case 5:
        fen.append("Q");
        break;
      case 6:
        fen.append("K");
        break;
      case 9:
        fen.append("p");
        break;
      case 10:
        fen.append("n");
        break;
      case 11:
        fen.append("b");
        break;
      case 12:
        fen.append("r");
        break;
      case 13:
        fen.append("q");
        break;
      case 14:
        fen.append("k");
        break;
      default:
        printf("Error parsing board! ");
        printf("%i\n", board[pos]);
        return "error";
      }
    } else {
      int empty = 1;
      pos++;
      while ((pos % 8 != 0) && !board[pos]) {
        empty++;
        pos++;
      }

      fen.append(to_string(empty));
      pos--;
    }

    if (pos % 8 == 7) {
      pos -= 16;
      if (pos >= -1) {
        fen.append("/");
      }
      
    }
    pos++;
  }

  string n = " w - - 0 0";

  return fen + n;
}


int main(int argc, char *argv[]){
    string i = argv[1];
    string o = argv[2];

    ifstream in(i);
    ofstream out(o);
    string f2;

    bulletformat temp;
    uint32_t lines_read = 0;

    while (in.read((char*) &temp, sizeof(bulletformat))){
    lines_read++;
    if (lines_read % 10000000 == 0){
        printf("%i lines read\n", lines_read);
    }
    uint64_t occ  = temp.occ;
    int board[64] = {0};

    int idx = 0;
    for (int sq = 0; occ; sq++, occ >>= 1){
        int lsb = occ & 1;
        if (lsb){
            int piece = (idx % 2) ? (temp.pieces[idx / 2] / 16) : (temp.pieces[idx / 2] & 15);
            board[sq] = piece + 1;
            idx++;
        }
    }

    string f = export_fen(board);
    if (f == "error"){
        printf("%i\n", lines_read);
        exit(0);
    }
    string result = to_string((float)temp.result / 2).substr(0, 3);
    f.append(" | " + to_string(temp.score) + " | " + result);
    out << f << endl;
    
    }
    
}