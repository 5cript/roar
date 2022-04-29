#pragma once

/**
 * @brief Utility function for rule of 5 abidance in pimpl classes.
 */
#define ROAR_PIMPL_SPECIAL_FUNCTIONS(Name) \
    ~Name(); \
    Name(Name const&) = delete; \
    Name(Name&&); \
    Name& operator=(Name const&) = delete; \
    Name& operator=(Name&&)

/**
 * @brief Utility function for rule of 5 abidance in pimpl classes.
 */
#define ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(Name) \
    Name::~Name() = default; \
    Name::Name(Name&&) = default; \
    Name& Name::operator=(Name&&) = default

/**
 * @brief Utility function for rule of 5 abidance in pimpl classes.
 */
#define ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL_NO_DTOR(Name) \
    Name::Name(Name&&) = default; \
    Name& Name::operator=(Name&&) = default