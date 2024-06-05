#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

int main() {
    std::string url = "https://www.google.com";
    std::string output_file = "screenshot.png";

    std::stringstream command;
    command << "/home/hyunsu00/dev/github.com/cdpSample/dist/chrome-headless-shell-linux64/chrome-headless-shell --screenshot --window-size=1280x1024 --no-sandbox --virtual-time-budget=5000 --url=" << url;

    std::system(command.str().c_str());

    std::ifstream file("screenshot.png");
    if (file.good()) {
        std::cout << "Screenshot saved to screenshot.png" << std::endl;
    } else {
        std::cerr << "Failed to save screenshot" << std::endl;
    }

    return 0;
}