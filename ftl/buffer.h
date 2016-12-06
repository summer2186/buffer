#ifndef FTL_BUFFER_H_
#define FTL_BUFFER_H_

#include <vector>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <algorithm>
#include <string>
#include <stdexcept>

/**
	Buffer和StreamBuffer类，提供对缓冲区的读写

	构造：
		Buffer和StreamBuffer提供两大类构造：拷贝外部缓冲区和引用外部缓冲区
		拷贝外部缓冲区成内部使用，不依赖外部，使用简单方便，例如：
		byte_buffer buffer(32, 0); // 构造一个32字节的，初始值为0的缓冲区
		byte_buffer buffer(p, 10); // 从外部缓冲区P拷贝10字节

		引用外部缓冲区，主要是快速读写内存数据使用
		byte_buffer buffer = byte_buffer::ref(p, 10); // 引用外部缓冲区，内置指针指向p

	写入：
		写入提供了对基础数据类型的读写，并且提供了大小端处理

		buffer.write((uint8_t)10, 0);  // 在0位置写入10，类型为uint8_t
		buffer.write((double)15.0f, 0); // 在0位置写入double 15.0
		buffer.write_be((uint16_t)0xffee, 0); // 大端方式写入0xffee
		buffer.write_le((uint16_t)0xffee, 0); // 小段方式写入0xffee

		write_bytes提供写入char* /signed char* /unsigned char*功能

		另外，还提供了append方法在末尾写入数据，该方法会导致buffer的增大，如果是ref类型的buffer
		那么将会进行拷贝操作

		而write写人，如果位置超过大小，则会抛出异常

	读取：
		读取提供基础数据类型以及大小端支持，例如：
		buffer.read<char>(0); // 从0位置，读取一个char
		buffer.read_be<uint16_t>(0); // 从0位置，按照大端方式读取一个uint16_t
		buffer.read_le<uint16_t>(0); // 从0位置，按照小端方式读取一个uint16_t

	流式读写：
		如果使用StreamBuffer，那么内部维护读写的位置，外部在读写的时候，不需要传递位置参数
		通过read_eof/write_eof来判断是否到了结尾
*/

namespace ftl {

    namespace buffer_internal {

        template<typename _Ty>
        class VectorContinerT {
        public:
            typedef _Ty value_type;
			typedef _Ty& reference;
			typedef _Ty& const_reference;
            typedef _Ty* pointer;
            typedef const _Ty* const_pointer;
            typedef _Ty* iterator;
            typedef const _Ty* const_iterator;
            typedef size_t size_type;

            VectorContinerT() {}

            VectorContinerT(size_type size, const _Ty& value):
                buf_(size, value) { }

            VectorContinerT(const VectorContinerT& other):
                buf_(other.buf_){ }

            VectorContinerT(VectorContinerT&& other) :
                buf_(std::move(other.buf_)){ }

            size_type size() const {
                return buf_.size();
            }

            bool empty() const {
                return buf_.empty();
            }

            void resize(size_type size) {
                buf_.resize(size);
            }

			iterator begin() {
                return buf_._Myfirst();
            }

			const_iterator begin() const {
                return buf_._Myfirst();
            }

			iterator end() {
                return buf_._Myend();
            }

			const_iterator end() const {
                return buf_._Myend();
            }

            size_type capacity() const{
                return buf_.capacity();
            }

            void shrink() {
                buf_.shrink_to_fit();
            }

            void shrink_to_fit() {
                buf_.shrink_to_fit();
            }

            void clear() {
                buf_.clear();
            }

            VectorContinerT& operator=(const VectorContinerT& other) {
				buf_.resize(other.size());
				std::memcpy(begin(), other.begin(), other.size());
				return *this;
            }

            VectorContinerT& operator=(VectorContinerT&& other) {
				buf_ = std::move(other.buf_);
				return *this;
            }

        private:
            std::vector<_Ty> buf_;
        };

        template<typename T>
        struct DefaultAllocator {
            T* allocate(size_t size) {
                return reinterpret_cast<T*>(::malloc(size));
            }

            void deallocate(T* p) {
                return ::free(p);
            }

            T* reallocate(T* p, size_t size) {
                return reinterpret_cast<T*>(::realloc(p, size));
            }
        };

        template<typename _Ty, typename _Alloc = DefaultAllocator<_Ty>, int DefaultMiniReserveSize = 32>
        class SimpleBufferT {
			static const int DEFAULT_SHRINK_TRIGGER_COUNT = 2;
			static const int DEFAULT_MINI_SIZE = DefaultMiniReserveSize;
        public:
            typedef _Ty value_type;
			typedef _Ty& reference;
			typedef _Ty& const_reference;
            typedef _Ty* pointer;
            typedef const _Ty* const_pointer;
            typedef _Ty* iterator;
            typedef const _Ty* const_iterator;
            typedef size_t size_type;			

            SimpleBufferT():
                buf_(nullptr),
                size_(0),
                capacity_(0),
				shrink_counter_(0),
                ref_(false){
            }

            SimpleBufferT(const SimpleBufferT& other):
				buf_(nullptr),
				size_(other.size()),
				capacity_(0),
				shrink_counter_(0),
				ref_(false) {
				reserve(other.size());
				memcpy(begin(), other.begin(), other.size());
            }

			explicit SimpleBufferT(size_t size) :
				buf_(nullptr),
				size_(size),
				capacity_(0),
				shrink_counter_(0),
				ref_(false) {
				reserve(size);
				size_ = size;
			}

            SimpleBufferT(size_t size, const _Ty& val):
				buf_(nullptr),
				size_(size),
				capacity_(0),
				shrink_counter_(0),
				ref_(false) {
				reserve(size);
				size_ = size;
				std::memset(begin(), val, size);
            }

            SimpleBufferT(const_pointer buf, size_type size):
				buf_(nullptr),
				size_(size),
				capacity_(0),
				shrink_counter_(0),
				ref_(false) {
				if (buf != nullptr && size > 0) {
					reserve(size);
					size_ = size;
					std::memcpy(begin(), buf, size);
				}
            }

			explicit SimpleBufferT(const std::vector<_Ty>& vec):
				buf_(nullptr),
				size_(0),
				capacity_(0),
				shrink_counter_(0),
				ref_(false) {
				if (vec.size() > 0) {
					reserve(vec.size());
					size_ = vec.size();
					std::memcpy(begin(), &vec.front(), size_);
				}
            }
            
            SimpleBufferT(SimpleBufferT&& other) {
                buf_ = other.buf_;
                size_ = other.size_;
                capacity_ = other.capacity_;
				shrink_counter_ = other.shrink_counter_;
				ref_ = other.ref_;
				alloc_ = std::move(other.alloc_);

                other.buf_ = nullptr;
                other.size_ = 0;
                other.capacity_ = 0;
				other.ref_ = false;
				other.shrink_counter_ = 0;
            }

			static SimpleBufferT ref(std::vector<_Ty>* vec) {
				SimpleBufferT simple_buffer;
				if (vec->empty()) {
					simple_buffer.buf_ = nullptr;
					simple_buffer.size_ = 0;
					simple_buffer.capacity_ = 0;
					simple_buffer.ref_ = false;
					simple_buffer.shrink_counter_ = 0;
				}
				else {
					simple_buffer.buf_ = &vec->front();
					simple_buffer.capacity_ = simple_buffer.size_ = vec->size();
					simple_buffer.ref_ = true;
					simple_buffer.shrink_counter_ = 0;
				}
				return std::move(simple_buffer);
			}

			static SimpleBufferT ref(pointer buf, size_type size) {
				SimpleBufferT simple_buffer;
				if (buf != nullptr && size > 0) {
					simple_buffer.buf_ = buf;
					simple_buffer.capacity_ = simple_buffer.size_ = size;
					simple_buffer.ref_ = true;
				}
				return std::move(simple_buffer);
			}

            ~SimpleBufferT() {
				if (buf_ && !ref_) {
					alloc_.deallocate(buf_);
				}
            }

			SimpleBufferT& operator=(const SimpleBufferT& other) {
				if (!other.empty()) {
					resize(other.size());
					ref_ = false;
					shrink_counter_ = 0;
					std::memcpy(buf_, other.buf_, size_);
				}
				else {
					resize(0);
				}
				return *this;
			}

			SimpleBufferT& operator=(SimpleBufferT&& other) {
				if (!other.empty()) {
					buf_ = other.buf_;
					size_ = other.size_;
					capacity_ = other.capacity_;
					shrink_counter_ = other.shrink_counter_;
					ref_ = other.ref_;
					alloc_ = std::move(other.alloc_);

					other.buf_ = nullptr;
					other.size_ = 0;
					other.capacity_ = 0;
					other.ref_ = false;
					other.shrink_counter_ = 0;
				}
				else {
					resize(0);
				}
				return *this;
			}

            inline iterator begin() {
                return buf_;
            }

            inline const_iterator begin() const {
                return buf_;
            }

            inline iterator end() {
                return buf_ + size_;
            }

            inline const_iterator end() const {
                return buf_ + size_;
            }

            size_type size() const {
                return size_;
            }

			size_type capacity() const {
				return capacity_;
			}

            void resize(size_type size) {
                if (size == 0) {
                    clear();
                } else if (size > capacity_) {
                    reserve(size);
                }                
                size_ = size;
            }

			void push_back(const value_type& val) {
				size_type size = size_;
				resize(size_ + sizeof(val));
				buf_[size] = val;
			}

			void push_back(const SimpleBufferT& other) {
				if (!other.empty()) {
					size_type size = size_;
					resize(size_ + other.size());
					std::memcpy(begin() + size, other.buf_, other.size());
				}
			}

			void shrink() {
				if (size_ > 0 && size_ < 2 * capacity_) {
					if (++shrink_counter_ > DEFAULT_SHRINK_TRIGGER_COUNT) {
						reserve(size_);
						shrink_counter_ = 0;
					}
				} else {
					shrink_counter_ = 0;
				}
			}

			void shrink_to_fit() {
				if (size_ > 0) {
					reserve(size_);
				}
				shrink_counter_ = 0;
			}

            bool empty() const {
                return !size_;
            }

            void clear() {
                if (buf_ && !ref_)
                    alloc_.deallocate(buf_);
                buf_ = nullptr;
                size_ = 0;
				ref_ = false;
                capacity_ = 0;
				shrink_counter_ = 0;
            }

        private:
            pointer buf_;
            size_type size_;
            size_type capacity_;
			int shrink_counter_;
			_Alloc alloc_;
            bool ref_;

            void reserve(size_type size) {
                if (size > capacity_) {
                    capacity_ = std::max<size_type>(size, 2 * capacity_);
                    capacity_ = std::max<size_type>(DEFAULT_MINI_SIZE, capacity_);
                } else if (size < capacity_) {
                    capacity_ = size;
                } else {
                    return;
                }

				pointer p = nullptr;
				if (!ref_) {
					p = alloc_.reallocate(buf_, capacity_);
				} else {
					p = alloc_.allocate(capacity_);
					std::memcpy(p, buf_, size_);
					ref_ = false;
				}

                if (!p)
                    throw std::bad_alloc();

                buf_ = p;
            }
        };

		/**
			是否需要处理大小端的基础类型
			除了char/signed char/unsigned char之外，都为true
		*/
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

		class Endian {
		public:
			enum Endianness {
				kBigEndian = 0,
				kLittleEndian = 1
			};

			static inline void Swizzle(char* start, unsigned int len) {
				char* end = start + len - 1;
				while (start < end) {
					char tmp = *start;
					*start++ = *end;
					*end-- = tmp;
				}
			}

			static inline bool detect_big_endian(void) {
				union {
					uint32_t i;
					char c[4];
				} bint = { 0x01020304 };

				return bint.c[0] == 1;
			}

			static inline Endianness CurrentEndian() {
				static const Endianness endian =
					(detect_big_endian() == true ? kBigEndian : kLittleEndian);
				return endian;
			}

			static bool isBigEndian() {
				static bool big_endian = detect_big_endian();
				return big_endian;
			}

			template<typename _Ty, typename _RetTy, enum Endianness endianness>
			static inline typename std::enable_if<is_endian_basictype<_RetTy>::value, _RetTy>::type
				read(const _Ty* buf) {
				union NoAlias {
					_RetTy val;
					char bytes[sizeof(_RetTy)];
				};
				union NoAlias na;
				std::memcpy(na.bytes, buf, sizeof(_RetTy));
				if (CurrentEndian() != endianness)
					Swizzle(na.bytes, sizeof(_RetTy));
				return na.val;
			}

			template<typename _Ty, typename _SrcTy, enum Endianness endianness>
			static inline typename std::enable_if<is_endian_basictype<_SrcTy>::value, _Ty>::type*
				write(_Ty* buf, const _SrcTy& value) {
				union NoAlias {
					_SrcTy val;
					char bytes[sizeof(value)];
				};
				union NoAlias na = { value };
				if (CurrentEndian() != endianness)
					Swizzle(na.bytes, sizeof(value));
				return reinterpret_cast<_Ty*>(std::memcpy(buf, na.bytes, sizeof(value)));
			}
		};
        
    } // namespace buffer_internal

    template<typename _Ty, typename Continer = buffer_internal::SimpleBufferT<_Ty> >
    class BufferT {
    public:
		typedef typename Continer::value_type value_type;
		typedef typename Continer::reference reference;
		typedef typename Continer::const_reference const_reference;
		typedef typename Continer::pointer pointer;
		typedef typename Continer::const_pointer const_pointer;
		typedef typename Continer::iterator iterator;
		typedef typename Continer::const_iterator const_iterator;
		typedef typename Continer::size_type size_type;

		BufferT() { }

		BufferT(const BufferT& other):
			continer_(other.continer_){

		}

		BufferT(BufferT&& other):
			continer_(std::move(other.continer_)){

		}
		
		explicit BufferT(size_type size):
			continer_(size){

		}

		BufferT(size_type size, const value_type& val):
			continer_(size, val){

		}

		BufferT(const_pointer buf, size_type size):
			continer_(buf, size){

		}

		BufferT(const_pointer begin, const_pointer end) :
			continer_(begin, end) {

		}

		explicit BufferT(const std::vector<_Ty>& vec):
			continer_(vec){

		}

		explicit BufferT(std::vector<_Ty>&& vec) :
			continer_(std::move(vec)) {

		}

		static BufferT ref(pointer buf, size_type size) {
			Continer tmp = Continer::ref(buf, size);
			BufferT rv;
			rv.continer_ = std::move(tmp);
			return rv;
		}

		static BufferT ref(std::vector<_Ty>* vec) {
			Continer tmp = Continer::ref(vec);
			BufferT rv;
			rv.continer_ = std::move(tmp);
			return rv;
		}

		void xran(size_t pos, size_t need_size) const {
			char buf[80];
			sprintf(buf, "BufferT(%08x), size=%08x, pos=%08x, need_size=%08x", (uint32_t)this, size(), pos, need_size);
			throw std::out_of_range(std::string(buf));
		}

		pointer begin() { 
			return continer_.begin();  
		}

		const_pointer begin() const {
			return continer_.begin();
		}

		pointer end() {
			return continer_.end();
		}

		const_pointer end() const {
			return continer_.end();
		}

		size_type size() const {
			return continer_.size();
		}

		size_type capacity() const {
			return continer_.capacity();
		}

		void shrink() {
			continer_.shrink();
		}

		void fill(const value_type& value, size_type offset, size_type count = 0) {
			if (count == 0 || (offset + count) > size())
				count = size() - offset;
			std::fill(continer_.begin() + offset, continer_.begin() + offset + count, value);
		}

		BufferT slice(size_type offset, size_type count) const {
			if (empty())
				return BufferT();
			else {
				if (count + offset >= size())
					return BufferT(begin() + offset, size() - offset);
				else
					return BufferT(begin() + offset, count);
			}
		}

		void append(const std::vector<_Ty>& vec) {
			if (!vec.empty()) {
				size_type size = size;
				continer_.resize(continer_.size() + vec.size());
				std::memcpy(continer_.begin() + size, &vec.front(), vec.size());
			}
		}

		BufferT& operator=(const BufferT& other) {
			if (other.empty()) {
				continer_.resize(0);
			}
			else {
				continer_.resize(other.size());
				std::memcpy(continer_.begin(), other.continer_.begin(), other.size());
			}
			return *this;
		}

		BufferT& operator+=(const BufferT& other) {
			continer_.push_back(other.continer_);
			return *this;
		}

		BufferT& operator+=(const std::vector<_Ty>& vec) {
			if (!vec.empty()) {
				size_type size = size;
				continer_.resize(continer_.size() + vec.size());
				std::memcpy(continer_.begin() + size, &vec.front(), vec.size());
			}
			return *this;
		}

		BufferT& operator=(BufferT&& other) {
			continer_ = std::move(other.continer_);
			return *this;
		}

		bool operator==(const BufferT& other) const {
			if (other.size() == size())
				return std::memcmp(continer_.begin(), other.continer_.begin(), size()) == 0;
			else
				return false;
		}

		bool empty() const {
			return continer_.empty();
		}

		void resize(size_type size) {
			continer_.resize(size);
		}

		void clear() {
			return continer_.clear();
		}

		///////////////////////////////////////
		// read functions
		template<typename _RetTy, enum buffer_internal::Endian::Endianness endianness>
		inline _RetTy read(size_type offset) const{
			static_assert(buffer_internal::is_endian_basictype<_RetTy>::value, 
				"read_le<>/read_be<>, only support int16_t/uint16_t, "
				"int32_t/uint32_t, int64_t/uint64_t, float/double, "
				"call read<>()/read_char/read_byte to read uint8_t/int8_t/char");
			if (offset + sizeof(_RetTy) > size())
				xran(offset, sizeof(_RetTy));

			return buffer_internal::Endian::read<_Ty, _RetTy, endianness>(continer_.begin() + offset);
		}

		template<typename _RetTy>
		inline _RetTy read_le(size_type offset) const {
			return read<_RetTy, buffer_internal::Endian::kLittleEndian>(offset);
		}

		template<typename _RetTy>
		inline _RetTy read_be(size_type offset) const {
			return read<_RetTy, buffer_internal::Endian::kBigEndian>(offset);
		}

		// 根据当前的字节序来读取
		template<typename _RetTy>
		inline _RetTy read(size_type offset, 
			typename std::enable_if<buffer_internal::is_endian_basictype<_RetTy>::value>::type* dummy = 0) const {
			if (buffer_internal::Endian::CurrentEndian() == buffer_internal::Endian::kBigEndian)
				return read<_RetTy, buffer_internal::Endian::kBigEndian>(offset);
			else
				return read<_RetTy, buffer_internal::Endian::kLittleEndian>(offset);
		}

		template<typename _RetTy>
		inline _RetTy read(size_type offset,
			typename std::enable_if<buffer_internal::is_8bit_basictype<_RetTy>::value>::type* dummy = 0) const {
			if (offset >= size())
				xran(offset, sizeof(char));
			return *(continer_.begin() + offset);
		}

		/*template<>
		inline uint8_t read(size_type offset) const {
			return read_byte(offset);
		}

		template<>
		inline int8_t read<int8_t>(size_type offset) const {
			return read_char(offset);
		}

		template<>
		inline char read<char>(size_type offset) const {
			return read_char(offset);
		}*/

		inline uint8_t read_byte(size_type offset) const {
			if (offset >= size())
				xran(offset, sizeof(uint8_t));
			return *(continer_.begin() + offset);
		}

		inline char read_char(size_type offset) const {
			if (offset >= size())
				xran(offset, sizeof(char));
			return *(continer_.begin() + offset);
		}

		/////////////////////////////////////////////////
		// write functions
		template<typename _SrcTy, enum buffer_internal::Endian::Endianness endianness>
		inline void write(const _SrcTy& value, size_t offset) {
			static_assert(buffer_internal::is_endian_basictype<_SrcTy>::value,
				"write_le/write_be, only support int16_t/uint16_t, "
				"int32_t/uint32_t, int64_t/uint64_t, float/double, "
				"call write() to write char/uint8_t/int8_t");
			if (offset + sizeof(value) > size())
				xran(offset, sizeof(value));
			buffer_internal::Endian::write<_Ty, _SrcTy, endianness>(continer_.begin() + offset, value);
		}

		template<typename _SrcTy>
		inline void write_le(const _SrcTy& value, size_t offset) {
			return write<_SrcTy, buffer_internal::Endian::kLittleEndian>(value, offset);
		}

		template<typename _SrcTy>
		inline void write_be(const _SrcTy& value, size_t offset) {
			return write<_SrcTy, buffer_internal::Endian::kBigEndian>(value, offset);
		}

		template<typename _SrcTy>
		inline void write(const _SrcTy& value, size_t offset,
			typename std::enable_if<buffer_internal::is_endian_basictype<_SrcTy>::value, _SrcTy>::type* dummy = 0) {
			if (buffer_internal::Endian::CurrentEndian() == buffer_internal::Endian::kBigEndian)
				write<_SrcTy, buffer_internal::Endian::kBigEndian>(value, offset);
			else
				write<_SrcTy, buffer_internal::Endian::kLittleEndian>(value, offset);
		}

		template<typename _SrcTy>
		inline void write(const _SrcTy& value, size_t offset, 
			typename std::enable_if<buffer_internal::is_8bit_basictype<_SrcTy>::value, _SrcTy>::type* dummy = 0) {
			if (offset >= size())
				xran(offset, sizeof(value));
			*(continer_.begin() + offset) = value;
		}

		// 写入缓冲区，仅支持：char*/signed char*(int8_t*)/unsigned char*(uint8_t)
		template<typename _SrcTy>
		inline typename std::enable_if<buffer_internal::is_8bit_basictype<_SrcTy>::value, _SrcTy>::type*
			write_bytes(const _SrcTy* buf, size_type buf_size, size_type offset = 0) {
			if (offset + buf_size > size())
				xran(offset, buf_size);
			return reinterpret_cast<_SrcTy*>(std::memcpy(continer_.begin() + offset, buf, buf_size));
		}

		/////////////////////////////////////
		// append functions
		template<typename _SrcTy, enum buffer_internal::Endian::Endianness endianness>
		inline void append(const _SrcTy& value) {
			static_assert(buffer_internal::is_endian_basictype<_SrcTy>::value, 
				"append_le/append_be, only support int16_t/uint16_t, "
				"int32_t/uint32_t, int64_t/uint64_t, float/double, "
				"call append() to write char/uint8_t/int8_t");
			size_type offset = size();
			EnsureWritableBytes(size(), sizeof(value));
			write<_SrcTy, endianness>(value, offset);
		}

		template<typename _SrcTy>
		inline void append_le(const _SrcTy& value) {
			append<_SrcTy, buffer_internal::Endian::kLittleEndian>(value);
		}

		template<typename _SrcTy>
		inline void append_be(const _SrcTy& value) {
			append<_SrcTy, buffer_internal::Endian::kBigEndian>(value);
		}

		template<typename _SrcTy>
		inline void append(const _SrcTy& value,
			typename std::enable_if<buffer_internal::is_endian_basictype<_SrcTy>::value, _SrcTy>::type* dummy = 0) {
			if (buffer_internal::Endian::CurrentEndian() == buffer_internal::Endian::kBigEndian)
				append<_SrcTy, buffer_internal::Endian::kBigEndian>(value);
			else
				append<_SrcTy, buffer_internal::Endian::kLittleEndian>(value);
		}

		// for char/signed char/unsigned char
		template<typename _SrcTy>
		inline void append(const _SrcTy& value,
			typename std::enable_if<buffer_internal::is_8bit_basictype<_SrcTy>::value, _SrcTy>::type* dummy = 0) {
			size_t offset = size();
			EnsureWritableBytes(size(), sizeof(value));
			*(continer_.begin() + offset) = value;
		}

        template<typename _SrcTy>
        inline typename std::enable_if<buffer_internal::is_8bit_basictype<_SrcTy>::value, _SrcTy>::type*
            append(const _SrcTy* buf, size_type buf_size) {
            size_type offset = size();
            EnsureWritableBytes(offset, buf_size);
            return reinterpret_cast<_SrcTy*>(std::memcpy(continer_.begin() + offset, buf, buf_size));
        }

		template<typename _RetTy>
		inline typename std::enable_if<buffer_internal::is_8bit_basictype<_RetTy>::value, _RetTy>::type*
			cast_to() {
			return reinterpret_cast<_RetTy*>(begin());
		}

		template<typename _SrcTy>
		inline typename std::enable_if<buffer_internal::is_8bit_basictype<_SrcTy>::value, _SrcTy>::type*
			append(const std::vector<_SrcTy>* vec) {
			if (vec != nullptr && !vec->empty()) {
				return append<_SrcTy>(&vec->front(), vec->size());
			} else {
				return nullptr;
			}
		}

    private:
        Continer continer_;

		inline void EnsureWritableBytes(size_t offset, size_t size) {
			if (offset + size > continer_.size()) {
				continer_.resize(continer_.size() + (offset + size - continer_.size()));
			}
		}
    };


	/////////////////////////////////////////////////////////////////////
	// class stream_buffer
	template<typename _Ty, typename Buffer = BufferT<_Ty> >
	class StreamBufferT {
	public:
		typedef typename Buffer::value_type value_type;
		typedef typename Buffer::reference reference;
		typedef typename Buffer::const_reference const_reference;
		typedef typename Buffer::pointer pointer;
		typedef typename Buffer::const_pointer const_pointer;
		typedef typename Buffer::iterator iterator;
		typedef typename Buffer::const_iterator const_iterator;
		typedef typename Buffer::size_type size_type;

		StreamBufferT():
			read_index_(0),
			write_index_(0){

		}

		StreamBufferT(const StreamBufferT& other):
			buffer_(other.buffer_),
			write_index_(other.write_index_),
			read_index_(other.read_index_){

		}

		StreamBufferT(StreamBufferT&& other):
			buffer_(std::move(other.buffer_)),
			read_index_(std::move(other.read_index_)),
			write_index_(std::move(other.write_index_)){

		}

		StreamBufferT(const_pointer buf, size_type size):
			buffer_(buf, size),
			read_index_(0),
			write_index_(0) {

		}

		StreamBufferT(size_type size, const _Ty& value):
			buffer_(size, value),
			read_index_(0),
			write_index_(0) {

		}

		static StreamBufferT ref(pointer buf, size_type size) {
			if (buf != nullptr && size > 0) {
				Buffer buffer = Buffer::ref(buf, size);
				StreamBufferT rv;
				rv.buffer_ = std::move(buffer);
				return rv;
			} else {
				return StreamBufferT();
			}
		}

		static StreamBufferT ref(std::vector<pointer>* vec) {
			if (vec != nullptr && vec->size() > 0) {
				Buffer buf = Buffer::ref(vec);
				StreamBufferT rv;
				rv.buffer_ = std::move(buf);
				return rv;
			} else {
				return StreamBufferT();
			}			
		}

		inline bool empty() const {
			return buffer_.empty();
		}

		inline bool read_eof() const {
			return (read_index_ >= buffer_.size());
		}

		inline bool write_eof() const {
			return (write_index_ >= buffer_.size());
		}

		void xran(size_type need_size, bool read_action) {
			char buf[80];
			if (read_action)
				sprintf(buf, "StreamBufferT(%08x) read, size=%08x, read_index=%08x, need_size=%08x",
				(uint32_t)this, buffer_.size(), read_index_, need_size);
			else
				sprintf(buf, "StreamBufferT(%08x) write, size=%08x, write_index=%08x, need_size=%08x",
				(uint32_t)this, buffer_.size(), write_index_, need_size);
			throw std::out_of_range(std::string(buf));
		}
		
		template<typename _RetTy, enum buffer_internal::Endian::Endianness endianness>
		inline typename std::enable_if<buffer_internal::is_endian_basictype<_RetTy>::value, _RetTy>::type
			read() {
				if (read_index_ + sizeof(_RetTy) > buffer_.size())
					xran(sizeof(_RetTy), true);
				size_type offset = read_index_;
				read_index_ += sizeof(_RetTy);
				return buffer_internal::Endian::read<_Ty, _RetTy, endianness>(buffer_.begin() + offset);
		}

		template<typename _RetTy>
		inline typename std::enable_if<buffer_internal::is_8bit_basictype<_RetTy>::value, _RetTy>::type 
			read() {
				if (read_index_ + sizeof(_RetTy) > buffer_.size())
					xran(sizeof(_RetTy), true);
				return *(buffer_.begin() + (read_index_ ++ ));
		}

		template<typename _RetTy>
		inline typename std::enable_if<buffer_internal::is_endian_basictype<_RetTy>::value, _RetTy>::type
			read_le() {
				return read<_RetTy, buffer_internal::Endian::kLittleEndian>();
		}

		template<typename _RetTy>
		inline typename std::enable_if<buffer_internal::is_endian_basictype<_RetTy>::value, _RetTy>::type
			read_be() {
				return read<_RetTy, buffer_internal::Endian::kBigEndian>();
		}

		template<typename _RetTy>
		inline typename std::enable_if<buffer_internal::is_endian_basictype<_RetTy>::value, _RetTy>::type
			read(){
				if (buffer_internal::Endian::CurrentEndian() == buffer_internal::Endian::kLittleEndian)
					return read_le<_RetTy>();
				else
					return read_be<_RetTy>();
		}

		template<typename _SrcTy, enum buffer_internal::Endian::Endianness endianness>
		inline void write(const _SrcTy& value, 
			typename std::enable_if<buffer_internal::is_endian_basictype<_SrcTy>::value, _SrcTy>::type* dummy = 0) {
			if (write_index_ + sizeof(_SrcTy) > buffer_.size())
				xran(sizeof(_SrcTy), false);
			size_type offset = write_index_;
			write_index_ += sizeof(_SrcTy);
			buffer_internal::Endian::write<_Ty, _SrcTy, endianness>(buffer_.begin() + offset, value);
		}

		template<typename _SrcTy>
		inline void write(const _SrcTy& value, 
			typename std::enable_if<buffer_internal::is_8bit_basictype<_SrcTy>::value, _SrcTy>::type* dummy = 0) {
			if (write_index_ + sizeof(_SrcTy) > buffer_.size())
				xran(sizeof(_SrcTy), false);
			*(buffer_.begin() + write_index_++) = value;
		}

		template<typename _SrcTy>
		inline void write_le(const _SrcTy& value, 
			typename std::enable_if<buffer_internal::is_endian_basictype<_SrcTy>::value, _SrcTy>::type* dummy = 0) {
			write<_SrcTy, buffer_internal::Endian::kLittleEndian>(value);
		}

		template<typename _SrcTy>
		inline void write_be(const _SrcTy& value, 
			typename std::enable_if<buffer_internal::is_endian_basictype<_SrcTy>::value, _SrcTy>::type* dummy = 0) {
			write<_SrcTy, buffer_internal::Endian::kBigEndian>(value);
		}

		template<typename _SrcTy>
		inline void write(const _SrcTy& value, 
			typename std::enable_if<buffer_internal::is_endian_basictype<_SrcTy>::value, _SrcTy>::type* dummy = 0) {
			if (buffer_internal::Endian::CurrentEndian() == buffer_internal::Endian::kLittleEndian)
				return write_le<_SrcTy>(value);
			else
				return write_be<_SrcTy>(value);
		}

		// 写入缓冲区，仅支持：char*/signed char*(int8_t*)/unsigned char*(uint8_t)
		template<typename _SrcTy>
		inline typename std::enable_if<buffer_internal::is_8bit_basictype<_SrcTy>::value, _SrcTy>::type*
			write_bytes(const _SrcTy* buf, size_type buf_size) {
			if (write_index_ + buf_size > size())
				xran(buf_size, false);
			size_type offset = write_index_;
			write_index_ += buf_size;
			return reinterpret_cast<_SrcTy*>(std::memcpy(buffer_.begin() + offset, buf, buf_size));
		}

		Buffer& buf() {
			return buffer_;
		}

		const Buffer& buf() const {
			return buffer_;
		}

	private:
		size_type read_index_;
		size_type write_index_;
		Buffer buffer_;
	};

	typedef BufferT<uint8_t> byte_buffer;
	typedef StreamBufferT<uint8_t> byte_streambuffer;
    typedef BufferT<char> char_buffer;
    //typedef BufferT<uint8_t, buffer_internal::VectorContinerT<uint8_t> > vector_buffer;

} // namespace ftl

#endif // FTL_MEMORY_SIMPLE_BUFFER_H_