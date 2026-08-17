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

    // Full history so the printed trace can be cross-checked frame by frame.
    lvl.setFullHistory(true);
    lvl.debug = true;
    for (size_t i = 2; i < inputs.size(); ++i) {
        auto state = lvl.runFrame(inputs[i] == '1');

        if (state.dead) {
            std::cerr << "Macro failed at frame " << lvl.currentFrame() << std::endl;
            return 0;
        }
    }

    return 0;
}
