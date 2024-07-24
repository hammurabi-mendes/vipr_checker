#ifndef BASIC_TYPES_H
#define BASIC_TYPES_H

using std::string;

// TODO: Change constructors/destructors after I understand the permanence requirements
//       Also, create a linear allocator for strings
struct Number {
	char *numerator;
	char *denominator;

	bool is_integral;
	bool is_positive_infinity;
	bool is_negative_infinity;
	bool is_nan;

	Number(): numerator{"0"}, denominator{"1"}, is_integral{true} {}

	Number(char *integral): numerator{integral}, denominator{"1"}, is_integral(true) {}
	Number(char *numerator, char *denominator): numerator{numerator}, denominator{denominator}, is_integral(false) {}

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