#include <iostream>
#include <deque>
#include <sstream>
#include <fstream>
#include <optional>
#include <vector>
#include <algorithm>

// sink to mute logging
class MutedBuffer : public std::streambuf {
    private:
        // char dummy_buffer[1024];
    protected:
        virtual int overflow(int c) {
            // setp( dummy_buffer, dummy_buffer + sizeof( dummy_buffer ) );
            return (c == traits_type::eof()) ? '\0' : c;
        }
};
MutedBuffer muted_buffer;
std::ostream muted_stream(&muted_buffer);

std::ostream& logg() {
    // return std::cout;
    return muted_stream;
}

enum Direction {
    NORTH, EAST, SOUTH, WEST
};

Direction opposite(Direction dir) {
    switch (dir) {
        case NORTH:
            return SOUTH;
        case EAST:
            return WEST;
        case SOUTH:
            return NORTH;
        case WEST:
            return EAST;
        default:
            throw std::runtime_error("Unexpected direction");
    }
}

class Position {
    public:
        Position(int x, int y) : x(x), y(y) {
        }
        Position(int index, int width, int height) {
            x = index % width;
            y = index / height;
        }
        int x;
        int y;
        int to_index(int width, int height) {
            return y * height + x;
        }
        // updates the direction; you manually call undo if not valid
        bool try_direction(Direction dir) {
            switch (dir) {
                case NORTH:
                    return try_north();
                case EAST:
                    return try_east();
                case SOUTH:
                    return try_south();
                case WEST:
                    return try_west();
                default:
                    throw std::runtime_error("Unexpected direction");
            }
        }
        void undo_direction(Direction dir) {
            try_direction(opposite(dir));
        }
        std::string to_string() const {
            std::stringstream os;
            os << "(" << x << ", " << y << ")";
            return os.str();
        }
    private:
        bool try_north() { y -= 1; if (y < 0) { return false; } return true; }
        bool try_east() { x += 1; if (x >= 4) { return false; } return true; }
        bool try_south() { y += 1; if (y >= 4) { return false; } return true; }
        bool try_west() { x -= 1; if (x < 0) { return false; } return true; }
};

class Layout {
    public:
        /**
         * @brief Load layout from file
         *
         * @param filepath
         */
        Layout(const char* filename) {
            std::ifstream ifs(filename);
            std::string buffer;
            ifs >> buffer;
            width = std::atoi(buffer.c_str());
            ifs >> buffer;
            height = std::atoi(buffer.c_str());
            size = width * height;
            layout.reserve(size);
            visited.reserve(size);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    ifs >> buffer;
                    layout.push_back(std::atoi(buffer.c_str()));
                    visited.push_back(false);
                }
            }
        }
        void mark_visited(Position pos) {
            visited[pos.to_index(width, height)] = true;
        }
        bool is_position_visited(Position pos) const {
            return visited[pos.to_index(width, height)];
        }
        int get_elevation_at(Position pos) const {
            return layout[pos.to_index(width, height)];
        }
        int get_width() const {
            return width;
        }
        int get_height() const {
            return height;
        }
        int get_size() const {
            return size;
        }
    private:
        int width;
        int height;
        int size;
        std::vector<int> layout;
        std::vector<bool> visited;
};

class Walker {
    public:
        Walker(Layout& layout) :
            layout(layout), pos(0, layout.get_width(), layout.get_height()),
            dir(NORTH), is_backtracking(false) {
            }
        void explore() {
            for (int i = 0; i < layout.get_size(); ++i) {
                logg() << "\nNew root position";
                pos = Position(i, layout.get_width(), layout.get_height());
                if (i % 1000 == 0) {
                    // show some progress every 1000 root positions
                    std::cout << pos.to_string() << std::endl;
                }
                if (layout.is_position_visited(pos)) {
                    logg() << "Position has been visited" << std::endl;
                    continue;
                }
                layout.mark_visited(pos);  // not really needed (i is monotonic)
                elevation = layout.get_elevation_at(pos);
                dir = NORTH;
                trail.clear();
                trail.push_back(pos);
                while (true) {
                    bool valid_step = forward();
                    if (valid_step) {
                        dir = NORTH;
                    } else {
                        int new_dir = (int)(dir) + 1;
                        // loop until get a valid direction if possible
                        while (new_dir >= 4) {
                            if (!is_backtracking) {
                                logg() << "Found dead end" << std::endl;
                                paths.push_back(trail);
                            }
                            if (!steps.empty()) {
                                Direction last_taken_dir = backtrack();
                                new_dir = (int)last_taken_dir + 1;
                                continue;
                            } else {
                                break;
                            }
                        }
                        if (new_dir >= 4) {
                            // loop ended, but still no valid direction
                            logg() << "Path exhausted" << std::endl;
                            break;
                        } else {
                            dir = (Direction)new_dir;
                        }
                    }
                }
            }
            // sort by the criteria given
            auto compare_size = [this](  // return true if a in front of b
                const std::deque<Position>& a,
                const std::deque<Position>& b) -> bool {
                if (a.size() < b.size()) {
                    return false;
                } else if (a.size() > b.size()) {
                    return true;
                } else {
                    int starting_height_a = layout.get_elevation_at(a.front());
                    int ending_height_a = layout.get_elevation_at(a.back());
                    int drop_a = starting_height_a - ending_height_a;
                    int starting_height_b = layout.get_elevation_at(b.front());
                    int ending_height_b = layout.get_elevation_at(b.back());
                    int drop_b = starting_height_b - ending_height_b;
                    if (drop_a < drop_b) {
                        return false;
                    } else {
                        return true;
                    }
                }
            };
            std::sort(paths.begin(), paths.end(), compare_size);
            logg() << "Done exploring" << std::endl;
        }
        void export_results() {
            std::ofstream ofs;
            ofs.open("results.txt");
            for (const auto& path : paths) {
                int starting_height = layout.get_elevation_at(path.front());
                int ending_height = layout.get_elevation_at(path.back());
                ofs << "Path length " << path.size()
                    << " (drop " << starting_height - ending_height << ")"
                    << "\n";
                for (const auto& pos : path) {
                    ofs << pos.to_string()
                        << " " << layout.get_elevation_at(pos)
                        << "\n";
                }
            }
            ofs.close();
        }
    private:
        Layout& layout;  // the map i am on
        Position pos;
        Direction dir;
        int elevation;
        bool is_backtracking;
        std::optional<Direction> came_from;
        std::deque<Position> trail;     // the path travelled from root
        std::deque<Direction> steps;    // directions taken from root
        std::deque<std::deque<Position>> paths;  // confirmed paths (result)
        // TODO should probably wrap the confirmed path in a class
        bool forward() {  // if fail to forward, returns false & nothing changed
            logg() << "Trying direction " << dir
                << " from " << pos.to_string()
                << " (elevation " << layout.get_elevation_at(pos)
                << ") " << std::endl;
            if (came_from && dir == *came_from) {
                logg() << "I came from " << (*came_from) << "!" << std::endl;
                return false;
            }
            bool new_pos_is_on_map = pos.try_direction(dir);
            if (!new_pos_is_on_map) {
                logg() << "New position not on map" << std::endl;
                pos.undo_direction(dir);
                return false;
            }
            int new_elevation = layout.get_elevation_at(pos);
            if (!(new_elevation < elevation)) {
                logg() << "Elevation of new position not lower" << std::endl;
                pos.undo_direction(dir);
                return false;
            }
            logg() << "Successful forward" << std::endl;
            elevation = new_elevation;
            layout.mark_visited(pos);
            is_backtracking = false;
            trail.push_back(pos);
            steps.push_back(dir);
            came_from = opposite(dir);
            return true;
        }
        Direction backtrack() {  // returns the last taken direction
            logg() << "Backtracking";
            is_backtracking = true;
            trail.pop_back();
            pos = trail.back();
            logg() << ", now at " << pos.to_string() << std::endl;
            elevation = layout.get_elevation_at(pos);
            Direction last_taken_direction = steps.back();
            steps.pop_back();
            if (!steps.empty()) {
                came_from = opposite(steps.back());
            } else {
                came_from = std::nullopt;
            }
            return last_taken_direction;
        }
};

int main() {
    Layout layout("map.txt");
    Walker walker(layout);
    walker.explore();
    walker.export_results();
}
