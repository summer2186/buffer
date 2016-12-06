
#include <type_traits>
#include <cstdint>
#include <iostream>

template<typename T>
struct is_endian_basictype :std::false_type {};
template<>
struct is_endian_basictype<int16_t> :std::true_type {};
template<>
struct is_endian_basictype<uint16_t> :std::true_type {};
template<>
struct is_endian_basictype<int32_t> :std::true_type {};
template<>
struct is_endian_basictype<uint32_t> :std::true_type {};
template<>
struct is_endian_basictype<int64_t> :std::true_type {};
template<>
struct is_endian_basictype<uint64_t> :std::true_type {};
template<>
struct is_endian_basictype<float> :std::true_type {};
template<>
struct is_endian_basictype<double> :std::true_type {};

template<typename T>
struct is_8bit_basictype : std::false_type {};
template<>
struct is_8bit_basictype<char> : std::true_type {};
template<>
struct is_8bit_basictype<int8_t> : std::true_type {};
template<>
struct is_8bit_basictype<uint8_t> : std::true_type {};

template<typename _Ty>
class Test {
public:
	template<typename _SrcTy>
	void read(const _SrcTy& value, 
		typename std::enable_if<is_endian_basictype<_SrcTy>::value, _SrcTy>::type* dummy = 0) {
		std::cout << "read is_endian_basictype" << std::endl;
	}

	template<typename _SrcTy>
	void read(const _SrcTy& value,
		typename std::enable_if<is_8bit_basictype<_SrcTy>::value, _SrcTy>::type* dummy = 0) {
		std::cout << "read is_8bit_basictype" << std::endl;
	}
};

int main(int argc, char* argv[]) {
	Test<uint8_t> test;
	uint8_t test1 = 0;
	uint16_t test2 = 0;
	test.read(test1);
	test.read(test2);

}