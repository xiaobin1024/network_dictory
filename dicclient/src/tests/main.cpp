#include <gtest/gtest.h>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char **argv) {
#ifdef _WIN32
    // 设置控制台编码为UTF-8（Windows）
    SetConsoleOutputCP(65001);  // CP_UTF8 的值是65001
    SetConsoleCP(65001);

    // 另外设置标准输出流为UTF-8
    std::locale::global(std::locale(".UTF-8"));
#endif

    std::cout << "Starting tests..." << std::endl;

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    return result;
}
