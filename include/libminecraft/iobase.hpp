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
#include <stdexcept>

/// Data type template placeholder, indicating the underlying 
/// type should be a fixed length one, usually big endian.
class McDtFlavourFixed;

/// Data type template placeholder, indicating the underlying
/// type should be variant length.
class McDtFlavourVariant;

/// Data type template placeholder, indicating the underlying
/// type should be a length prefixed string, with a predefined max length.
/// The length is in the unit of code unit.
template<size_t maxLength>
class McDtFlavourString;

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
	// The constructors for the type.
	McDtDataType(): data() {}
	McDtDataType(const T& data): data(data) {}
	McDtDataType(T&& data): data(data) {}
	
	/// The const type casting operator, to extract the internal data.
	inline operator const T&() const { return data; }
	inline operator T&&() { return std::move(data); }
		
	// Allow assignment between data type of different flavours.
	template<typename U>
	inline McDtDataType& operator=(const McDtDataType<T, U>& a) 
			{	data = (const T&)a; return *this; }
	template<typename U>
	inline McDtDataType& operator=(McDtDataType<T, U>&& a) 
			{	data = (T&&)a; return *this; }
	
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

/**
 * @brief Read utf-8 string from the stream (whose byte length is known) and 
 * convert it to utf-16. 
 * It is for mc::ustring to call as it is template.
 * @param[in] inputStream the input stream instance.
 * @param[in] byteLength the length of byte in the string.
 * @param[out] resultString the converted string.
 * @param[out] codeUnitLength the number of code units.
 * @return the pointer to input stream.
 */
McIoInputStream& McIoReadUtf16String(McIoInputStream& inputStream, 
		size_t byteLength, std::u16string& resultString);

/**
 * @brief Write utf-16 string to the stream as utf-8 with length prefixed.
 * It is for mc::ustring to call as it is template.
 * @param[in] outputStream the output stream instance.
 * @param[in] outputString the string to output.
 */
McIoOutputStream& McIoWriteUtf16String(McIoOutputStream& outputStream,
		const std::u16string& outputString);
		
/**
 * @brief Convert from a string with locale imbued to an utf-16 string.
 * @param[in] localeImbuedString the string with application locale.
 * @return the utf-16 string converted.
 */
std::u16string McIoLocaleStringToUtf16(const std::string& localeImbuedString);

/**
 * @brief Convert from a utf-16 string into string with locale imbued.
 * @param[in] utf16String the string with utf-16 encoding.
 * @return the locale imbued string.
 */
std::string McIoUtf16StringLocale(const std::u16string& utf16String);
	
/**
 * Template partial specialization for std::u16string as internal data,
 * which is the storage form of mc::ustring.
 *
 * Documents are omitted, see the most general McDtDataType for interface
 * description.
 */
template<size_t maxLength>
class McDtDataType<std::u16string, McDtFlavourString<maxLength> > {
	std::u16string data;
	
	// Performing length integrity checking.
	inline void ensureLengthConstrain() const {
		if(maxLength != 0 && data.length() > maxLength) throw std::runtime_error(
			std::string("The string is too long (must be shorter than ") 
				+ std::to_string(maxLength) + " code units).");
	}
public:
	McDtDataType(): data() {}
	McDtDataType(const std::u16string& data): data(data) 
		{	ensureLengthConstrain();	}
	McDtDataType(std::u16string&& data): data(std::move(data))
		{	ensureLengthConstrain();	}
	
	inline operator const std::u16string&() const { return data; }
	inline operator std::u16string&&() const { return std::move(data); }
	
	template<typename F>
	inline McDtDataType& operator=(const McDtDataType<std::u16string, F>& a) { 
		if(std::is_same<McDtFlavourString<maxLength>, F>::value) {
			// No check required to perform, directly copy assign.
			data = a.data; 
		}
		else {
			// Check copied value before copy assigning.
			using std::swap;
			McDtDataType checkingObject(std::move((std::u16string&&)a));
			swap(data, checkingObject.data);
		}
		return *this; 
	}
	
	template<typename F>
	inline McDtDataType& operator=(McDtDataType<std::u16string, F>& a) { 
		return operator=(const_cast<const McDtDataType<std::u16string, F>&>(a));
	}
	
	template<typename F>
	inline McDtDataType& operator=(McDtDataType<std::u16string, F>&& a) {
		if(std::is_same<McDtFlavourString<maxLength>, F>::value) {
			// No check required to perform, directly move assign.
			data = std::move(a.data); 
		}
		else {
			// Check copied value before move assigning.
			using std::swap;
			McDtDataType checkingObject(std::move((std::u16string&&)a));
			swap(data, checkingObject.data);
		}
		return *this; 
	}
	
	McIoInputStream& read(McIoInputStream& inputStream);
	McIoOutputStream& write(McIoOutputStream& outputStream) const {
		return McIoWriteUtf16String(outputStream, data);
	}
	
	// Special constructors and toString() like only for this specialization.
	// Notice that the string passed in is localized (depending on application
	// locale settings).
	McDtDataType(const std::string& localeImbuedString): 
		data(McIoLocaleStringToUtf16(localeImbuedString))
		{	ensureLengthConstrain();	}
	std::string str() const { return McIoUtf16StringLocale(data); }
};
	
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

/**
 * The unicode (utf-16 while processing, utf-8 while transmitting) string.
 * It is usually built in with a boundary check, the string won't be parsed
 * if it fails to pass the check.
 *
 * The length 32767 is the currently max allowed length transmitted in 
 * minecraft protocol, however setting 0 could completely disable this 
 * length check.
 */
template<size_t maxLength = 32767>
using ustring = McDtDataType<std::u16string, McDtFlavourString<maxLength> >;
};	// End of namespace mc.

/// The implementation of reading mc::ustring with max length constrain.
template<size_t maxLength> inline McIoInputStream& 
mc::ustring<maxLength>::read(McIoInputStream& inputStream) {
	mc::var32 byteLength;	// The string length in byte.
	McIoInputStream& aInputStream = byteLength.read(inputStream);
	if(byteLength < 0) throw std::runtime_error("The string has negative length.");
	else if(maxLength > 0 && byteLength > (maxLength * 4)) 
		throw std::runtime_error("The string is too long.");
	McIoInputStream& bInputStream = McIoReadUtf16String(
		aInputStream, (size_t)byteLength, data);
	ensureLengthConstrain();
	return bInputStream;
}