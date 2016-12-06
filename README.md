# BufferT class

简单的Buffer类，用于访问缓冲区，提供以下特性：

* 引用或者拷贝外部缓冲区
* 大小端方式读取/写入
* 可选流式读写

VS2015/G++ 5.4编译通过

# 使用

仅有一个头文件：  **ftl/buffer.h**

include进项目即可

## 构造

**BufferT** 用于可变的缓冲区，也可以引用外部缓冲区，支持：char/signed char/unsigned char，内部已经定义了：  **byte_buffer**

```c++
// 构造一个大小为32字节的buffer
byte_buffer buf(32);

// 构造一个大小为32字节，初始值为0的buffer
byte_buffer buf(32, 0);

// 引用外部的buffer
uint8_t fix_buf = {1,2,3};
// 如果采用引用的方式，当发生长度变更，内部会重新拷贝
byte_buffer buf = byte_buffer::ref(fix_buf, sizeof(fix_buf));

// move other instance
byte_buffer buf(std::move(other));
```

## 读写

### read

**read** 主要参数为：类型、位置，如果溢出则会抛出异常

```c++
buffer.read<char>(0); // 从0位置，读取一个char
buffer.read_be<uint16_t>(0); // 从0位置，按照大端方式读取一个uint16_t
buffer.read_le<uint16_t>(0); // 从0位置，按照小端方式读取一个uint16_t
```

### write

**write** 主要参数为：数值、位置，如果溢出则会抛出异常

```c++
buffer.write((uint8_t)10, 0);  // 在0位置写入10，类型为uint8_t
buffer.write((double)15.0f, 0); // 在0位置写入double 15.0
buffer.write_be((uint16_t)0xffee, 0); // 大端方式写入0xffee
buffer.write_le((uint16_t)0xffee, 0); // 小段方式写入0xffee

//write_bytes提供写入char* /signed char* /unsigned char*功能
write_bytes(str.c_str(), str.length(), 0);
```


### append

**append** 提供在缓冲区尾部，增加数据功能，主要参数为：数值

```c++
buf1.append(char(1));
buf1.append((unsigned char)(2));
buf1.append((signed char)(1));
buf1.append(uint16_t(0xffee));
buf1.append_be(uint16_t(0xffee));
buf1.append_le(uint16_t(0xffee));
```
