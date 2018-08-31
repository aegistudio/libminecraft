#pragma once
/**
 * @file libminecraft/union.hpp
 * @brief Typed-Union (aka. Contextual Union, mc::cuinon)
 * @author Haoran Luo
 *
 * Defines typed union that works for Non-POD (Non-Plain Old Data) objects.
 *
 * The typed union (mc::cunion, instance of which is denoted by cuobj in the 
 * following text) can hold exactly one of the listed types in the 
 * template parameter, and tell currently which object is stored. The user can 
 * typed-access either by explcit type (cuobj.asType<T>(), cuobj.isType<T>(), 
 * cuobj.convertType<T>(), etc.) or implicit ordinal (cuobj.ordinal(), 
 * cuobj.convertOrdinal()).
 *
 * The user can assign object of one of the listed types to alter the stored
 * object in the union (By cuobj = T(), cuobj = instanceOfTypeT, etc.). And
 * type-casting it to one of the listed types could retrieve the stored 
 * object with type-checking.
 *
 * The typed union info (mc::cuinfo) stores reflection information about the 
 * provided types. (Like retrieving an ordinal of a type, via info.ordinalOf<T>()).
 * The user could retrieve info via mc::cunion<>::info (or cuobj.info).
 */

#include <cstddef>
#include <stdexcept>
 
/**
 * @brief The typed union info reflecting ordinal information and memory
 * management information.
 */
template <size_t baseOrdinal, typename... T>
class McDtUnionInfo;

/**
 * @brief The final block storing the union information.
 *
 * The final block specifies failure or termination conditions for 
 * template's specialization deduction.
 */
template <size_t maxOrdinal>
class McDtUnionInfo<maxOrdinal> {
public:
	/// Construct an instance to the specified buffer by ordinal.
	/// @brief ordinal The ordinal of the object to create.
	/// @brief buffer The buffer to create object onto.
	inline static void newByOrdinal(int32_t ordinal, char* buffer) {
		// Ordinal must be greater than or equal the max ordinal this case.
		throw std::runtime_error("Union ordinal value exceeds boundary.");
	}
	
	/// Destroy an instance with prior known ordinal.
	/// @brief ordinal of ordinal of the object to destroy.
	/// @brief buffer The buffer to destroy object from.
	inline static void deleteByOrdinal(int32_t ordinal, char* buffer) noexcept {
		// Do nothing, no throw here.
	}
	
	/// @return the max size of specified union types.
	inline static constexpr size_t maxSize() {	return 0;	}
};

/**
 * @brief A typed union information block that stores typed information
 * for the head type in the typed list.
 */
template <size_t baseOrdinal, typename T0, typename... T>
class McDtUnionInfo<baseOrdinal, T0, T...> {
	// The next node of union information block.
	typedef McDtUnionInfo<baseOrdinal + 1, T...> next;
public:
	/// Construct an instance to the specified buffer by ordinal.
	/// @brief ordinal The ordinal of the object to create.
	/// @brief buffer The buffer to create object onto.
	inline static void newByOrdinal(int32_t ordinal, char* buffer) {
		if(ordinal < baseOrdinal) 
			throw std::runtime_error("Union ordinal value exceeds boundary");
		else if(ordinal == baseOrdinal) new (buffer) T0();
		else next::newByOrdinal(ordinal, buffer);
	}
	
	/// Destroy an instance with prior known ordinal.
	/// @brief ordinal of ordinal of the object to destroy.
	/// @brief buffer The buffer to destroy object from.
	inline static void deleteByOrdinal(int32_t ordinal, char* buffer) noexcept {
		if(ordinal < baseOrdinal) return;
		else if(ordinal == baseOrdinal) try {
			((T0*)buffer) -> ~T0();
		} catch(const std::exception&) { /* Digest exceptions. */ }
		else next::deleteByOrdinal(ordinal, buffer);
	}
	
	// Helper functions to be used as info.ordinalOf<U>().
	// The specified type is current union information block's type, so just return current
	// ordinal as result.
	template<typename U> static constexpr 
	typename std::enable_if< std::is_same<T0, U>::value, size_t>::type
	ordinalOf() {	return baseOrdinal;	}
	
	// If the specified type is not the in the typed union's list, the code won't compile
	// due to failure of matching specialization.
	template<typename U> static constexpr 
	typename std::enable_if<!std::is_same<T0, U>::value, size_t>::type
	ordinalOf() {	return next::template ordinalOf<U>();	}
	
	/// @return the max size of specified union types.
	inline static constexpr size_t maxSize() {	
		return 	sizeof(T0) > next::maxSize()? 
				sizeof(T0) : next::maxSize();
	}
};

/**
 * @brief The typed-union object.
 *
 * The max-size calculation and union information deduction is decoupled 
 * from this class to make the template easier to comprehend.
 *
 * The object associated with the ordinal (type) is managed by the union,
 * and will be destroyed when the union object get destroyed.
 */
template<class UnionInfo>
class McDtUnion {
public:
	///  The union information block for this typed union.
	static constexpr UnionInfo info = UnionInfo();
private:
	/// Which type is currently stored in the typed-union.
	size_t type;
	
	/// Whether the buffer stores a object, or it stores undefined data.
	bool valueValid;
	
	/// Holding the object specified as one of the union type candidate.
	int64_t value[(info.maxSize() + sizeof(int64_t) - 1) / sizeof(int64_t)];
public:
	
	/// Constructor for the typed union object.
	McDtUnion(): type(0), valueValid(false) {
		// Don't automatically initialize object.
	}
	
	/// Destructor for the typed union object.
	~McDtUnion() noexcept {
		if(valueValid) info.deleteByOrdinal(type, (char*)value);
	}
	
	/// Update the union to make it store the object of provided ordinal.
	/// @brief typeOrdinal which type of object should be stored.
	void convertOrdinal(size_t typeOrdinal) {
		// No need to prepare for object this case.
		if(valueValid && typeOrdinal == type) return;
		
		// Destroy the previous object first.
		if(valueValid && typeOrdinal != type) {
			valueValid = false;
			info.deleteByOrdinal(type, (char*)value);
		}
		
		// Create a new object them.
		type = typeOrdinal;
		info.newByOrdinal(type, (char*)value);
		valueValid = true;
	}
	
	/// Update the union to make it store the object of provided type.
	/// The type should be deduced from the template parameter, so no
	/// function parameter is provided.
	/// @return the converted object of given type.
	template<typename V>
	V& convertType() {
		convertOrdinal(info.template ordinalOf<V>());
		return *reinterpret_cast<V*>(&value);
	}
	
	/// Check whether the object stored in the union is given type.
	/// The type should be deduced from the template parameter, and 
	/// no exception is thrown if it is not of specified type.
	template<typename V>
	bool isType() const noexcept {
		size_t typeOrdinal = info.template ordinalOf<V>();
		if(!valueValid || typeOrdinal != type) return false;
		else return true;
	}
	
	/// Type cast the object stored in the union as given type's 
	/// const reference. When the type-cast could not be performed, 
	/// an runtime exception will be thrown.
	template<typename V>
	const V& asType() const {
		if(!isType<V>())
			throw std::runtime_error("Object is not of specified type.");
		else return *reinterpret_cast<const V*>(&value);
	}
	
	/// Type cast the object stored in the union as given type's 
	/// reference. When the type-cast could not be performed, an runtime
	/// exception will be thrown.
	template<typename V>
	V& asType() {
		return const_cast<V&>(
			static_cast<const McDtUnion&>(*this).asType<V>());
	}
	
	/// Assigning an immutable object to the object stored in the union.
	/// If the stored object is not of the assigend object's type, it 
	/// will be converted to the assigned object's type first.
	template<typename V>
	McDtUnion& operator=(const V& copyValue) {
		convertType<V>() = copyValue;
		return *this;
	}
	
	/// Assigning a mutable object to the object stored in the union.
	template<typename V>
	McDtUnion& operator=(V&& moveValue) {
		convertType<typename std::remove_reference<V>::type>() 
				= std::forward<V>(moveValue);
		return *this;
	}
	
	/// Retrieve the current ordinal of the stored types.
	size_t ordinal() const { return type; }
	
	/// Type-casting method of this class. Which wraps the asType<V>() 
	/// method and will throw exception when failing to cast.
	template<typename V>
	operator const V&() const { return asType<V>(); }
	
	/// Type-casting method of this class, just like the constant 
	/// casting version.
	template<typename V>
	operator V&() { return asType<V>(); }
};

/// Use namespace 'mc' to separate cunion and cuinfo from global namespace.
namespace mc {
	/// The mc::cuinfo type indicating this is a union information.
	template<typename... T>
	using cuinfo = McDtUnionInfo<0, T...>;
	
	/// The mc::cunion type indicating this is the typed-union object.
	template<typename... T>
	using cunion = McDtUnion<cuinfo<T...> >;
}; // End of namespace mc.