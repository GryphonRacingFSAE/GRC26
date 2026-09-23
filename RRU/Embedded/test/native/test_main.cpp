#include "TestSupport.h"

unsigned testChecks = 0;

int main()
{
    runDecoderTests();
    runCsvTests();
    std::cout << "Receiver native tests passed: " << testChecks << " checks\n";
    return 0;
}
