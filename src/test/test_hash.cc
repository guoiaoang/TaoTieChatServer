#include "test_headers.h"

class HashFixture : public ::testing::Test {
protected:
    // 每个 TEST_F 运行前都会执行
    void SetUp() override {
        // 删除测试文件
        if (Utils::exists(test_file_path)) {
            Utils::remove(test_file_path);
        }

        // 创建测试文件
        Utils::mkdir(test_file_path.parent_path());
        std::ofstream ofs(test_file_path);
        ofs << "test"; // 098f6bcd4621d373cade4832627b4f6
        ofs.close();
    }

    // 每个 TEST_F 运行后都会执行
    void TearDown() override {
    }

    const path   test_file_path = Utils::temp_dir() / "hash_test_file.txt";
    const string test_str       = "The quick brown fox jumps over the lazy dog";
};

TEST_F(HashFixture, sha256) {
    // [Cyan4973/xxHash: Extremely fast non-cryptographic hash algorithm](https://github.com/Cyan4973/xxHash)
    SHA256 sha256;

    // hashing an std::string
    // std::cout << sha256("Hello World") << std::endl;
    // => a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e
    EXPECT_EQ(sha256("Hello World"), "a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e");

    // hashing a buffer of bytes
    const char* buffer = "How are you";
    // std::cout << sha256(buffer, 11) << std::endl;
    // => 9c7d5b046878838da72e40ceb3179580958df544b240869b80d0275cc07209cc
    EXPECT_EQ(sha256(buffer, 11), "9c7d5b046878838da72e40ceb3179580958df544b240869b80d0275cc07209cc");

    // or in a streaming fashion (re-use "How are you")
    SHA256 sha256stream;
    const char* url = "create.stephan-brumme.com"; // 25 bytes
    int step = 5;
    for (int i = 0; i < 25; i += step) {
        sha256stream.add(url + i, step); // add five bytes at a time
    }
    // std::cout << sha256stream.getHash() << std::endl;
    // => 82aa771f1183c52f973c798c9243a1c73833ea40961c73e55e12430ec77b69f6
    EXPECT_EQ(sha256stream.getHash(), "82aa771f1183c52f973c798c9243a1c73833ea40961c73e55e12430ec77b69f6");

    // 测试文件的 sha256
    std::ifstream ifs(test_file_path, std::ios::binary);
    SHA256 sha256file;
    const size_t buffer_size = 8192;
    char buffer_file[buffer_size] = {};
    while (ifs.read(buffer_file, buffer_size) || ifs.gcount() > 0) {
        sha256file.add(buffer_file, ifs.gcount());
    }
    // std::cout << sha256file.getHash() << std::endl; // => 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08
    EXPECT_EQ(sha256file.getHash(), "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08");
}

TEST_F(HashFixture, sha1) {
    SHA1 sha1;
    EXPECT_EQ(sha1("Hello World"), "0a4d55a8d778e5022fab701977c5d840bbc486d0");
}

// md5 example
TEST_F(HashFixture, md5) {
    MD5 md5;
    EXPECT_EQ(md5(test_str), "9e107d9d372bb6826bd81d3542a419d6");

    // 也可以使用流式接口
    MD5 md5stream;
    int step = 10;
    for (size_t i = 0; i < test_str.size(); i += step)
        md5stream.add(test_str.c_str() + i, std::min(step, static_cast<int>(test_str.size() - i)));
    EXPECT_EQ(md5stream.getHash(), "9e107d9d372bb6826bd81d3542a419d6");

    // 计算文件的 md5
    std::ifstream ifs(test_file_path, std::ios::binary);
    MD5 md5file;
    const size_t buffer_size = 8192;
    char buffer[buffer_size] = {};
    while (ifs.read(buffer, buffer_size) || ifs.gcount() > 0) {
        md5file.add(buffer, ifs.gcount());
    }
    EXPECT_EQ(md5file.getHash(), "098f6bcd4621d373cade4e832627b4f6");

    using namespace Utils::Algorithm;
    //  组件中的校验函数也应该得到相同的结果
    EXPECT_EQ(md5(test_str), "9e107d9d372bb6826bd81d3542a419d6");

    EXPECT_EQ(calculate_file_md5(test_file_path), "098f6bcd4621d373cade4e832627b4f6");
}
