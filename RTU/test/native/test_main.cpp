#include "TestSupport.h"

unsigned testChecks = 0;

int main()
{
    runDecoderTests();
    runSenderTests();
    std::cout << "PASS: " << testChecks << " sender checks, including guarded CAN payload reads\n";
    return 0;
}
