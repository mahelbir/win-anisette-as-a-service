#include <iostream>

void info(std::string msg) {
	std::cout << "[*] " << msg << std::endl;
}

void error(std::string msg) {
	std::cerr << "[!] " << msg << std::endl;
}
