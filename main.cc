#include "ftl/buffer.h"

#include <iostream>
#include <assert.h>

using namespace ftl;
using namespace ftl::buffer_internal;
using namespace std;


void TestByteBuffer1() {
	byte_buffer buf1;
	assert((buf1.size() == 0));

	byte_buffer::value_type local_array[] = {1,2,3};
	byte_buffer buf2(local_array, sizeof(local_array));
	assert((buf2.size() == sizeof(local_array)));

	byte_buffer buf3(std::move(buf2));
	assert((buf2.size() == 0));
	assert((buf3.size() == sizeof(local_array)));

	std::vector<uint8_t> vec = {1,2,3};
	auto buf4 = byte_buffer::ref(&vec);
	assert((buf4.size() == vec.size()));

	byte_buffer buf5(std::move(vec));
	assert((buf5.size() == vec.size()));

}

void read_test() {
	byte_buffer::value_type local_array[] = { 0xff, 0xee, 0xdd };
	byte_buffer buf1(local_array, sizeof(local_array));
	assert((buf1.read_char(0) == (char)0xff));
	assert((buf1.read_byte(0) == 0xff));
	assert((buf1.read<int8_t>(0) == (int8_t)0xff));
	assert((buf1.read_be<uint16_t>(0) == 0xffee));
	assert((buf1.read_le<uint16_t>(0) == 0xeeff));
}

void write_test() {
	byte_buffer buf1(16);
	byte_buffer::value_type local_array[] = { 0xff, 0xee, 0xdd, 0xcc };
	std::string str("abc");
	buf1.write_bytes(local_array, sizeof(local_array));
	buf1.write_bytes(str.data(), str.length());

	buf1.write(char(1), 0);
	buf1.write((unsigned char)(2), 0);
	buf1.write((signed char)(1), 0);
	buf1.write(uint16_t(0xffee), 0);

	// uint16_t test_array[] = {0xffee, 0xddcc};
	// buf1.write_bytes(test_array, sizeof(test_array));
}

void cast_test() {
	byte_buffer buf1(10, 5);
	char* p1 = buf1.cast_to<char>();
	uint8_t* p2 = buf1.cast_to<uint8_t>();
	int8_t* p3 = buf1.cast_to<int8_t>();
}

void grow_test() {
	byte_buffer buf;
	for (size_t i = 0; i < 1024; i++) {
		buf.append((uint8_t)0);
	}

	assert((buf.size() == 1024));
	buf.append((uint8_t)0);
}

void append_test() {
	byte_buffer buf1;
	buf1.append(char(1));
	buf1.append((unsigned char)(2));
	buf1.append((signed char)(1));
	buf1.append(uint16_t(0xffee));
	buf1.append_be(uint16_t(0xffee));
	buf1.append_le(uint16_t(0xffee));
}

void append_buf_test() {
	std::string str("123");
	std::vector<uint8_t> vec = {0xff, 0xee, 0xdd};
	int8_t buf[] = {-10, -20, -30};
	byte_buffer buf1;
	buf1.append(str.data(), str.length());
	buf1.append(&vec.front(), vec.size());
	buf1.append(buf, sizeof(buf));
	assert((buf1.size() == 9));
	buf1.append(&vec);
	assert((buf1.size() == 12));
}


void test_streambuffer() {
	uint8_t ary[] = {1,2,3,4,5,6,7,8};
	int ary_sum = 0;
	for (auto i : ary) {
		ary_sum += i;
	}
	byte_streambuffer buf(ary, sizeof(ary));
	int count = 0;
	while (!buf.read_eof()) {
		count += buf.read<uint8_t>();
	}

	assert(ary_sum == count);

	byte_streambuffer ref_buf = byte_streambuffer::ref(ary, sizeof(ary));
	count = 0;
	while (!ref_buf.read_eof()) {
		count += ref_buf.read<uint8_t>();
	}
	assert(ary_sum == count);

}

void test_streambuffer1() {
	byte_streambuffer buf(10, 0);
	int sum1 = 0;
	uint8_t i = 1;
	while (!buf.write_eof()) {
		sum1 += i;
		buf.write(i++);
	}
	int sum2 = 0;
	while (!buf.read_eof())	{
		sum2 += buf.read<uint8_t>();
	}
	assert(sum2 == sum1);
}

int main(int argc, char* argv[]) {

    //vector_buffer vec_buf;

	TestByteBuffer1();
	read_test();
	write_test();
	append_test();
	cast_test();
	grow_test();
	append_buf_test();
	test_streambuffer();
	test_streambuffer1();
	return 0;
}