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
#include <cassert>
 
/**
 * @brief The typed union info reflecting ordinal information and memory
 * management information.
 */
template <size_t baseOrdinal, typename... T>
class McDtUnionInfo;

/**
 * The union actions specifies what union information block can do with the 
 * buffer provided by the typed union.
 *
 * All action should provide a perform(bool&, char*, char*) method with generic 
 * type, to keep track of the action.
 */
namespace McDtUnionAction {
	// Invoke the default constructor (the destination buffer will be omitted).
	struct defaultConstruct {
		// Any type to perform construct must possess a default constructor.
		template<typename U> static inline 
		typename std::enable_if<std::is_default_constructible<U>::value>::type
		perform(bool& valueValid, char* dstBuffer, char* srcBuffer = nullptr) {
			assert(!valueValid);
			new (dstBuffer) U();
			valueValid = true;
		}
	};
	
	// Invoke the destructor (the destination buffer will be omitted).
	struct destruct {
		// Any type must be destructible, there's no exception.
		template<typename U> static inline void 
		perform(bool& valueValid, char* dstBuffer, char* srcBuffer = nullptr) {
			assert(valueValid);
			((U*)dstBuffer) -> ~U();	
			valueValid = false;
		}
	};
	
	// Invoke the copy constructor.
	struct copyConstruct {
		// When the object is copy constructible, just perform copy construction.
		template<typename U> static inline 
		typename std::enable_if<std::is_copy_constructible<U>::value>::type
		perform(bool& valueValid, char* dstBuffer, char* srcBuffer) {
			assert(!valueValid);
			const U& copiedObject = *reinterpret_cast<const U*>(srcBuffer);
			new (dstBuffer) U(copiedObject);
			valueValid = true;
		}
		
		// When the object is not copy constructible but copy assignable, perform 
		// copy assignment instead.
		template<typename U> static inline 
		typename std::enable_if<	!std::is_copy_constructible<U>::value && 
									std::is_copy_assignable<U>::value	>::type
		perform(bool& valueValid, char* dstBuffer, char* srcBuffer) {	
			assert(!valueValid);
			const U& copiedObject = *reinterpret_cast<const U*>(srcBuffer);
			defaultConstruct::template perform<U>(valueValid, dstBuffer);
			if(valueValid) *((U*)dstBuffer) = copiedObject;
		}
	};
	
	// Invoke the move constructor.
	struct moveConstruct {
		// When the object is move constructible, just perform move construction.
		template<typename U> static inline
		typename std::enable_if<std::is_move_constructible<U>::value>::type
		perform(bool& valueValid, char* dstBuffer, char* srcBuffer) {
			assert(!valueValid);
			U& movedObject = *reinterpret_cast<U*>(srcBuffer);
			new (dstBuffer) U(std::move(movedObject));
			valueValid = true;
		}
		
		// When the object is not move constructible but move assignable, perform 
		// move assignment instead.
		template<typename U> static inline 
		typename std::enable_if<	!std::is_move_constructible<U>::value && 
									std::is_move_assignable<U>::value	>::type
		perform(bool& valueValid, char* dstBuffer, char* srcBuffer) {
			assert(!valueValid);
			U& movedObject = *reinterpret_cast<U*>(srcBuffer);
			defaultConstruct::template perform<U>(valueValid, dstBuffer);
			if(valueValid) *((U*)dstBuffer) = std::move(movedObject);
		}
	};
	
	// Invoke the copy assign. Can only be invoked when the source objeet presents, and 
	// source buffer and destination buffer's object are of same type.
	struct copyAssign {
		// When the object is copy constructible, just perform copy construction.
		template<typename U> static inline 
		typename std::enable_if<std::is_copy_assignable<U>::value>::type
		perform(bool& valueValid, char* dstBuffer, char* srcBuffer) {	
			assert(valueValid);
			const U& copiedObject = *reinterpret_cast<const U*>(srcBuffer);
			*((U*)dstBuffer) = copiedObject;
		}
		
		// When the object is not copy assignable but copy constructible, perform 
		// copy construction instead.
		template<typename U> static inline 
		typename std::enable_if<	!std::is_copy_assignable<U>::value && 
									std::is_copy_constructible<U>::value>::type
		perform(bool& valueValid, char* dstBuffer, char* srcBuffer) {	
			assert(valueValid);
			const U& copiedObject = *reinterpret_cast<const U*>(srcBuffer);
			destruct::template perform<U>(valueValid, dstBuffer);
			if(!valueValid) copyConstruct::template perform<U>(
					valueValid, dstBuffer, srcBuffer);
		}
	};
	
	// Invoke the copy assign, Can only be invoked when the source objeet presents, and 
	// source buffer and destination buffer's object are of same type.
	struct moveAssign {
		// When the object is move constructible, just perform move construction.
		template<typename U> static inline 
		typename std::enable_if<std::is_move_assignable<U>::value>::type
		perform(bool& valueValid, char* dstBuffer, char* srcBuffer) {
			assert(valueValid);
			U& movedObject = *reinterpret_cast<U*>(srcBuffer);
			*((U*)dstBuffer) = std::move(movedObject);
		}
		
		// When the object is not move assignable but move constructible, perform 
		// move construction instead.
		template<typename U> static inline 
		typename std::enable_if<	!std::is_move_assignable<U>::value && 
									std::is_copy_constructible<U>::value>::type
		perform(bool& valueValid, char* dstBuffer, char* srcBuffer) {	
			assert(valueValid);
			U& copiedObject = *reinterpret_cast<U*>(srcBuffer);
			destruct::template perform<U>(valueValid, dstBuffer);
			if(!valueValid) moveConstruct::template perform<U>(
				valueValid, dstBuffer, srcBuffer);
		}
	};
	
	// The reserve action of move assign, used for fallback actions.
	struct reverseMoveAssign {
		// Just swap src buffer and dst buffer's location.
		template<typename U> static inline void
		perform(bool& valueValid, char* dstBuffer, char* srcBuffer) {
			moveAssign::template perform<U>(valueValid, srcBuffer, dstBuffer);
		}
	};
};
 
/**
 * @brief The final block storing the union information.
 *
 * The final block specifies failure or termination conditions for 
 * template's specialization deduction.
 */
template <size_t maxOrdinal>
class McDtUnionInfo<maxOrdinal> {
public:
	/// Perform an action associated to the ordinal with the object.
	/// @brief ordinal The ordinal of the object to create.
	/// @brief dstBuffer The buffer of the object to update.
	/// @brief srcBuffer The buffer to the object serving as parameter.
	template<typename Action>
	inline static void byOrdinal(int32_t ordinal, bool& valueValid,
			char* dstBuffer, char* srcBuffer) {				
		// Ordinal must be greater than or equal the max ordinal this case.
		throw std::runtime_error("Union ordinal value exceeds boundary.");
	}
	
	/// Perform an action associated to the ordinal with the object.
	/// @brief ordinal The ordinal of the object to create.
	/// @brief args The user defined function parameters.
	template<typename UserAction, typename... Args>
	inline static void userByOrdinal(int32_t ordinal, Args... args) {
		// Ordinal must be greater than or equal the max ordinal this case.
		throw std::runtime_error("Union ordinal value exceeds boundary.");
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
	// Make sure that the T0 has a default constructor, being
	// default constructible and move constructible.
	static_assert(	std::is_copy_constructible<T0>::value || 
					std::is_copy_assignable<T0>::value, 
			"The specified type must be copy-constructible!");
	
	static_assert(	std::is_move_constructible<T0>::value ||
					std::is_move_assignable<T0>::value, 
			"The specified type must be move-constructible!");
	
	// Make sure that nullptr is not specified as parameter.
	static_assert(!std::is_same<std::nullptr_t, T0>::value,
			"The nullptr is reserved for special use in union.");
	
	// The next node of union information block.
	typedef McDtUnionInfo<baseOrdinal + 1, T...> next;
public:
	// Used by union to specify union related actions.
	template<typename Action>
	inline static void byOrdinal(int32_t ordinal, bool& valueValid,
				char* dstBuffer, char* srcBuffer = nullptr) {
		if(ordinal < baseOrdinal) 
			throw std::runtime_error("Union ordinal value exceeds boundary");
		else if(ordinal == baseOrdinal)
			Action::template perform<T0>(valueValid, dstBuffer, srcBuffer);
		else next::template byOrdinal<Action>(ordinal, valueValid, dstBuffer, srcBuffer);
	}

	// Used by user to specify their own by ordinal actions.
	template<typename UserAction, typename... Args>
	inline static void userByOrdinal(int32_t ordinal, Args... args) {
		if(ordinal < baseOrdinal) 
			throw std::runtime_error("Union ordinal value exceeds boundary");
		else if(ordinal == baseOrdinal)
			UserAction::template perform<T0>(args...);
		else next::template userByOrdinal<UserAction, Args...>(ordinal, args...);
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
	
	/// Copy constructor for the typed union object.
	McDtUnion(const McDtUnion& a): type(a.type), valueValid(false) {
		if(a.valueValid) 
			info.template byOrdinal<McDtUnionAction::copyConstruct>
				(type, valueValid, (char*)value, (char*)a.value);
	}
	
	/// Move constructor for the typed union object.
	McDtUnion(McDtUnion&& a): type(a.type), valueValid(false) {
		if(a.valueValid)
			info.template byOrdinal<McDtUnionAction::moveConstruct>
				(type, valueValid, (char*)value, (char*)a.value);
	}
	
	/// Move assignment for the typed union object.
	McDtUnion& operator=(McDtUnion&& a) {
		if(valueValid && type == a.type) {
			info.template byOrdinal<McDtUnionAction::moveAssign>
					(type, valueValid, (char*)value, (char*)a.value);
		}
		else {
			if(valueValid)
				info.template byOrdinal<McDtUnionAction::destruct>
					(type, valueValid, (char*)value);
			type = a.type;
			if(a.valueValid)
				info.template byOrdinal<McDtUnionAction::moveConstruct>
					(type, valueValid, (char*)value, (char*)a.value);
		}
	}
	
	/// Specially construction from another pointer with given ordinal.
	template<typename Action>
	McDtUnion(size_t ordinal, char* pointer, Action action): 
			type(ordinal), valueValid(false) {
		info.template byOrdinal<Action>(type, 
			valueValid, (char*)value, pointer);
	}
	
	/// Destructor for the typed union object.
	~McDtUnion() noexcept {
		if(valueValid) try {
			info.template byOrdinal<McDtUnionAction::destruct>
				(type, valueValid, (char*)value);
		} catch(const std::exception& ex) { /* Digest exception */ }
	}
	
	/// Update the union to make it store the object of provided ordinal.
	/// @brief typeOrdinal which type of object should be stored.
	void convertOrdinal(size_t typeOrdinal) {
		// No need to prepare for object this case.
		if(valueValid && typeOrdinal == type) return;
		
		// Destroy the previous object first.
		if(valueValid && typeOrdinal != type) {
			info.template byOrdinal<McDtUnionAction::destruct>
				(type, valueValid, (char*)value);
		}
		
		// Create a new object them.
		type = typeOrdinal;
		info.template byOrdinal<McDtUnionAction::defaultConstruct>
				(type, valueValid, (char*)value);
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
	
	/// Return that whether there's nothing held in the union.
	bool isNull() const { return !valueValid; }
	
	/// Assigning an immutable object to the object stored in the union.
	/// If the stored object is not of the assigend object's type, it 
	/// will be converted to the assigned object's type first.
	template<typename V>
	typename std::enable_if<!std::is_same<std::nullptr_t, V>::value, McDtUnion&>::type
	operator=(const V& copyValue) {
		char* copiedBuffer = (char*)(const_cast<V*>(&copyValue));
		if(!valueValid) {
			info.template byOrdinal<McDtUnionAction::copyConstruct>(
				(int32_t)(type = info.template ordinalOf<V>()), 
				valueValid, (char*)value, copiedBuffer);
		}
		else if(valueValid && type != info.template ordinalOf<V>()) {
			info.template byOrdinal<McDtUnionAction::destruct>(
				type, valueValid, (char*)value);
			info.template byOrdinal<McDtUnionAction::copyConstruct>(
				(int32_t)(type = info.template ordinalOf<V>()), 
				valueValid, (char*)value, copiedBuffer);
		}
		else {
			info.template byOrdinal<McDtUnionAction::copyAssign>(type, 
				valueValid, (char*)value, copiedBuffer);
		}
		return *this;
	}
	
	/// Assigning a mutable l-value object to the object stored in the union.
	template<typename V>
	typename std::enable_if<!std::is_same<std::nullptr_t, V>::value, McDtUnion&>::type
	operator=(V& copiedValue) {
		return *this = (const_cast<const V&>(copiedValue));
	}
	
	/// Assigning a mutable r-value object to the object stored in the union.
	template<typename V>
	typename std::enable_if<!std::is_same<std::nullptr_t, V>::value, McDtUnion&>::type
	operator=(V&& movedValue) {
		char* movedBuffer = (char*)(const_cast<V*>(&movedValue));
		if(!valueValid) {
			info.template byOrdinal<McDtUnionAction::moveConstruct>(
				(int32_t)(type = info.template ordinalOf<V>()),
				valueValid, (char*)value, movedBuffer);
		}
		else if(valueValid && type != info.template ordinalOf<V>()) {
			info.template byOrdinal<McDtUnionAction::destruct>(
				type, valueValid, (char*)value);
			info.template byOrdinal<McDtUnionAction::moveConstruct>(
				(int32_t)(type = info.template ordinalOf<V>()), 
				valueValid, (char*)value, movedBuffer);
		}
		else {
			info.template byOrdinal<McDtUnionAction::moveAssign>(type, 
				valueValid, (char*)value, movedBuffer);
		}
		return *this;
	}
	
	// Perform destruction when assigning nullptr to the union.
	template<typename V>
	typename std::enable_if<std::is_same<std::nullptr_t, V>::value, McDtUnion&>::type
	operator=(V&& movedValue) {
		info.template byOrdinal<McDtUnionAction::destruct>(
				type, valueValid, (char*)value);
		return *this;
	}
	
	/// Retrieve the current ordinal of the stored types.
	size_t ordinal() const { return type; }
	
	/// Type-casting method of this class. Which wraps the asType<V>() 
	/// method and will throw exception when failing to cast.
	template<typename V> operator const V&() const { return asType<V>(); }
	
	/// Type-casting method of this class, just like the constant 
	/// casting version.
	template<typename V> operator V&() { return asType<V>(); }
	
	/// Still type casting, in the flavour of functor operation.
	template<typename V> const V& operator()() const { return asType<V>(); }
	template<typename V> V& operator()() { return asType<V>(); }
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