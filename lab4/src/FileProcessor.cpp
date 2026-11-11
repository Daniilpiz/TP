#include "FileProcessor.h"
#include "Lab2Lib.h"
#include <fstream>
#include <sstream>

std::string processFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "ERROR: cannot open " + path;

    std::ostringstream buf;
    buf << f.rdbuf();
    const std::string text = buf.str();

    std::ostringstream out;
    out << "[" << lab2::languageName() << "] ";

    std::string err;
    if (!lab2::validate(text, err)) {
        out << "INVALID: " << err;
        return out.str();
    }

    auto el = lab2::extract(text);
    out << "headers="    << el.headers.size()
        << " paragraphs=" << el.paragraphs.size()
        << " lists="      << el.lists.size();
    return out.str();
}