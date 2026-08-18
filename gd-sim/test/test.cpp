#include <iostream>
#include <Level.hpp>
#include <fstream>


std::string readFromFile(const std::string& path) {
    std::ifstream file(path);
    std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return str;
}

int main(int argc, char** argv) {
	if (argc < 3) {
		std::cerr << "Must be used by mod!" << std::endl;
        return 1;
	}

	std::string levelString = readFromFile(argv[1]);
	std::string inputs = readFromFile(argv[2]);

	Level lvl(levelString);

    lvl.debug = true;
    bool pressed = false;
    size_t frame = 2;
    while (lvl.latestState().pos.x < lvl.length && !lvl.latestState().dead) {
        if (frame < inputs.size())
            pressed = inputs[frame] == '1';
        auto const& state = lvl.runFrame(pressed);
        ++frame;

        if (state.dead) {
            std::cerr << "Macro failed at frame " << lvl.currentFrame() << std::endl;
            return 0;
        }
    }

    return 0;
}
