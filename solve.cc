#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <vector>

namespace {

// Orthogonal directions: left, right, down, up
const int ND = 4;
const int DR[ND] = {  0,  0, +1, -1 };
const int DC[ND] = { -1, +1,  0,  0 };

struct Point {
  uint8_t r, c;

  static Point Narrow(int r, int c) {
    return Point{
      .r = static_cast<uint8_t>(r),
      .c = static_cast<uint8_t>(c)};
  }
};

/*
std::ostream &operator<<(std::ostream &os, const Point &p) {
  return os << int{p.r} << ',' << int{p.c};
}
*/

struct Cell {
  enum Type : uint8_t {
    OPEN = 0,
    WALL = 1,
    MOVABLE = 2,
  };

  Type type = OPEN;

  // Only set if type == movable.
  // 0 for black (which doesn't connect to anything)
  // 1+ for a color
  uint8_t color = 0;

  // 0 if type != movable.
  // 1+ if type == movable; cells in the same group belong together.
  uint8_t group = 0;

  char Char() const {
    return type == OPEN ? ' ' : type == WALL ? '#' : static_cast<char>('0' + color);
  }

  auto operator<=>(const Cell&) const = default;
};

// Maybe TODO: normalize groups to deduplicate states?
class Level {
private:
  // Width of the level, including padding walls on the left and right.
  int width;

  // Height of the level, including padding walls on the left and right.
  int height;

  // Number of movable groups in the level. Groups are numbered from 1 to
  // `groups`, inclusive.
  int groups;

  // Cells of the grid in row-major order (i.e. a vector of `height` vectors
  // of length `width`, each describing a row of the grid).
  std::vector<std::vector<Cell>> grid;

public:
  Level(Level&&) = default;
  Level(const Level&) = default;

  Level& operator=(Level&&) = default;
  Level& operator=(const Level&) = default;

  Level(const std::vector<std::string> &input) :
      width(input[0].size() + 2),
      height(input.size() + 2),
      groups(0),
      grid(std::vector<std::vector<Cell>>(height, std::vector<Cell>(width))) {
    for (int r = 0; r < height; ++r) {
      for (int c = 0; c < width; ++c) {
        if (r == 0 || r == height - 1 || c == 0 || c == width - 1) {
          grid[r][c] = Cell{.type = Cell::WALL};
        } else {
          char ch = input[r - 1][c - 1];
          if (ch == '#') {
            grid[r][c] = Cell{.type = Cell::WALL};
          } else if (ch >= '1' && ch <= '9') {
            grid[r][c] = Cell{
              .type = Cell::MOVABLE,
              .color = static_cast<uint8_t>(ch - '0'),
              .group = static_cast<uint8_t>(++groups)};
          }
        }
      }
    }
    UpdateConnections();
  }

  int Groups() const {
    return groups;
  }

  void Print(std::ostream &os) {
    os << "+-";
    for (int c = 1; c < width; ++c) os << "--";
    os << "+" << std::endl;
    for (int r = 0; r < height; ++r) {
      os << '|';
      for (int c = 0; c < width; ++c) {
        const Cell &cell = grid[r][c];
        os << cell.Char();
        if (c + 1 < width) {
          os << (cell.type == grid[r][c + 1].type && cell.group == grid[r][c + 1].group ? ' ' : '|');
        }
      }
      os << "|\n";
      if (r + 1 < height) {
        os << '|';
        for (int c = 0; c < width; ++c) {
          const Cell &cell = grid[r][c];
          os << (cell.type == grid[r + 1][c].type && cell.group == grid[r + 1][c].group ? ' ' : '-');
          if (c + 1 < width) {
            os << (cell.type == grid[r + 1][c].type && cell.group == grid[r + 1][c].group &&
              cell.type == grid[r][c + 1].type && cell.group == grid[r][c + 1].group &&
              cell.type == grid[r + 1][c + 1].type && cell.group == grid[r + 1][c + 1].group ? "·" : "+");
          }
        }
        os << "|\n";
      }
    }
    os << "+-";
    for (int c = 1; c < width; ++c) os << "--";
    os << "+" << std::endl;
  }

  auto operator<=>(const Level &) const = default;

  bool MoveGroup(uint8_t group, int dr, int dc) {
        assert((dr == 0 && (dc == -1 || dc == +1)) || (dc == 0 && (dr == -1 || dr == +1)));
    assert(group > 0 && group <= groups);
    for (int r = 1; r + 1 < height; ++r) {
      for (int c = 1; c + 1 < width; ++c) {
        if (grid[r][c].group == group) {
          if (!TryMove(r, c, dr, dc)) return false;
          DropDown();
          UpdateConnections();
          return true;
        }
      }
    }
    assert(false);   // group not found
    return false;
  }

  std::vector<Level> Successors() const {
    Level copy = *this;
    std::vector<Level> result;
    for (int g = 1; g <= groups; ++g) {
      for (int dc : {-1, +1}) {
        if (copy.MoveGroup(g, 0, dc)) {
          result.push_back(std::move(copy));
          copy = *this;
        }
      }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }

  bool Solved() {
    // Maybe TODO: we can calculate this on the fly by keep tracking of groups being merged.

    // Note: it's not sufficient to check that each color exists only in one group
    // since two blocks can be connected through a black block, which means they are
    // part of the same group but the colors don't touch.
    std::vector<std::vector<char>> visited(height, std::vector<char>(width, char{false}));
    std::set<int> colors;
    for (int r = 1; r + 1 < height; ++r) {
      for (int c = 1; c + 1 < width; ++c) {
        if (!visited[r][c] && grid[r][c].type == Cell::MOVABLE && grid[r][c].color > 0) {
          if (!colors.insert(grid[r][c].color).second) {
            // second group of one color discovered
            return false;
          }
          MarkColorVisited(r, c, visited);
        }
      }
    }
    return true;
  }

private:

  void UpdateConnections() {
    for (int r = 1; r + 1 < height; ++r) {
      for (int c = 1; c + 1 < width; ++c) {
        const Cell &cell = grid[r][c];
        if (cell.type == Cell::MOVABLE && cell.color > 0) {
          const Cell &right = grid[r][c + 1];
          if (right.type == Cell::MOVABLE && right.group != cell.group && right.color == cell.color) {
            int g = right.group;
            Regroup(r, c + 1, g, cell.group);
            RemoveUnusedGroupNumber(g);
          }
          const Cell &down = grid[r + 1][c];
          if (down.type == Cell::MOVABLE && down.group != cell.group && down.color == cell.color) {
            int g = down.group;
            Regroup(r + 1, c, g, cell.group);
            RemoveUnusedGroupNumber(g);
          }
        }
      }
    }
  }

  void Regroup(int r, int c, int from, int to) {
    if (grid[r][c].group != from) return;
    grid[r][c].group = to;
    Regroup(r - 1, c, from, to);
    Regroup(r + 1, c, from, to);
    Regroup(r, c - 1, from, to);
    Regroup(r, c + 1, from, to);
  }

  void RemoveUnusedGroupNumber(int g) {
    assert(g > 0 && g <= groups);
    for (int r = 1; r + 1 < height; ++r) {
      for (int c = 1; c + 1 < width; ++c) {
        assert(grid[r][c].group != g);
        if (grid[r][c].group > g) --grid[r][c].group;
      }
    }
    --groups;
  }

  bool TryMove(int r, int c, int dr, int dc) {
    // optimization: find some way to reuse this vector (allocate it in Successors()?)
    std::vector<std::pair<Point, Cell>> points;
    bool res = GrabMovable(points, r, c, dr, dc);
    if (!res) dr = dc = 0;
    for (const auto &p : points) {
      int r2 = p.first.r + dr;
      int c2 = p.first.c + dc;
      assert(grid[r2][c2].type == Cell::OPEN);
      grid[r2][c2] = std::move(p.second);
    }
    return res;
  }

  bool GrabMovable(std::vector<std::pair<Point, Cell>> &points, int r, int c, int dr, int dc) {
    assert(grid[r][c].type == Cell::MOVABLE);
    int g = grid[r][c].group;
    points.emplace_back(Point::Narrow(r, c), std::move(grid[r][c]));
    grid[r][c] = Cell();
    for (int d = 0; d < ND; ++d) {
      int r2 = r + DR[d];
      int c2 = c + DC[d];
      if (DR[d] == dr && DC[d] == dc) {
        if (grid[r2][c2].type == Cell::WALL) return false;
        if (grid[r2][c2].type == Cell::MOVABLE) {
          if (!GrabMovable(points, r2, c2, dr, dc)) return false;
        }
      } else {
        if (grid[r2][c2].type == Cell::MOVABLE && grid[r2][c2].group == g) {
          if (!GrabMovable(points, r2, c2, dr, dc)) return false;
        }
      }
    }
    return true;
  }

  void DropDown() {
    // FIXME: this is very inefficient.
    for (int r = 1; r + 1 < height; ++r) {
      for (int c = 1; c + 1 < width; ++c) {
        if (grid[r][c].type == Cell::MOVABLE) TryMove(r, c, 1, 0);
      }
    }
  }

  void MarkColorVisited(int r, int c, std::vector<std::vector<char>> &visited) {
    visited[r][c] = true;
    for (int d = 0; d < ND; ++d) {
      int r2 = r + DR[d];
      int c2 = c + DC[d];
      if (grid[r2][c2].type == Cell::MOVABLE && grid[r2][c2].color == grid[r][c].color && !visited[r2][c2]) {
        MarkColorVisited(r2, c2, visited);
      }
    }
  }
};

std::optional<Level> ReadLevel(std::istream &is) {
  int width = 0;
  int height = 0;
  std::string line;
  std::vector<std::string> grid;
  while (std::getline(is, line) && !line.empty()) {
    if (width == 0) {
      if (line.size() == 0) return {};
      width = line.size();
    } else {
      if (line.size() != width) return {};
    }
    grid.push_back(line);
    ++height;
  }
  if (width == 0 || height == 0) return {};
  return {Level(grid)};
}

std::vector<Level> Solve(Level initial_level) {
  std::vector<Level> result;
  if (initial_level.Solved()) {
    result.push_back(std::move(initial_level));
    return result;
  }

  std::map<Level, int> level_index;
  std::vector<std::map<Level, int>::iterator> levels;
  std::vector<int> previous_level_index;
  levels.push_back(level_index.insert({std::move(initial_level), 0}).first);
  previous_level_index.push_back(-1);

  for (int i = 0; i < level_index.size(); ++i) {
    const Level &level = levels[i]->first;
    for (Level &next_level : level.Successors()) {
      if (next_level.Solved()) {
        std::cerr << "Solution found (expanded " << levels.size() << " states)\n";
        result.push_back(next_level);
        for (int j = i; j >= 0; j = previous_level_index[j]) {
          result.push_back(levels[j]->first);
        }
        std::reverse(result.begin(), result.end());
        return result;
      }
      int j = levels.size();
      auto res = level_index.insert({std::move(next_level), j});
      if (res.second) {
        levels.push_back(res.first);
        previous_level_index.push_back(i);
      }
    }
  }
  std::cerr << "No solution found (expanded " << levels.size() << " states)\n";
  return result;
}

}  // namespace

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cout << "Usage: solve <level.txt>" << std::endl;
    return 1;
  }
  const char *filename = argv[1];
  std::optional<Level> opt_level;
  {
    std::ifstream ifs(filename);
    if (!ifs) {
      std::cerr << "Failed to open input file (" << filename << ")!" << std::endl;
      return 1;
    }
    opt_level = ReadLevel(ifs);
  }
  if (!opt_level) {
    std::cerr << "Failed to read level!" << std::endl;
    return 1;
  }
  std::vector<Level> steps = Solve(std::move(*opt_level));
  if (steps.empty()) {
    std::cout << "No solution found!" << std::endl;
  } else {
    std::cout << "Found a solution in " << steps.size() - 1 << " steps.\n";
    for (int i = 0; i != steps.size(); ++i) {
      std::cout << "\nStep " << i << ":\n";
      steps[i].Print(std::cout);
    }
    std::cout << std::flush;
  }
}
