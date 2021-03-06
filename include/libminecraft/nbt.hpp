#pragma once
/**
 * @file libminecraft/nbt.hpp
 * @brief The minecraft nbt data type
 * @author Haoran Luo
 *
 * Defines data types related to minecraft nbt format, providing 
 * representations operations and to manipulate them.
 */
#include "libminecraft/union.hpp"
#include "libminecraft/iobase.hpp"
#include "libminecraft/markable.hpp"
#include "libminecraft/member.hpp"
#include <unordered_map>
#include <memory>
#include <cstring>
 
// Predefine types that is an nbt item but recursive hold one.
// See concrete types documentation for detailed information.
class McDtNbtCompound;
class McDtNbtList;

/**
 * @brief Defines types that could be stored as an nbt payload, for
 * further usage of nbt payload in mc::nbtcompound and mc::nbtlist,
 * see also mc::cunion.
 */
typedef mc::cuinfo<
	// The signed integer types.
	mc::s8, mc::s16,
	mc::s32, mc::s64,

	// The IEEE-754 floating point types.
	mc::f32, mc::f64,

	// Special mc::s8 array with mc::s32 prefix.
	mc::array<mc::s8, mc::s32>,
	
	// The utf-8 string prefixed with mc::u16 prefix.
	mc::jstring,
	
	// Complex objects made of payloads.
	McDtNbtList, McDtNbtCompound,

	// Special integer arrays with mc::s32 prefix.
	mc::array<mc::s32, mc::s32>, mc::array<mc::s64, mc::s32>
> McDtNbtTagEnum;

// Declare the mc::nbtinfo (constexpr) into the mc namespace.
namespace mc {
static constexpr McDtNbtTagEnum nbtinfo = McDtNbtTagEnum();
} // End of putting nbtinfo into mc namespace.

/// @brief The payload of an nbt item, which should be identified by 
/// the tag index before the nbt's tag name. 
typedef McDtUnion<McDtNbtTagEnum> McDtNbtPayload;

/// @brief The nbt compound storing key-value data in nbt format.
class McDtNbtCompound {
public:	
	/// @brief The nbt compound data serving as access points to entries in 
	/// the nbt compound's accessing point.
	class McDtNbtCompoundData {
		// The basic constructor and destructors.
		McDtNbtCompoundData();
		McDtNbtCompoundData(const McDtNbtCompoundData& block);
		McDtNbtCompoundData(McDtNbtCompoundData&& block);
		~McDtNbtCompoundData();
		
		// This class cannot be constructed by user, and it could be only
		// constructed by its container classes.
		friend class McDtNbtCompound;
		friend class std::pair<std::u16string, McDtNbtCompoundData>;
		friend class std::pair<const std::u16string, McDtNbtCompoundData>;
	public:
		// The methods reflecting the underlying payload instance.
		operator McDtNbtPayload&();
		operator const McDtNbtPayload&() const;
		McDtNbtPayload* operator->();
		const McDtNbtPayload* operator->() const;
		
		// The method reflecting payload's assignment methods.
		template<typename U> McDtNbtCompoundData& operator=(const U&);
		template<typename U> McDtNbtCompoundData& operator=(U&&);
		McDtNbtCompoundData& operator=(const char16_t*);
		
		static constexpr size_t payloadDataBlockSize = 64;
	private:
		char payloadData[payloadDataBlockSize];
	};
private:
	typedef std::unordered_map<std::u16string, McDtNbtCompoundData> nbtMapType;
	std::unique_ptr<nbtMapType> entries;
public:
	typedef McDtNbtCompound type;

	// Constructors for an NbtCompound.
	McDtNbtCompound(): entries(new nbtMapType) {}
	McDtNbtCompound(const McDtNbtCompound& a): 
			entries(new nbtMapType(*a.entries)) {}
	McDtNbtCompound(McDtNbtCompound&& a) noexcept: entries() {
		using std::swap;
		swap(entries, a.entries);
	}
	
	// Assignment operators for NbtCompound.
	McDtNbtCompound& operator=(const McDtNbtCompound& a) {
		*entries = *a.entries;
		return *this;
	}
	
	McDtNbtCompound& operator=(McDtNbtCompound& a) {
		return (*this) = const_cast<McDtNbtCompound&>(a);
	}
	
	McDtNbtCompound& operator=(McDtNbtCompound&& a) {
		using std::swap;
		swap(entries, a.entries);
	}
	
	// Delegates the indexed-by-key method.
	McDtNbtCompoundData& operator[](const std::u16string& key) 
		{	return (*entries)[key];	}
	const McDtNbtCompoundData& operator[](const std::u16string& key) const 
		{	return (*entries)[key];	}
		
	// Delegate the erase() and count() method.
	bool erase(const std::u16string& key) { return entries -> erase(key); }
	size_t count(const std::u16string& key) { return entries -> count(key); }
	
	// The iterator and const iterator methods.
	typedef nbtMapType::iterator iterator;
	typedef nbtMapType::const_iterator const_iterator;
	iterator begin()				{	return entries -> begin();		}
	iterator end()					{	return entries -> end();		}
	const_iterator cbegin() const	{	return entries -> cbegin();		}
	const_iterator cend() 	const	{	return entries -> cend();		}
};
static_assert(	std::is_copy_constructible<McDtNbtCompound>::value &&
				std::is_move_constructible<McDtNbtCompound>::value,
	"McDtNbtCompound should be both copy and move constructible!");

/**
 * @brief The nbt list storing single types of nbt data.
 * Please notice that once the list is constructed, the type of nbt data would 
 * never change, though you can alter its content.
 *
 * The std::vector cannot be used because it is allocator aware and does not 
 * allow variant length element in it. It may wrong memory allocation or deallocation 
 * when resizing vector content.
 */
class McDtNbtList {
	std::unique_ptr<char[]> items; ///< The actual data storage in the list.
	size_t m_length;               ///< The effective elements in the list.
	size_t m_capacity;             ///< The reserved memory (in number of elements).
	const size_t ordinal;          ///< Which type is the nbt list holding.
	const size_t stride;           ///< The size of every nbt element in the list.
	const bool isTrivial;          ///< Is the nbt element primitives.
	
	// Find the i-th nbt element in the list.
	inline const char* indexBlock(size_t index) const 
		{	return items.get() + index * stride;	}
	inline char* indexBlock(size_t index) 
		{	return items.get() + index * stride;	}
	
	// Perform std::vector style allocation.
	template<typename allocateAction, typename 
				fallbackAction = McDtUnionAction::destruct>
	inline void allocate(size_t size, char* dstBuffer, char* srcBuffer) {
		size_t i; try {
			// Perform block allocation.
			for(i = 0; i < size; ++ i) {
				bool valueValid = false;
				mc::nbtinfo.template byOrdinal<allocateAction>(
					(int32_t)ordinal, valueValid, 
					dstBuffer + i * stride, srcBuffer + i * stride);
			}
		} catch(std::exception& ex) {
			// Destroy already allocated elements.
			for(i = i - 1; i >= 0; ++ i) {
				bool valueValid = true;
				mc::nbtinfo.template byOrdinal<fallbackAction>(
					(int32_t)ordinal, valueValid, 
					dstBuffer + i * stride, srcBuffer + i * stride);
			}
			throw ex;
		}
	}
public:
	// Forwarded to the templates requring type entry.
	typedef McDtNbtList type;

	// Basic std::vector like operations.
	inline size_t size() const { return m_length; }
	inline size_t capacity() const { return m_capacity; }
	
	/// Reserve enough space for the container.
	void reserve(size_t newCapacity) {
		// Do nothing.
		if(capacity() >= newCapacity) return;
		
		// Allocate more space for the new items.
		std::unique_ptr<char[]> newBuffer(new char[newCapacity * stride]);
		if(!isTrivial) allocate<
			McDtUnionAction::moveConstruct, 
			McDtUnionAction::reverseMoveAssign>
				(m_length, newBuffer.get(), items.get());
		else memcpy(newBuffer.get(), items.get(), m_length * stride);
		
		// Safely swap and update the capacity.
		std::swap(newBuffer, items);
		m_capacity = newCapacity;
	}
	
	// Resize the container, either by default construct or destruct.
	void resize(size_t newSize) {
		if(m_length == newSize) return;
		else if(m_length < newSize) {
			// Allocate more empty objects (if not trivial).
			reserve(newSize);
			if(isTrivial) memset(indexBlock(m_length), 0, 
				(newSize - m_length) * stride);
			else allocate<McDtUnionAction::defaultConstruct>
				(newSize - m_length, indexBlock(m_length), nullptr);
			m_length = newSize;
		}
		else {
			// Destroy trailing objects (if not trivial).
			if(!isTrivial) for(; m_length > newSize; -- m_length) {
				bool valueValid = true;
				mc::nbtinfo.template byOrdinal<McDtUnionAction::destruct>(
				(int32_t)ordinal, valueValid, indexBlock(m_length - 1), nullptr);
			}
			else m_length = newSize;
		}
	}
	
	// Don't construct through this method if you do want some useful data type.
	McDtNbtList(): items(), m_length(0), m_capacity(0), 
		ordinal(SIZE_MAX), stride(1), isTrivial(true) {}
	
	// Copy status of other another nbt list.
	McDtNbtList(const McDtNbtList& a): items(new char[a.m_capacity * a.stride]), 
		m_length(a.m_length), m_capacity(a.m_capacity),
		ordinal(a.ordinal), stride(a.stride), isTrivial(a.isTrivial) {
		
		if(isTrivial) memcpy(items.get(), a.items.get(), a.size() * stride);
		else allocate<McDtUnionAction::copyConstruct>(m_length, items.get(), 
				const_cast<char*>(a.items.get()));
	}
	
	// Move status of another nbt list, because the union's object is newed just
	// on another object's vector, you can safely swap them.
	McDtNbtList(McDtNbtList&& a) noexcept: items(), ordinal(a.ordinal), 
			m_length(a.m_length), m_capacity(a.m_capacity),
			stride(a.stride), isTrivial(a.isTrivial) {
			
			std::swap(items, a.items);
			a.items = NULL;
			a.m_length = 0;
			a.m_capacity = 0;
	}
	
	// Destroy the object, based on the data stored.
	~McDtNbtList() {
		if(items != NULL && !isTrivial) for(size_t i = 0; i < size(); ++ i) {
			bool valueValid = true;
			mc::nbtinfo.template byOrdinal<McDtUnionAction::destruct>(
				(int32_t)ordinal, valueValid, indexBlock(i), nullptr);
			valueValid = false;
		}
	}
	
	// Copy construct from the generic type.
	template<typename V>
	McDtNbtList(const std::vector<V>& v): 
		items(new char[v.capacity() * sizeof(V)]), 
		m_length(v.size()), m_capacity(v.capacity()),
		ordinal(mc::nbtinfo.ordinalOf<V>()), stride(sizeof(V)), 
		isTrivial(std::is_fundamental<typename V::type>::value) {
		
		if(isTrivial) memcpy(items.get(), reinterpret_cast<const char*>(v.data()), m_length * stride);
		else allocate<McDtUnionAction::copyConstruct>(m_length, items.get(), 
				const_cast<char*>(reinterpret_cast<const char*>(v.data())));
	}
	
	// Move construct for the generic type.
	template<typename V> McDtNbtList(std::vector<V>&& v):
		items(new char[v.capacity() * sizeof(V)]), 
		m_length(v.size()), m_capacity(v.capacity()),
		ordinal(mc::nbtinfo.ordinalOf<V>()), stride(sizeof(V)), 
		isTrivial(std::is_fundamental<typename V::type>::value) {
		
		if(isTrivial) memcpy(items.get(), reinterpret_cast<const char*>(v.data()), m_length * stride);
		else allocate<McDtUnionAction::moveConstruct>(m_length, items.get(), 
				const_cast<char*>(reinterpret_cast<const char*>(v.data())));
	}
	
	/// Swap with another nbt list.
	void swap(McDtNbtList& rlist) noexcept {
		using std::swap;
		swap(items, rlist.items);
		swap(m_length, rlist.m_length);
		swap(m_capacity, rlist.m_capacity);
		swap(const_cast<size_t&>(ordinal), const_cast<size_t&>(rlist.ordinal));
		swap(const_cast<size_t&>(stride), const_cast<size_t&>(rlist.stride));
		swap(const_cast<bool&>(isTrivial), const_cast<bool&>(rlist.isTrivial));
	}
	
	/// @brief Providing typed control to the nbt list's internal data. Please 
	/// notice this instance could never change the internal type of nbt list.
	template<typename V>
	struct McDtNbtListAccessor {
		McDtNbtList& list;
		
		/// @brief Construct an instance of the accessor, serving as accessing
		/// point of internal object's data.
		/// @throw when the data type in the list is not of the specified type.
		McDtNbtListAccessor(McDtNbtList& list): list(list) {
			if(mc::nbtinfo.template ordinalOf<V>() != list.ordinal) throw std::runtime_error(
				"The element type of the list is not of specified type.");
		}
		
		// The number subscription indexing methods.
		V& operator[](size_t i) {
			return *((V*)list.indexBlock(i));
		}
		
		const V& operator[](size_t i) const {
			return *((const V*)list.indexBlock(i));
		}
		
		/// Shadowed operations from the list.
		size_t size() const { return list.size(); }
		size_t capacity() const { return list.capacity(); }
		size_t reserve(size_t newCapacity) { list.reserve(newCapacity); }
		size_t resize(size_t newSize) { list.resize(newSize); }
		
		// The modification method.
		void push_back(const V& v) {
			// Reserve more memory for the element.
			reserve(size() + 1);
			
			// Copy construct the element.
			if(list.isTrivial) {
				*((V*)list.indexBlock(list.m_length)) = v;
				list.m_length ++;
			}
			else try {
				bool valueValid = false;
				mc::nbtinfo.template byOrdinal<McDtUnionAction::copyConstruct>(
					mc::nbtinfo.template ordinalOf<V>(), valueValid, 
					list.indexBlock(list.m_length), 
					reinterpret_cast<char*>(&const_cast<V&>(v)));
				list.m_length ++;
			}
			catch(const std::exception& ex) {
				throw ex;
			}
		}
		
		// Special overload for mc::jstring.
		void push_back(const char16_t* v) {
			if(mc::nbtinfo.template ordinalOf<mc::jstring>() != list.ordinal) 
				throw std::runtime_error("The element type of the "
					"list is not of specified type.");
			else push_back(mc::jstring(v));
		}
		
		// Iterator methods and const iterator methods.
		V* begin()				{	return (V*)list.indexBlock(0); 					}
		V* end()				{	return (V*)list.indexBlock(list.size()); 		}
		const V* cbegin() const {	return (const V*)list.indexBlock(0); 			}
		const V* cend()	const 	{	return (const V*)list.indexBlock(list.size()); 	}
	};
	
	// Get a typed accessor of the list.
	template<typename V>
	McDtNbtListAccessor<V> asType() {
		return McDtNbtListAccessor<V>(*this);
	}
	
	/// Get a copied union from the array as an element.
	McDtNbtPayload operator[](size_t i) const {
		return McDtNbtPayload(ordinal, const_cast<char*>(indexBlock(i)), 
				McDtUnionAction::copyConstruct());
	}
};
static_assert(	std::is_copy_constructible<McDtNbtList>::value &&
				std::is_move_constructible<McDtNbtList>::value,
	"McDtNbtList should be both copy and move constructible!");

// Provide std operation specification for nbt classes.
namespace std {
	/// Define the std::swap operation for nbt list.
	inline void swap(McDtNbtList& llist, McDtNbtList& rlist) {
		llist.swap(rlist);
	}
}
	
/// @brief The nbt item, holding a tag, a tag name, and the payload.
typedef std::pair<mc::jstring, McDtNbtPayload> McDtNbtItem;

/// @brief Data type template placeholder, indicating the input or output stream
/// should treat the underlying data type as nbt, which has its own distinct format.
class McDtFlavourNbt;
namespace mc {		// Forward of the minecraft namespace.
typedef McDtNbtCompound nbtcompound;
typedef McDtDataType<McDtNbtItem, McDtFlavourNbt> nbtitem;

typedef McDtNbtList nbtlist;
template<typename V>
using nbttypedlist = nbtlist::McDtNbtListAccessor<V>;

template<typename I>
using nbtintarray = mc::array<I, mc::s32>;
};					// End of forwarded minecraft namespace.

// Begin to tell us what to do with the payloadData block.
// This block are defined after the size of payload data could be determined.
static_assert(sizeof(McDtNbtPayload) <= McDtNbtCompound::McDtNbtCompoundData
	::payloadDataBlockSize, "The payload data block is not big to hold payload content.");
inline McDtNbtCompound::McDtNbtCompoundData::McDtNbtCompoundData() 
	{	new (payloadData) McDtNbtPayload;		}
inline McDtNbtCompound::McDtNbtCompoundData::McDtNbtCompoundData(const McDtNbtCompoundData& block) 
	{	new (payloadData) McDtNbtPayload((const McDtNbtPayload&)block);	};
inline McDtNbtCompound::McDtNbtCompoundData::McDtNbtCompoundData(McDtNbtCompoundData&& block) 
	{	new (payloadData) McDtNbtPayload(std::move((McDtNbtPayload&&)block));	};
inline McDtNbtCompound::McDtNbtCompoundData::~McDtNbtCompoundData() 
	{	((McDtNbtPayload*)payloadData) -> ~McDtNbtPayload();	}
inline McDtNbtCompound::McDtNbtCompoundData::operator McDtNbtPayload&() 
	{	return *reinterpret_cast<McDtNbtPayload*>(payloadData);	}
inline McDtNbtCompound::McDtNbtCompoundData::operator const McDtNbtPayload&() const
	{	return *reinterpret_cast<const McDtNbtPayload*>(payloadData);	}
inline McDtNbtPayload* McDtNbtCompound::McDtNbtCompoundData::operator->()
	{	return reinterpret_cast<McDtNbtPayload*>(payloadData);	}
inline const McDtNbtPayload* McDtNbtCompound::McDtNbtCompoundData::operator->() const
	{	return reinterpret_cast<const McDtNbtPayload*>(payloadData);	}
	
// Special assignment methods to make it just like its payload counter part.
template<typename U> McDtNbtCompound::McDtNbtCompoundData& 
McDtNbtCompound::McDtNbtCompoundData::operator=(const U& v) {
	((McDtNbtPayload&)*this) = v;
	return *this;
}

inline McDtNbtCompound::McDtNbtCompoundData& 
McDtNbtCompound::McDtNbtCompoundData::operator=(const char16_t* v) {
	((McDtNbtPayload&)*this) = mc::jstring(v);
	return *this;
}

template<typename U> McDtNbtCompound::McDtNbtCompoundData& 
McDtNbtCompound::McDtNbtCompoundData::operator=(U&& v) {
	((McDtNbtPayload&)*this) = std::forward<U>(v);
	return *this;
}

// Some I/O methods for other modules to work with nbt.
// Please notice that mc::nbtitem::read and mc::nbtitem::write are already there for 
// reading/writing wildcard nbt data. But usually we would prefer SAX-style processor, 
// for compacting size and improve runtime proessing efficiency.
void McIoReadNbtCompound(McIoInputStream&, mc::nbtcompound& compound);
void McIoReadNbtList(McIoInputStream&, mc::nbtlist& list);

// Forwarded I/O methods, for easier to comprehend.
inline McIoInputStream& operator>>(McIoInputStream& inputStream, mc::nbtcompound& compound) {
	McIoReadNbtCompound(inputStream, compound);
	return inputStream;
}
inline McIoInputStream& operator>>(McIoInputStream& inputStream, mc::nbtlist& list) {
	McIoReadNbtList(inputStream, list);
	return inputStream;
}

// Completely skip up to given elements so that the input stream's read pointer points
// to the next element.
void McIoSkipNbtElement(McIoInputStream&, mc::s8 type);

/// The SAX processor action corresponding to strongly typed tag.
struct McIoNbtCompoundSaxAction {
	/// The type is calculates as follow:
	/// - When expectedType is between 0 and 12, it refers to the basic nbt types, and 
	/// for mc::nbtlist, it should accept any list.
	/// - When expectedType is between 13 and 25, it refers to the mc::nbttypedlist 
	/// with corresponding ordinal, and the list would be ignored if the internal 
	/// element type mismatches the expectedType.
	size_t expectedType;
	
	/// Defines the actions when the tag completes its typed checking.
	/// If the expectedType is between 0 and 12, the inputStream's pointer should 
	/// be right after the tag name.
	/// If the expectedType is between 13 and 25, the inputStream's pointer should 
	/// be after the tag type. (pointing to the first byte of list length).
	/// @warning this pointer may not be null!
	void (*tagPresent)(McIoMarkableStream& inputStream, void* data, void* ud);
	
	/// Defines prerequisites of invoking tagPresent method.
	/// It is presented in a list of compound index, with count followed.
	size_t numPrerequisites;	size_t* prerequisites;
	
	/// Invoked when the entry cannot be found, may be null.
	void (*tagAbsent)(void* data, void* ud);
	
	/// Invoked when the tag is present but its prerequisites cannot be resolved.
	/// The input stream will be still be set to the place of the read tag.
	/// May be null and will not be invoked if so.
	void (*tagFailedResolve)(McIoMarkableStream& inputStream, void* data, void* ud);
	
	/**
	 * @brief Initialize the sax result into a member of an instance.
	 */
	template<typename memberType, bool mandatory = false>
	static constexpr McIoNbtCompoundSaxAction forMember() {
		typedef typename memberType::fieldType fieldType;
		typedef typename memberType::classType classType;
		
		static_assert(mc::nbtinfo.ordinalOf<fieldType>() 
			!= mc::nbtinfo.ordinalOf<mc::nbtlist>(),
			"The field type mc::nbtlist is not a common type.");
		static_assert(mc::nbtinfo.ordinalOf<fieldType>() 
			!= mc::nbtinfo.ordinalOf<mc::nbtcompound>(),
			"The field type mc::nbtlist is not a common type.");
		return { 
			// Expected to be the ordinal of the field type.
			.expectedType = mc::nbtinfo.ordinalOf<fieldType>(), 
			
			// Copy the result into the place by pointer.
			.tagPresent = [] (McIoMarkableStream& inputStream, 
					void* data, void* ud) -> void  {

				// Read into the tag stream.
				fieldType& memberReference = memberType::dereference(data);
				inputStream >> memberReference;
			}, 
			
			// Common field of instance should not have any prerequisites.
			.numPrerequisites = 0, .prerequisites = nullptr,
			
			// Depending on whether is mandatory, throw exception or do nothing.
			.tagAbsent = [] (void* data, void* ud) -> void {
				if(mandatory) throw std::runtime_error(
					"The field is mandatory but not found.");
			},
			
			// We don't have to resolve prerequisites, so no more fail resolve.
			.tagFailedResolve = nullptr 
		};
	}
	
	/**
	 * @brief Initialize the sax result into a vector member of an instance, where
	 * the input type is assumed to be an nbt list, with component type the same
	 * as the vector's component type.
	 *
	 * This is only applicable to member of std::vector<>, and the component type
	 * must be one from the mc::nbtinfo.
	 */
	template<typename memberType, bool mandatory = false>
	static constexpr McIoNbtCompoundSaxAction forVectorMember() {
		typedef typename memberType::fieldType::value_type componentType;
		static_assert(std::is_same<typename memberType::fieldType,
			std::vector<componentType> >::value, "The specified member must be a vector.");
		
		return {
			// Expected to be the value of component's type.
			.expectedType = 13 + mc::nbtinfo.ordinalOf<componentType>(),
			
			// Copy the result into the place by pointer.
			.tagPresent = [] (McIoMarkableStream& inputStream,
					void* data, void* ud) {
				
				// Read the length of vector first.
				mc::s32 vectorLength = 0;
				inputStream >> vectorLength;
				if(vectorLength <= 0) return;
				
				// Loop reading the content into the field.
				std::vector<componentType>& vectorField = memberType::dereference(data);
				vectorField.reserve(vectorLength);
				for(size_t i = 0; i < vectorLength; ++ i) {
					componentType componentValue;
					inputStream >> componentValue;
					vectorField.push_back(std::move(componentValue));
				}
			},
			
			// Vector field of instance should (also) not have any prerequisites.
			.numPrerequisites = 0, .prerequisites = nullptr,
			
			// Depending on whether is mandatory, throw exception or do nothing.
			.tagAbsent = [] (void* data, void* ud) -> void {
				if(mandatory) throw std::runtime_error(
					"The vector field is mandatory but not found.");
			},
			
			// We don't have to resolve prerequisites, so no more fail resolve.
			.tagFailedResolve = nullptr 
		};
	}
	
	/**
	 * @brief Initialize the sax result into a bit field, the field in the 
	 * compound is always mc::s8, however can be any integral type when in 
	 * the objects representations.
	 */
	template<typename memberType, typename memberType::fieldType mask>
	static constexpr McIoNbtCompoundSaxAction forBitField() {
		typedef typename memberType::fieldType bitfieldType;
		static_assert(std::is_integral<bitfieldType>::value,
			"The bit field's type should always be an integral.");
		
		return {
			// Expected to be mc::s8.
			.expectedType = mc::nbtinfo.ordinalOf<mc::s8>(),
			
			// Copy the result into the bitfield.
			.tagPresent = [] (McIoMarkableStream& inputStream,
					void* data, void* ud) {
						
				// Read the tag first.
				mc::s8 bitValue; inputStream >> bitValue;
				if(bitValue != 0 && bitValue != 1)
					throw std::runtime_error("Boolean value can only be 0 or 1.");
				
				// Place that into the bit field.
				bitfieldType& bitfield = memberType::dereference(data);
				bitfield = bitfield | mask;
				if(bitValue == 0) bitfield = bitfield ^ mask;
			},
			
			// Bitfield of instance should not have prerequisites.
			.numPrerequisites = 0, .prerequisites = nullptr,
			
			// Bit field is usually not mandatory, and set to 0 if not present.
			.tagAbsent = [] (void* data, void* ud) -> void {
				// Clear bit field here.
				bitfieldType& bitfield = memberType::dereference(data);
				bitfield = (bitfield | mask) ^ mask;
			},
			
			// We don't have prerequisites here, so do nothing.
			.tagFailedResolve = nullptr
		};
	}
};

/// The maximum length of a tag name while being processed in SAX mode, when the 
/// value exceeds this length, it will simply be ignored, even if could be looked
/// up in the user dictionary.
static const size_t McIoSaxMaxNbtTagNameLength = 64;

/**
 * @brief Perform SAX-style reading of an nbt compound.
 *
 * When the proecssor encounters a compound's name tag (shorter than the tag name
 * length should be ensured), it will first look it up in the dictionary, when the 
 * returned index is invalid, it will ignore it, depending on whether ignoredTag compound 
 * is present, it will either skip the data or place the data into the compound.
 *
 * If the returned index is valid, it will look up the saxActions, and compare 
 * the expectedType with actualType, then skip or place it in the ignoredTag if
 * the type mismatches.
 *
 * If the type matches, it will then check for the prerequisites, and mark the 
 * position if not all prerequisites are met. Else the tagPresent will be invoked,
 * in which user might copy the data into actual struct, or more verbose processing.
 *
 * It is guaranteed that when tagPresent is invoked, the inputStream is right in
 * the location of the data part of a tag.
 *
 * @warning The tag name is presented in utf-8 format, which is different from the type 
 * stored in the mc::nbtcompound's key and value. As the tag in minecraft are always 
 * in English, don't convert to utf-16 will eliminate unnecessary logic and simplify 
 * the logic.
 *
 * @param[inout] inputStream the stream to perform SAX reading from, must support mark.
 * @param[inout] data the currently processing data.
 * @param[inout] ud the userdata to call tag present, hopefully some control information.
 * @param[in] dictionary the function to lookup an encountered tag, hopefully a perfect
 * hash function. Tag would be ignored when returned value < 0 or >= tagLength.
 * @param[in] numSaxActions indicates number of saxActions followed.
 * @param[in] saxActions the sax actions to perform.
 * @param[in] ignoredTag when the items are ignore, it might be placed here.
 */
void McIoSaxNbtCompound(McIoMarkableStream& inputStream, void* data, void* ud,
	int (*dictionary)(size_t tagLength, const char* tagName),
	size_t numSaxActions, const McIoNbtCompoundSaxAction* const saxActions,
	McDtNbtCompound* ignoredTag = nullptr);

// Forward the nbt sax action as mc::nbtsax.
namespace mc {
typedef McIoNbtCompoundSaxAction nbtsax;
} // End of namespace mc.