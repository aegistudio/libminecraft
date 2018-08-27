#pragma once
/**
 * @file libminecraft/iobase.hpp
 * @brief Basic I/O Operations
 * @author Haoran Luo
 *
 * Defines basic I/O operations that is related to minecraft's 
 * using data type, either trasmitted or represented.
 *
 * All integer values under transmission that is not variant are 
 * always in big endian (network endian), as it is defined so 
 * in java.
 *
 * The variant integer and variant long operation are supported,
 * the maximum length for a variant integer is always 5, while 
 * it is always 9 for a variant long.
 *
 * The string in libminecraft should always be std::u16string,
 * and it will be translated from / to std::u8string during
 * receiving / transmitting, as minecraft always uses java's
 * String representation, which is utf-16 oriented.
 */
#include "libminecraft/stream.hpp"
#include <cstdint>
#include <string>

/// Data type template placeholder, indicating the underlying 
/// type should be a fixed length one, usually big endian.
class McDtFlavourFixed;

/// Data type template placeholder, indicating the underlying
/// type should be variant length.
class McDtFlavourVariant;

/**
 * @brief Data type template used in generic deduction, the compiler
 * should find the right function to call.
 */
template<typename T, typename McDtFlavour>
struct McDtDataType {
private:
	/// The data storaged in the cell.
	T data;
public:
	/// The empty constructor.
	McDtDataType(): data() {}
	
	/// The assignment constructor.
	McDtDataType(T data): data(data) {}
	
	/// The type casting operator, to extract the internal data.
	inline operator T&() { return data; }
	
	/// The const type casting operator, to extract the internal data.
	inline operator const T&() const { return data; }
	
	// The assignments from a data to this wrapper.
	inline McDtDataType& operator=(const McDtDataType& a) { data = a.data; }
	
	/// The input method that reads data from the input stream.
	McIoInputStream& read(McIoInputStream&);
	
	/// The output method that writes data to the output stream.
	McIoOutputStream& write(McIoOutputStream&) const;
};

// The input stream style shifting operator.
template<typename T, typename McDtFlavour> inline McIoInputStream& 
operator>>(McIoInputStream& inputStream, McDtDataType<T, McDtFlavour>& data) {
	return data.read(inputStream);
}

// The output stream style shifting operator.
template<typename T, typename McDtFlavour> inline McIoOutputStream& 
operator<<(McIoOutputStream& inputStream, const McDtDataType<T, McDtFlavour>& data) {
	return data.write(inputStream);
}

/// Use namespace 'mc' to indicate that these types are minecraft related data.
namespace mc {
/// Variant integer of 32-bit long.
typedef McDtDataType<int32_t,  McDtFlavourVariant> var32;
/// Variant integer of 64-bit long.
typedef McDtDataType<int64_t,  McDtFlavourVariant> var64;
/// Signed single byte.
typedef McDtDataType<int8_t,   McDtFlavourFixed>   s8;
/// Unsigned single byte.
typedef McDtDataType<uint8_t,  McDtFlavourFixed>   u8;
/// Signed 16-bit big-endian integer.
typedef McDtDataType<int16_t,  McDtFlavourFixed>   s16;
/// Unsigned 16-bit big-endian integer.
typedef McDtDataType<uint16_t, McDtFlavourFixed>   u16;
/// Signed 32-bit big-endian integer.
typedef McDtDataType<int32_t,  McDtFlavourFixed>   s32;
/// Unsigned 32-bit big-endian integer.
typedef McDtDataType<uint32_t, McDtFlavourFixed>   u32;
/// Signed 64-bit big-endian integer.
typedef McDtDataType<int64_t,  McDtFlavourFixed>   s64;
/// Unsigned 64-bit big-endian integer.
typedef McDtDataType<uint64_t, McDtFlavourFixed>   u64;
};	// End of namespace mc.

/**
 * @brief Read utf-8 string from the stream (whose byte length is known) and 
 * convert it to utf-16. 
 * It is prepared for strings whose length is confined as template.
 * @param[in] inputStream the input stream instance.
 * @param[in] byteLength the length of byte in the string.
 * @param[out] resultString the converted string.
 * @return the pointer to input stream.
 */
McIoInputStream& McIoReadUtf16String(McIoInputStream& inputStream, 
		size_t byteLength, std::u16string& resultString);