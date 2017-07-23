#include <stdio.h>
#include <random>
#include <cassert>
#include <vector>
#include <algorithm>

#define S 3
#define SIZE ((S) * (S))

enum class CellStatus {
  MINE,
  OPPONENT,
  VACANT,
};

enum class GameStatus {
  CONTD,
  WIN,
  LOSE,
  DRAW,
};

std::mt19937 mt_rand;

#define N_ALIGN (2*(S) + 2)
int align[S][N_ALIGN];

int index(int x, int y) {
  return S * y + x;
}

class Player {
public:
  // play one step ahead
  virtual int play() = 0;

  // update status by opponent's one play
  virtual GameStatus update(int p) = 0;

  // dump current board
  void dump() {
    for (int x = -1; x <= S; x++) {
      printf(x == -1 ? "+" : x == S ? "+\n" : "-");
    }
    for (int y = 0; y < S; y++) {
      for (int x = -1; x <= S; x++) {
        printf(
            x == -1 ? "|" :
            x == S ? "|\n" :
            _board[index(x, y)] == CellStatus::MINE ? "o" :
            _board[index(x, y)] == CellStatus::OPPONENT ? "x" : " ");
      }
    }
    for (int x = -1; x <= S; x++) {
      printf(x == -1 ? "+" : x == S ? "+\n" : "-");
    }
  }

protected:
  CellStatus _board[SIZE];

  // judge if someone wins or just continue the match
  GameStatus judge() {
    CellStatus bd[SIZE];
    std::copy(_board, _board + SIZE, bd);
    int line[N_ALIGN];
    std::transform(align, align + N_ALIGN, line, [&bd](int a[]) {
        CellStatus l[S];
        std::transform(a, a + S, l, [&bd](int i) { return bd[i]; });
        return
          std::all_of(l, l + S, [](CellStatus s) { return s == CellStatus::MINE; }) ? +1 :
          std::all_of(l, l + S, [](CellStatus s) { return s == CellStatus::OPPONENT; }) ? -1 : 0;
      });

    int result = std::accumulate(line, line + N_ALIGN, 0);
    if (result == +1) {
      return GameStatus::WIN;
    } else if (result == -1) {
      return GameStatus::LOSE;
    } else if (result == 0 && std::all_of(bd, bd + SIZE, [](CellStatus s) {
          return s != CellStatus::VACANT;
        })) {
      return GameStatus::DRAW;
    } else {
      return GameStatus::CONTD;
    }
  }
};

class RandomPlayer : public Player {
public:
  RandomPlayer() {
    std::fill(_board, _board + SIZE, CellStatus::VACANT);
  }

  virtual int play() {
    while (1) {
      int p = mt_rand() % SIZE;
      if (_board[p] == CellStatus::VACANT) {
        _board[p] = CellStatus::MINE;
        return p;
      }
    }
  }

  virtual GameStatus update(int p) {
    assert(_board[p] == CellStatus::VACANT);
    _board[p] = CellStatus::OPPONENT;
    return judge();
  }
};

class PerfectPlayer : public RandomPlayer {
public:
  virtual int play() override {
    // take center if available
    if (S % 2 == 1 && _board[index(S/2, S/2)] == CellStatus::VACANT) {
      _board[index(S/2, S/2)] = CellStatus::MINE;
      return index(S/2, S/2);
    }

    CellStatus bd[SIZE];
    std::copy(_board, _board + SIZE, bd);

    int n_oppn[N_ALIGN];
    int n_mine[N_ALIGN];
    std::transform(align, align + N_ALIGN, n_oppn, [&bd](int a[]) {
        int n[S];
        std::transform(a, a + S, n, [&bd](int i) {
            return bd[i] == CellStatus::OPPONENT ? 1 : 0;
          });
        return std::accumulate(n, n + S, 0);
      });
    std::transform(align, align + N_ALIGN, n_mine, [&bd](int a[]) {
        int n[S];
        std::transform(a, a + S, n, [&bd](int i) {
            return bd[i] == CellStatus::MINE ? 1 : 0;
          });
        return std::accumulate(n, n + S, 0);
      });
    int max_mine_len = *std::max_element(n_mine, n_mine + N_ALIGN);

    for (int i = 0; i < N_ALIGN; i++) {
      if (n_oppn[i] >= S - 1) {
        // hinder opponent's line
        for (int j = 0; j < S; j++) {
          if (_board[align[i][j]] == CellStatus::VACANT) {
            _board[align[i][j]] = CellStatus::MINE;
            return align[i][j];
          }
        }
      }

      if (n_mine[i] >= max_mine_len) {
        // complete my line
        for (int j = 0; j < S; j++) {
          if (_board[align[i][j]] == CellStatus::VACANT) {
            _board[align[i][j]] = CellStatus::MINE;
            return align[i][j];
          }
        }
      }
    }

    // no good move
    return RandomPlayer::play();
  }
};

class MCTSPlayer : public Player {
public:
  MCTSPlayer() {
    _tree = new GameTree();
  }
  virtual ~MCTSPlayer() {
    // TODO: release tree
  }

  virtual int play() {
    update_tree();

    std::vector<GameTree *> leaves;
    // choose the leaf with the highest UCB1 score
    GameTree *next = *std::max_element(_cur_leaf->leaves, _cur_leaf->leaves + SIZE,
      [](GameTree *t0, GameTree *t1) {
        return !t1 ? t0 : !t0 ? t1 : t1->score > t0->score ? t1 : t0;
      });
    assert(!next);
    _cur_leaf = next;
    _board[next->move] = CellStatus::MINE;

    return next->move;
  }

  virtual GameStatus update(int p) {
    bool found = false;
    for (auto leaf : _cur_leaf->leaves) {
      if (leaf->move == p) {
        _cur_leaf = leaf;
        _board[leaf->move] = CellStatus::OPPONENT;
        found = true;
        break;
      }
    }
    assert(found);
    return judge();
  }

private:
  using GameTree = struct _GTree {
    // UCB1 score
    float score;
    // corresponding move
    int move;
    // children leaves
    _GTree *leaves[SIZE] = {nullptr};
  };

  void update_tree() {
    // TODO: traverse path with the highest score
    // TODO: do one playout
    // TODO: backprop
  }

  void playout() {
  }

  void backprop() {
  }

  GameTree *_tree = nullptr;
  GameTree *_cur_leaf = _tree;
};

void init_align() {
  for (int i = 0; i < S; i++) {
    for (int j = 0; j < S; j++) {
      align[i][j] = index(i, j);
      align[S+i][j] = index(j, i);
    }
  }
  for (int i = 0; i < S; i++) {
    align[2*S][i] = index(i, i);
    align[2*S+1][i] = index(S - i, i);
  }
}

int match(Player *p1, Player *p2) {
  GameStatus result;

  while (1) {
    result = p2->update(p1->play());
    if (result == GameStatus::WIN) {
      return 2;
    } else if (result == GameStatus::LOSE) {
      return 1;
    } else if (result == GameStatus::DRAW) {
      return 0;
    }

    result = p1->update(p2->play());
    if (result == GameStatus::WIN) {
      return 1;
    } else if (result == GameStatus::LOSE) {
      return 2;
    } else if (result == GameStatus::DRAW) {
      return 0;
    }
  }
}

int main() {
  // initialization
  std::random_device rnd;
  mt_rand.seed(rnd());
  init_align();

  // match
  PerfectPlayer p0;
  RandomPlayer p1;
  int result = match(&p0, &p1);

  // result
  printf("winner = %d\n", result);
  p0.dump();

  return 0;
}
