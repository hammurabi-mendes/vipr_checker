#ifndef BASIC_TYPES_H
#define BASIC_TYPES_H

#include <string.h>

using std::string;

// TODO: Change constructors/destructors after I understand the permanence requirements
//       Also, create a linear allocator for strings
struct Number {
	char *numerator;
	char *denominator;

	bool is_integral;
	bool is_positive_infinity;
	bool is_negative_infinity;

	Number(): numerator{"0"}, denominator{"1"}, is_integral{true}, is_positive_infinity{false}, is_negative_infinity{false} {}

	Number(char *integral): numerator{integral}, denominator{"1"}, is_integral(true), is_positive_infinity{false}, is_negative_infinity{false} {}
	Number(char *numerator, char *denominator): numerator{numerator}, denominator{denominator}, is_integral(false), is_positive_infinity{false}, is_negative_infinity{false} {}

	inline string get_string() noexcept {
		if(is_integral) {
			return string(numerator);
		}
		else {
			return string(numerator) + "/" + string(denominator);
		}
	}

	inline bool is_zero() {
		return (strcmp(numerator, "0") == 0);
	}
};

#endif /* BASIC_TYPES_H */