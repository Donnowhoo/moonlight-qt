#include "../app/streaming/input/touchdiagnostics.h"
#include <cassert>
#include <iostream>
#include <iterator>

int main()
{
    const auto dir = std::filesystem::path("build/touch-policy-tests");
    std::filesystem::create_directories(dir);
    const auto path = dir / std::filesystem::path(L"diagnostic-\ud130\uce58.log");
    DesktopTouch::DiagnosticFile log;
    assert(log.open(path));
    assert(log.write("Desktop touch input: stage=setWindow toolbar=0"));
    // Read before destruction: no shutdown or SDL logger is needed to flush.
    std::ifstream live(path, std::ios::binary);
    std::string line;
    std::getline(live, line);
    assert(line == "Desktop touch input: stage=setWindow toolbar=0");
    live.close();
    assert(!log.write(std::string(2049, 'x')));
    for (unsigned i = 1; i < 256; ++i) assert(log.write("bounded"));
    assert(!log.write("overflow"));
    DesktopTouch::DiagnosticFile unopened;
    assert(!unopened.write("must not write"));
    assert(!unopened.open(dir)); // A directory cannot be opened as a log file.
    std::cout << "Desktop touch diagnostics: live flush, Unicode path, bounds and open failure passed\n";
}
