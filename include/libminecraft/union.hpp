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
 * The user could retrieve info via mc::cunion<>::info() (or cuobj.info()).
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
 * @brief The ordinal view of a typed union information. Type-casting
 * to this class would yield ordinal information.
 */
template <typename T>
struct McDtUnionOrdinal {
	/// The ordinal of the specified type.
	const size_t ordinal;

	// Constructor of the ordinal view.
	McDtUnionOrdinal(size_t ordinal): ordinal(ordinal) {}
};

/**
 * @brief The final block storing the union information, should be 
 * inherited by other typed-union informationn blocks.
 */
template <size_t maxOrdinal>
class McDtUnionInfo<maxOrdinal> {
protected:
	/// The abstract constructor for a typed union specified type.
	typedef void (*newInstance)(char* buffer);
	
	/// The abstract destructor for a typed union specified type.
	typedef void (*deleteInstance)(char* buffer);
	
	/// Stores the constructor information. Other typed-union
	/// information block should inhert register its typed 
	/// constructor here.
	newInstance constructors[maxOrdinal];
	
	/// Stores the desctructor information. Used just like the 
	/// constructor counterpart.
	deleteInstance destructors[maxOrdinal];
public:
	/// Construct an instance to the specified buffer by ordinal.
	/// @brief ordinal The ordinal of the object to create.
	/// @brief buffer The buffer to create object onto.
	void newByOrdinal(int32_t ordinal, char* buffer) const {
		if(ordinal < 0 || ordinal >= maxOrdinal)
			throw std::runtime_error("Union ordinal value exceeds boundary.");
		constructors[ordinal](buffer);
	}
	
	/// Destroy an instance with prior known ordinal.
	/// @brief ordinal of ordinal of the object to destroy.
	/// @brief buffer The buffer to destroy object from.
	void deleteByOrdinal(int32_t ordinal, char* buffer) const noexcept {
		if(ordinal >= 0 && ordinal < maxOrdinal) try {
			destructors[ordinal](buffer);
		} catch(const std::exception&) { /* Digest exceptions. */ }
	}
};

/**
 * @brief A typed union information block that stores typed information
 * for the head type in the typed list.
 */
template <size_t baseOrdinal, typename T0, typename... T>
class McDtUnionInfo<baseOrdinal, T0, T...> : 
	// Conjuncted with tailing information blocks.
	public McDtUnionInfo<baseOrdinal + 1, T...>,
	// Providing ordinal view of the fhead type.
	public McDtUnionOrdinal<T0> {					
	// Max value of ordinal to find the last block in the union information.
	static constexpr size_t maxOrdinal = baseOrdinal + 1 + (sizeof...(T));
	
	/// The constructor function for head type.
	static void newInstanceMethod(char* buffer) 
		{	new (buffer) T0();	}
	
	/// The destructor for the head type.
	static void deleteInstanceMethod(char* buffer) noexcept
		{	((T0*)buffer) -> ~T0();	}
public:	
	/// Current information block constructor.
	McDtUnionInfo(): McDtUnionInfo<baseOrdinal + 1, T...>(),
			McDtUnionOrdinal<T0>(baseOrdinal) {
		
		// Register constructor and destructor of the head type.
		McDtUnionInfo<maxOrdinal>::constructors[baseOrdinal] = 
			McDtUnionInfo<baseOrdinal, T0, T...>::newInstanceMethod;
		McDtUnionInfo<maxOrdinal>::destructors[baseOrdinal] = 
			McDtUnionInfo<baseOrdinal, T0, T...>::deleteInstanceMethod;
	}
	
	/// Helper function to be used as info.ordinalOf<U>().
	template<typename U> size_t ordinalOf() const noexcept;
};

// The body of the ordinalOf<U>() helper function.
template<size_t baseOrdinal, typename T0, typename... T> 
template<typename U> 
size_t McDtUnionInfo<baseOrdinal, T0, T...>::ordinalOf() const noexcept {
	return McDtUnionOrdinal<U>::ordinal;
}

/// Used to determine max memory usage of the provided typed list.
template<typename... T>
struct McDtUnionMaxSize;

/// End of the max memory usage block.
template<>
struct McDtUnionMaxSize<> {
	static constexpr size_t value = 0;
};

/// Generate the next memory usage block based on head type and list tail.
template<typename T0, typename... T>
struct McDtUnionMaxSize<T0, T...> {
	static constexpr size_t value = 
		sizeof(T0) > McDtUnionMaxSize<T...>::value?
		sizeof(T0) : McDtUnionMaxSize<T...>::value;
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
template<size_t maxSize, class UnionInfo>
class McDtUnion {
	/// Which type is currently stored in the typed-union.
	size_t type;
	
	/// Whether the buffer stores a object, or it stores undefined data.
	bool valueValid;
	
	/// Holding the object specified as one of the union type candidate.
	int64_t value[(maxSize + sizeof(int64_t) - 1) / sizeof(int64_t)];
public:
	/// Retrieve the decoupled typed-union information.
	static const UnionInfo& info() noexcept {
		static const UnionInfo uinfo;
		return uinfo;
	}
	
	/// Constructor for the typed union object.
	McDtUnion(): type(0), valueValid(false) {
		info().newByOrdinal(type, (char*)value);
		valueValid = true;
	}
	
	/// Destructor for the typed union object.
	~McDtUnion() noexcept {
		if(valueValid) info().deleteByOrdinal(type, (char*)value);
	}
	
	/// Update the union to make it store the object of provided ordinal.
	/// @brief typeOrdinal which type of object should be stored.
	void convertOrdinal(size_t typeOrdinal) {
		// No need to prepare for object this case.
		if(valueValid && typeOrdinal == type) return;
		
		// Destroy the previous object first.
		if(valueValid && typeOrdinal != type) {
			valueValid = false;
			info().deleteByOrdinal(type, (char*)value);
		}
		
		// Create a new object them.
		type = typeOrdinal;
		info().newByOrdinal(type, (char*)value);
		valueValid = true;
	}
	
	/// Update the union to make it store the object of provided type.
	/// The type should be deduced from the template parameter, so no
	/// function parameter is provided.
	/// @return the converted object of given type.
	template<typename V>
	V& convertType() {
		convertOrdinal(info().McDtUnionOrdinal<V>::ordinal);
		return *reinterpret_cast<V*>(&value);
	}
	
	/// Check whether the object stored in the union is given type.
	/// The type should be deduced from the template parameter, and 
	/// no exception is thrown if it is not of specified type.
	template<typename V>
	bool isType() const noexcept {
		size_t typeOrdinal = info().McDtUnionOrdinal<V>::ordinal;
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
	using cunion = McDtUnion<McDtUnionMaxSize<T...>::value, cuinfo<T...> >;
}; // End of namespace mc.