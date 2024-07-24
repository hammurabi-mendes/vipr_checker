#include <cstdio>
#include <cstdlib>

#include <stdexcept>
#include <format>

#include <vector>

#include <unistd.h>
#include <fcntl.h>

#include "parser.h"
#include "certificate.h"

using std::string;
using std::format;

using std::runtime_error;

using std::vector;

inline void read_coefficients_with_size(Parser &parser, vector<Number> &coefficients, long number_coefficients) {
	for(unsigned long i = 0; i < number_coefficients; i++) {
		unsigned long index = parser.get_unsigned_long();
		Number coefficient = parser.get_number();

		coefficients[index] = coefficient;
	}
}

inline void read_coefficients(Parser &parser, vector<Number> &coefficients) {
	unsigned long number_coefficients = parser.get_unsigned_long();

	read_coefficients_with_size(parser, coefficients, number_coefficients);
}

inline Constraint read_constraint(Parser &parser, unsigned long number_variables, vector<Number> &objective_coefficients) {
	char *name = parser.get_stable_string(parser.get_token());
	char *direction_string = parser.get_token();

	Direction direction;

	if(strcmp(direction_string, "E") == 0) {
		direction = Direction::Equal;
	}
	else if(strcmp(direction_string, "L") == 0) {
		direction = Direction::SmallerEqual;
	}
	else if(strcmp(direction_string, "G") == 0) {
		direction = Direction::GreaterEqual;
	}
	else {
		throw runtime_error(format("Expected valid direction in line {}\n", parser.get_line_number()));
	}

	Number target = parser.get_number();

	char *coefficient_specification = parser.get_token();

	vector<Number> constraint_coefficients;

	if(strcmp(coefficient_specification, "OBJ") == 0) {
		constraint_coefficients = objective_coefficients;
	}
	else {
		unsigned long number_coefficients = parser.parse_unsigned_long(coefficient_specification);

		constraint_coefficients.resize(number_variables);
		read_coefficients_with_size(parser, constraint_coefficients, number_coefficients);
	}

	return Constraint(name, constraint_coefficients, direction, target);
}

inline void read_index_number_pairs_with_size(Parser &parser, vector<unsigned long> &indexes, vector<Number> &numbers, unsigned long size) {
	for(unsigned long i = 0; i < size; i++) {
		indexes.emplace_back(parser.get_unsigned_long());
		numbers.emplace_back(parser.get_number());
	}
}

inline void read_index_number_pairs(Parser &parser, vector<unsigned long> &indexes, vector<Number> &numbers) {
	unsigned long size = parser.get_unsigned_long();

	read_index_number_pairs_with_size(parser, indexes, numbers, size);
}

inline Reason read_reason(Parser &parser) {
	vector<unsigned long> constraint_indexes;
	vector<Number> constraint_multipliers;

	char *token;
	
	token = parser.get_token();

	if(strcmp(token, "{") != 0) {
		throw runtime_error(format("Expected open bracket in line {}\n", parser.get_line_number()));
	}

	char *type_string = parser.get_token();

	ReasonType type;

	if(strcmp(type_string, "asm") == 0) {
		type = ReasonType::TypeASM;
	}
	else if(strcmp(type_string, "lin") == 0) {
		type = ReasonType::TypeLIN;
		read_index_number_pairs(parser, constraint_indexes, constraint_multipliers);
	}
	else if(strcmp(type_string, "rnd") == 0) {
		type = ReasonType::TypeRND;
		read_index_number_pairs(parser, constraint_indexes, constraint_multipliers);
	}
	else if(strcmp(type_string, "uns") == 0) {
		type = ReasonType::TypeUNS;

		// Extracts only four indices
		for(unsigned long i = 0; i < 4; i++) {
			constraint_indexes.emplace_back(parser.get_unsigned_long());
		}
	}
	else if(strcmp(type_string, "sol") == 0) {
		type = ReasonType::TypeSOL;
	}
	else {
		throw runtime_error(format("Unexpected derivation name in line {}\n", parser.get_line_number()));
	}

	token = parser.get_token();

	if(strcmp(token, "}") != 0) {
		throw runtime_error(format("Expected close bracket in line {}\n", parser.get_line_number()));
	}

	return Reason(type, constraint_indexes, constraint_multipliers);
}

int main(int argc, char **argv) {
	// Checks if the VIPR certificate file was provided

	if(argc < 2) {
		fprintf(stderr, "usage: %s <vipr_certificate_file>\n", argv[0]);

		return EXIT_FAILURE;
	}

	// Creates the parser object that will return lines and tokens

	Parser parser(argv[1]);

	Certificate certificate;

	// Main parsing loop

	char *line;
	char *token;

    while((line = parser.get_line()) != nullptr) {
		token = parser.get_token();

		// Comment lines: Ignore
		if(strcmp(token, "%") == 0) {
			continue;
		}

		// VAR: Get number of variables (unsigned long) followed by many variable names
		//      separated by space or new lines
		if(strcmp(token, "VAR") == 0) {
			certificate.number_variables = parser.get_unsigned_long();

			certificate.variable_names.reserve(certificate.number_variables);
			certificate.variable_integral_flags.resize(certificate.number_variables);

			for(unsigned long i = 0; i < certificate.number_variables; i++) {
				token = parser.get_token();

				certificate.variable_names.emplace_back(parser.get_stable_string(token));
			}
		}

		// INT: Get the number of integral variables (unsigned long) followed by many variable indexes
		//      separated by space or new lines
		if(strcmp(token, "INT") == 0) {
			certificate.number_integral_variables = parser.get_unsigned_long();

			for(unsigned long i = 0; i < certificate.number_integral_variables; i++) {
				unsigned long index = parser.get_unsigned_long();

				certificate.variable_integral_flags[index] = true;
			}
		}

		// OBJ: Followed by "min" or "max" and a sequence of pairs of unsigned long (for indexes)
		//      and Number (for coefficients) separated by space or new lines
		if(strcmp(token, "OBJ") == 0) {
			char *min_or_max = parser.get_token();

			if(strcmp(min_or_max, "min") == 0) {
				certificate.minimization = true;
			}
			else if(strcmp(min_or_max, "max") == 0) {
				certificate.minimization = false;
			}
			else {
				fprintf(stderr, "Error in line %lu\n: expected 'min' or 'max'\n", parser.get_line_number());

				return EXIT_FAILURE;
			}

			certificate.objective_coefficients.resize(certificate.number_variables);
			read_coefficients(parser, certificate.objective_coefficients);
		}

		if(strcmp(token, "CON") == 0) {
			certificate.number_problem_constraints = parser.get_unsigned_long();

			// Not used
			unsigned long bound_constraints = parser.get_unsigned_long();

			for(unsigned long i = 0; i < certificate.number_problem_constraints; i++) {
				certificate.constraints.emplace_back(read_constraint(parser, certificate.number_variables, certificate.objective_coefficients));
			}
		}

		if(strcmp(token, "RTP") == 0) {
			char *token = parser.get_token();

			// Infeasible
			if(strcmp(token, "infeas")) {
				certificate.feasible = false;
			}
			// Feasible + range
			else if(strcmp(token, "range")) {
				certificate.feasible = true;

				certificate.feasible_lower_bound = parser.get_number_or_infinity();
				certificate.feasible_upper_bound = parser.get_number_or_infinity();
			}
			else {
				fprintf(stderr, "Expected valid bound in line %lu\n", parser.get_line_number());
				
				return EXIT_FAILURE;
			}
		}

		if(strcmp(token, "SOL") == 0) {
			certificate.number_solutions = parser.get_unsigned_long();

			for(unsigned long i = 0; i < certificate.number_solutions; i++) {
				char *name = parser.get_stable_string(parser.get_token());
				vector<Number> solution_coefficients;

				solution_coefficients.resize(certificate.number_variables);
				read_coefficients(parser, solution_coefficients);

				certificate.solutions.emplace_back(Solution(name, solution_coefficients));
			}

			if(certificate.solutions.size() != certificate.number_solutions) {
				fprintf(stderr, "Number of solutions is incorrect after reading the SOL section\n");

				return EXIT_FAILURE;
			}
		}

		if(strcmp(token, "DER") == 0) {
			certificate.number_derived_constraints = parser.get_unsigned_long();

			for(unsigned long i = 0; i < certificate.number_derived_constraints; i++) {
				Constraint constraint = read_constraint(parser, certificate.number_variables, certificate.objective_coefficients);
				Reason reason = read_reason(parser);
				long index = parser.get_long();

				certificate.constraints.emplace_back(constraint);

				certificate.derivations.emplace_back(Derivation(i + certificate.number_problem_constraints, reason, index));
			}

			if(certificate.constraints.size() != certificate.number_problem_constraints + certificate.number_derived_constraints) {
				fprintf(stderr, "Number of problem + derived constraints is incorrect after reading the DER section\n");

				return EXIT_FAILURE;
			}
		}
    }

	certificate.precompute();
	certificate.print_formula();

	return EXIT_SUCCESS;
}
