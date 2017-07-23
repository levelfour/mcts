#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <random>
#include <cassert>
#include <cmath>
#include <vector>
#include <stack>
#include <queue>
#include <algorithm>

#define S 3
#define SIZE ((S) * (S))

#define debug_log(...) {if (verbose) fprintf(stderr, __VA_ARGS__);}

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

bool verbose = false;
std::mt19937 mt_rand;

#define N_ALIGN (2*(S) + 2)
int align[S][N_ALIGN];

int index(int x, int y) {
  return S * y + x;
}

class Player {
public:
  Player() {
    std::fill(_board, _board + SIZE, CellStatus::VACANT);
  }

  virtual ~Player() {};

  // play one step ahead
  virtual int play() = 0;

  // update status by opponent's one play
  virtual GameStatus update(int p) = 0;

  // dump current board
  void dump() {
    dump_board(_board);
  }

protected:
  CellStatus _board[SIZE];

  // judge if someone wins or just continue the match
  GameStatus judge(CellStatus board[]) {
    CellStatus bd[SIZE];
    std::copy(board, board + SIZE, bd);
    int line[N_ALIGN];
    std::transform(align, align + N_ALIGN, line, [&bd](int a[]) {
        CellStatus l[S];
        std::transform(a, a + S, l, [&bd](int i) { return bd[i]; });
        return
          std::all_of(l, l + S, [](CellStatus s) { return s == CellStatus::MINE; }) ? +1 :
          std::all_of(l, l + S, [](CellStatus s) { return s == CellStatus::OPPONENT; }) ? -1 : 0;
      });

    int result = std::accumulate(line, line + N_ALIGN, 0);
    if (result > 0) {
      return GameStatus::WIN;
    } else if (result < 0) {
      return GameStatus::LOSE;
    } else if (result == 0 && std::all_of(bd, bd + SIZE, [](CellStatus s) {
          return s != CellStatus::VACANT;
        })) {
      return GameStatus::DRAW;
    } else {
      return GameStatus::CONTD;
    }
  }

  void dump_board(CellStatus board[]) {
    for (int x = -1; x <= S; x++) {
      printf(x == -1 ? "+" : x == S ? "+\n" : "-");
    }
    for (int y = 0; y < S; y++) {
      for (int x = -1; x <= S; x++) {
        printf(
            x == -1 ? "|" :
            x == S ? "|\n" :
            board[index(x, y)] == CellStatus::MINE ? "o" :
            board[index(x, y)] == CellStatus::OPPONENT ? "x" : " ");
      }
    }
    for (int x = -1; x <= S; x++) {
      printf(x == -1 ? "+" : x == S ? "+\n" : "-");
    }
  }
};

class RandomPlayer : public Player {
public:
  RandomPlayer() {
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
    return judge(_board);
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
    // opponent's promissing line
    std::transform(align, align + N_ALIGN, n_oppn, [&bd](int a[]) {
        for (int i = 0; i < S; i++) {
          if (bd[a[i]] == CellStatus::MINE) {
            return -1;
          }
        }
        int n[S];
        std::transform(a, a + S, n, [&bd](int i) {
            return bd[i] == CellStatus::OPPONENT ? 1 : 0;
          });
        return std::accumulate(n, n + S, 0);
      });
    // player's promissing line
    std::transform(align, align + N_ALIGN, n_mine, [&bd](int a[]) {
        for (int i = 0; i < S; i++) {
          if (bd[a[i]] == CellStatus::OPPONENT) {
            return -1;
          }
        }
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
    }

    for (int i = 0; i < N_ALIGN; i++) {
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
    // root
    _tree = new GameTree(-1);
    _cur_leaf = _tree;
  }
  virtual ~MCTSPlayer() {
    // free memory
    std::stack<GameTree *> release_stack;
    std::queue<GameTree *> q;
    GameTree *tr;
    q.push(_tree);
    while (!q.empty()) {
      tr = q.front(); q.pop();
      release_stack.push(tr);
      for (auto _tr: tr->leaves) {
        if (_tr) q.push(_tr);
      }
    }
    while (!release_stack.empty()) {
      tr = release_stack.top(); release_stack.pop();
      delete tr;
    }
  }

  virtual int play() {
    GameTree *next;
    int index_center = index(S/2, S/2);
    if (_board[index_center] == CellStatus::VACANT) {
      // heuristic (take center if possible)
      next = _cur_leaf->leaves[index_center];
      next = new GameTree(index_center);
    } else {
      // choose the leaf with the highest UCB1 score
      update_tree();
      next = highest_leaf(_cur_leaf);
    }

    _cur_leaf = next;
    debug_log("mine move %d (UCB1 = %f)\n", next->move, next->score(_total_playouts));
    _board[next->move] = CellStatus::MINE;
    return next->move;
  }

  virtual GameStatus update(int p) {
    bool found = false;
    debug_log("opp. move %d\n", p);
    for (int i = 0; i < SIZE; i++) {
      auto leaf = _cur_leaf->leaves[i];
      if (i == p) {
        if (leaf) {
          _cur_leaf = leaf;
        } else {
          _cur_leaf = new GameTree(p);
        }
        _board[p] = CellStatus::OPPONENT;
        found = true;
        break;
      }
    }
    assert(found);
    return judge(_board);
  }

private:
  using GameTree = struct _GTree {
    _GTree(int _move) : n_win(0), n_playouts(0), move(_move) {
      std::fill(leaves, leaves + SIZE, nullptr);
    }
    // UCB1 score
    float score(int total_playouts) {
      if (n_playouts == 0) {
        return 0;
      } else {
        return 1.0 * n_win / n_playouts + std::sqrt(2.0 * std::log(total_playouts / n_playouts));
      }
    }
    int n_win;
    int n_playouts;
    // corresponding move
    int move;
    // children leaves
    _GTree *leaves[SIZE];
  };

  GameTree *highest_leaf(GameTree *node) {
    return *std::max_element(
        node->leaves, node->leaves + SIZE,
        [this](GameTree *a, GameTree *b) {
          return b && (!a || a->score(_total_playouts) < b->score(_total_playouts));
        });
  }

  void update_tree() {
    if (std::any_of(_cur_leaf->leaves, _cur_leaf->leaves + SIZE,
          [](GameTree *l) { return !l; })) {
      // raise next possible moves
      for (int i = 0; i < SIZE; i++) {
        if (_board[i] == CellStatus::VACANT) {
          _cur_leaf->leaves[i] = new GameTree(i);
        }
      }
    }

    // do one playout
    playout();
  }

  void playout() {
    CellStatus tmp_board[SIZE];
    std::copy(_board, _board + SIZE, tmp_board);

    // traverse path with the highest score
    bool my_turn = false;
    std::stack<GameTree *> moves;
    GameTree *trace = _cur_leaf;
    _total_playouts += 1;
    GameStatus game_status;

    while ((game_status = judge(tmp_board)) == GameStatus::CONTD) {
      int next_move;
      my_turn = !my_turn;
      do {
        next_move = mt_rand() % SIZE;
      } while (tmp_board[next_move] != CellStatus::VACANT);

      tmp_board[next_move] = my_turn ? CellStatus::MINE : CellStatus::OPPONENT;
      if (!trace->leaves[next_move]) {
        trace->leaves[next_move] = new GameTree(next_move);
      }
      trace = trace->leaves[next_move];
      moves.push(trace);
    }

    // backprop
    while(!moves.empty()) {
      auto leaf = moves.top();
      leaf->n_win += (game_status == GameStatus::WIN ? 1 : 0);
      leaf->n_playouts += 1;
      moves.pop();
    }
  }

  GameTree *_tree = nullptr;
  GameTree *_cur_leaf = _tree;
  int _total_playouts = 0;
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
    align[2*S+1][i] = index(S - i - 1, i);
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
    if (verbose) p1->dump();

    result = p1->update(p2->play());
    if (result == GameStatus::WIN) {
      return 1;
    } else if (result == GameStatus::LOSE) {
      return 2;
    } else if (result == GameStatus::DRAW) {
      return 0;
    }
    if (verbose) p1->dump();
  }
}

int main(int argc, char *argv[]) {
  Player *p1 = nullptr, *p2 = nullptr;
  std::string random_player = "random";
  std::string perfect_player = "perfect";
  std::string mcts_player = "mcts";
  int opt;
  while ((opt = getopt(argc, argv, "a:b:v")) != -1) {
    switch (opt) {
      case 'a':
        if (optarg == random_player) {
          p1 = new RandomPlayer();
        } else if (optarg == perfect_player) {
          p1 = new PerfectPlayer();
        } else if (optarg == mcts_player) {
          p1 = new MCTSPlayer();
        } else {
          fprintf(stderr, "Unknown player\n");
          exit(-1);
        }
        break;

      case 'b':
        if (optarg == random_player) {
          p2 = new RandomPlayer();
        } else if (optarg == perfect_player) {
          p2 = new PerfectPlayer();
        } else if (optarg == mcts_player) {
          p2 = new MCTSPlayer();
        } else {
          fprintf(stderr, "Unknown player\n");
          exit(-1);
        }
        break;

      case 'v':
        verbose = true;
        break;
    }
  }

  if (!p1) p1 = new RandomPlayer();
  if (!p2) p2 = new RandomPlayer();

  // initialization
  std::random_device rnd;
  auto seed = rnd();
  mt_rand.seed(seed);
  if (verbose) {
    fprintf(stderr, "seed = %u\n", seed);
  }
  init_align();

  // match
  int result = match(p1, p2);

  // result
  if (verbose) {
    p1->dump();
    printf("winner = %d\n", result);
  } else {
    printf("%d\n", result);
  }

  delete p1;
  delete p2;

  return 0;
}
