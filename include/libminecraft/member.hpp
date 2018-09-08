#pragma once
/**
 * @file libminecraft/member.hpp
 * @brief libminecraft member pointer workaround
 * @author Haoran Luo
 *
 * This file is used to work around with the member pointers working
 * with a template.
 *
 * To specify a member pointer to a template, we need to know:
 * - classType: which class are we calling.
 * - fieldType: in which type is the member.
 * - member: the member pointer to the field.
 *
 * Given a function like:
 * template<typename classType, typename fieldType, 
 *     fieldType classType::*member> foo();
 *
 * To invoke foo() with specialization, saying field int A::bar,
 * we will have to write like: foo<A, int, &A::bar>(), which is 
 * actually quite verbose and template deduction cannot solve
 * such situation as merely specifying the member pointer in
 * template list is contextless.
 *
 * The workaround in this file is done by specifying an actual
 * type, storing essential elements of a member pointer, and 
 * other modules of libminecraft is to work with such type.
 *
 * @see https://stackoverflow.com/questions/15148749
 */

/**
 * @brief The class prototype to work around with member pointers in 
 * our templates.
 */
template<typename ClassType, typename FieldType, 
	FieldType ClassType::*MemberPointer> struct McDtMemberType {
	
	/// The forwarded class containing the member.
	typedef ClassType classType;
	
	/// The forwarded type in which the member is declared.
	typedef FieldType fieldType;
	
	/// The forwarded member pointer.
	static constexpr FieldType ClassType::*member = MemberPointer;
	
	/// Dereference the field from a void pointer.
	static FieldType& dereference(void* ptr) {
		return (*reinterpret_cast<ClassType*>(ptr)).*member;
	}
	
	/// Dereference the field from a constant void pointer.
	static const FieldType& dereference(const void* ptr) {
		return (*reinterpret_cast<const ClassType*>(ptr)).*member;
	}
};

/// The macro which convert a (type, member) pair into a McDtMemberType
/// specialization. You could specify it to templates requiring memberType.
#ifdef __mc_member
#ifndef __mc_member_override
static_assert(false, "The __mc_member which is a work around has already "
"been defined, specify __mc_member_override if you know what could happend"
" and still decide to override it.");
#endif
#else 
#define __mc_member(type, member)\
	McDtMemberType<type, decltype(type::member), &type::member>
#endif