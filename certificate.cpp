#include "certificate.h"

#include <stdexcept>
#include <format>

#include <fstream>
#include <iostream>

using std::string;

using std::runtime_error;
using std::format;

using std::ofstream;

//////////////////////////
// Operator definitions //
//////////////////////////

#define LAMBDA(CODE) [&]() -> void { CODE; }
#define LITERAL(VAR) (LAMBDA(write_output(#VAR)))

constexpr int OP_ASSERT = 0;
constexpr int OP_NOT = 1;
constexpr int OP_AND = 2;
constexpr int OP_OR = 3;
constexpr int OP_EQ = 4;
constexpr int OP_NEQ = 5;
constexpr int OP_PLUS = 6;
constexpr int OP_MINUS = 7;
constexpr int OP_TIMES = 8;
constexpr int OP_DIVIDE = 9;
constexpr int OP_LEQ = 10;
constexpr int OP_GEQ = 11;
constexpr int OP_L = 12;
constexpr int OP_G = 13;
constexpr int OP_INTEGRAL = 14;
constexpr int OP_RND_DOWN = 15;
constexpr int OP_ITE = 16;
constexpr int OP_IMPLICATION = 17;

constexpr const char *OP_STRINGS[] = {
	"assert",
	"not",
	"and",
	"or",
	"=",
	"distinct",
	"+",
	"-",
	"*",
	"/",
	"<=",
	">=",
	"<",
	">",
	"is_int",
	"to_int",
	"ite",
	"=>"
};

//////////////////////////
// Precomputation tasks //
//////////////////////////

void Certificate::precompute() {
	number_total_constraints = number_problem_constraints + number_derived_constraints;

	for(unsigned long i = 0; i < number_variables; i++) {
		if(variable_integral_flags[i] == true) {
			variable_integral_vector.push_back(i);
		}
		else {
			variable_non_integral_vector.push_back(i);
		}
	}

	calculate_dependencies();
}

void Certificate::calculate_dependencies() {
	dependencies.resize(number_total_constraints);

	for(unsigned long i = number_problem_constraints; i < number_total_constraints; i++) {
		dependencies[i] = new unordered_set<unsigned long>();

		switch(get_derivation_from_offset(i).reason.type) {
			case ReasonType::TypeASM:
				dependencies[i]->insert(i);
				break;
			case ReasonType::TypeLIN:
			case ReasonType::TypeRND:
				for(unsigned long dependency_index: get_derivation_from_offset(i).reason.constraint_indexes) {
					// If it is one of the problem constraints, there are no assumptions
					if(dependency_index < number_problem_constraints) {
						continue;
					}

	 				// If the dependency has index bigger than or equal to the current one
					if(dependency_index >= i) {
						throw runtime_error(format("Constraint {} has dependency {} with index bigger than or equal to itself\n", i, dependency_index));
					}

					auto &other_dependency = dependencies[dependency_index];

					dependencies[i]->insert(other_dependency->begin(), other_dependency->end());
				}
				break;
			case ReasonType::TypeUNS:
				for(unsigned long dependency_index: get_derivation_from_offset(i).reason.constraint_indexes) {
	 				// If the dependency has index bigger than or equal to the current one
					if(dependency_index >= i) {
						throw runtime_error(format("Constraint {} has dependency {} with index bigger than or equal to itself\n", i, dependency_index));
					}
				}

				unsigned long dependency_index1 = get_derivation_from_offset(i).reason.get_i1();

				if(dependency_index1 >= number_problem_constraints) {
					auto &other_dependency1 = dependencies[dependency_index1];
					unsigned long exclusion1 = get_derivation_from_offset(i).reason.get_l1();

					dependencies[i]->insert(other_dependency1->begin(), other_dependency1->end());
					dependencies[i]->erase(exclusion1);
				}

				unsigned long dependency_index2 = get_derivation_from_offset(i).reason.get_i2();

				if(dependency_index2 >= number_problem_constraints) {
					auto &other_dependency2 = dependencies[dependency_index2];
					unsigned long exclusion2 = get_derivation_from_offset(i).reason.get_l2();

					// Only exclude if exclusion2 was not added twice

					bool exclude = !(dependencies[i]->contains(exclusion2));

					dependencies[i]->insert(other_dependency2->begin(), other_dependency2->end());

					if(exclude) {
						dependencies[i]->erase(exclusion2);
					}
				}

				break;
			// case ReasonType::TypeSOL:
				// No action
				// break;
		}
	}
}

//////////////////////////////
// Basic printing functions //
//////////////////////////////

// Space for the 64-bit decimal digits
constexpr size_t BUFFER_LONG_STRING_SIZE = 32;

thread_local ofstream *output_stream = nullptr;

inline void write_output(const char *message) {
	(*output_stream) << message;
}

inline void print_bool(bool variable) {
	write_output(variable ? "true" : "false");
}

inline void print_unsigned_long(unsigned long variable) {
	char buffer[BUFFER_LONG_STRING_SIZE];

	snprintf(buffer, BUFFER_LONG_STRING_SIZE, "%lu", variable);
	write_output(buffer);
}

inline void print_integral_string(char *number) {
	if(number[0] == '-') {
		write_output("(- ");
		write_output(number + 1);
		write_output(")");
	}
	else {
		write_output(number);
	}
}

inline void print_number(Number &number) {
	if(number.is_integral) {
		print_integral_string(number.numerator);
	}
	else {
		write_output("(/ ");
		print_integral_string(number.numerator);
		write_output(" ");
		print_integral_string(number.denominator);
		write_output(")");
	}
}

////////////////////////
// Generate functions //
////////////////////////

inline void generate(bool variable) {
	print_bool(variable);
}

inline void generate(unsigned long variable) {
	print_unsigned_long(variable);
}

template<typename F>
inline void generate(F &&function) {
	function();
}

template<>
inline void generate(bool &variable) {
	print_bool(variable);
}

template<>
inline void generate(unsigned long &variable) {
	print_unsigned_long(variable);
}

template<>
inline void generate(Number &variable) {
	print_number(variable);
}

template<>
inline void generate(const char *&literal) {
	write_output(literal);
}

template<typename T, typename U, typename W>
inline void ensure_minimum(T count, U minimum, W &&function) {
	for(auto i = count + 1; i <= minimum; i++) {
		function();
		write_output(" ");
	}
}

// For handling minimum arguments
#define MIN_SET(val) unsigned long __count = 0; unsigned long __minimum = val;
#define MIN_COUNT __count++;
#define MIN_ENSURE(CODE) ensure_minimum(__count, __minimum, CODE);

#define MIN_ENSURE_ZERO MIN_ENSURE(LAMBDA(print_integral_string("0")))

#define MIN_ENSURE_TRUE MIN_ENSURE(LAMBDA(print_bool(true)));
#define MIN_ENSURE_FALSE MIN_ENSURE(LAMBDA(print_bool(false)));

//////////////////////////////////////////////////
// Generating operators and logical constraints //
//////////////////////////////////////////////////

template<int OP_INDEX, typename T>
inline void print_op1(T &&variable) {
	write_output("(");
	write_output(OP_STRINGS[OP_INDEX]); // TODO: Construct at compile time?
	write_output(" ");
	generate<T>(std::forward<T>(variable));
	write_output(")");
}

template<int OP_INDEX, typename T, typename U>
inline void print_op2(T &&variable1, U &&variable2) {
	write_output("(");
	write_output(OP_STRINGS[OP_INDEX]); // TODO: Construct at compile time?
	write_output(" ");
	generate<T>(std::forward<T>(variable1));
	write_output(" ");
	generate<U>(std::forward<U>(variable2));
	write_output(")");
}

template<typename T, typename U>
inline void print_direction_op2(Direction direction, T &&variable1, U &&variable2) {
	write_output("(");
	switch(direction) {
		case Direction::SmallerEqual:
			write_output(OP_STRINGS[OP_LEQ]);
			break;
		case Direction::Equal:
			write_output(OP_STRINGS[OP_EQ]);
			break;
		case Direction::GreaterEqual:
			write_output(OP_STRINGS[OP_GEQ]);
			break;
	}
	write_output(" ");
	generate<T>(std::forward<T>(variable1));
	write_output(" ");
	generate<U>(std::forward<U>(variable2));
	write_output(")");
}

template<typename T, typename U, typename W>
inline void print_ifelse(T &&test, U &&if_value, W &&else_value) {
	print_op1<OP_ITE>(
		[&test, &if_value, &else_value] {
			generate<T>(std::forward<T>(test));
			write_output(" ");
			generate<U>(std::forward<U>(if_value));
			write_output(" ");
			generate<W>(std::forward<W>(else_value));
		}
	);
}

template<typename T>
inline void print_ceil(T &&parameter) {
	print_op1<OP_MINUS>(
		[&parameter] {
			print_op1<OP_RND_DOWN>(
				[&parameter] { print_op1<OP_MINUS>(parameter); }
			);
		}
	);
}

inline void print_s(Direction direction) {
	switch(direction) {
		case Direction::SmallerEqual:
			print_op1<OP_MINUS>(
				LITERAL(1)
			);
			break;
		case Direction::Equal:
			print_integral_string("0");
			break;
		case Direction::GreaterEqual:
			print_integral_string("1");
			break;
	}
}

///////////////////////////
// Print header & footer //
///////////////////////////

void print_header() {
    write_output("(set-info :smt-lib-version 2.6)\n");
    write_output("(set-logic AUFLIRA)\n");
    write_output("(set-info :source \"Transformed from a VIPR certificate\")\n");
    write_output("; --- END HEADER --- \n\n");
}

void print_footer() {
    write_output("(check-sat)\n");
}

/////////////////////////////
// Print model constraints //
/////////////////////////////

static Number zero("0");

bool Certificate::get_PUB() {
	return (feasible && !feasible_upper_bound.is_positive_infinity);
}

bool Certificate::get_PLB() {
	return (feasible && !feasible_lower_bound.is_negative_infinity);
}

Number &Certificate::get_U() {
	return (get_PUB() ? feasible_upper_bound : zero);
}

Number &Certificate::get_L() {
	return (get_PLB() ? feasible_lower_bound : zero);
}

void Certificate::print_pub() {
	print_op2<OP_AND>(
		feasible,
		LAMBDA(print_bool(!(feasible_upper_bound.is_positive_infinity)))
	);
}

void Certificate::print_plb() {
	print_op2<OP_AND>(
		feasible,
		LAMBDA(print_bool(!(feasible_lower_bound.is_negative_infinity)))
	);
}

void Certificate::print_respect_bound(vector<Number> &coefficients, vector<Number> &assignments, Direction direction, Number &target) {
	print_direction_op2(
		direction,
		LAMBDA(
			print_op1<OP_PLUS>(LAMBDA(
#ifndef FULL_MODEL
				MIN_SET(2);
#endif /* !FULL_MODEL */

				for(unsigned long i = 0; i < number_variables; i++) {
#ifndef FULL_MODEL
					auto &coefficient = coefficients[i];
					auto &assignment = assignments[i];

					if(coefficient.is_zero() || assignment.is_zero()) {
						continue;
					}

					MIN_COUNT;
#endif /* !FULL_MODEL */

					print_op2<OP_TIMES>(
						LAMBDA(print_number(coefficient)),
						LAMBDA(print_number(assignment))
					);

					// Space between multiplicative terms on the left-hand side
					write_output(" ");
				}

#ifndef FULL_MODEL
				MIN_ENSURE_ZERO;
#endif /* !FULL_MODEL */
			));
		),
		LAMBDA(print_number(target))
	);
}

void Certificate::print_one_solution_within_bound(Direction direction, Number &bound) {
	print_op1<OP_OR>(LAMBDA(
		MIN_SET(2);

		for(Solution &solution: solutions) {
			print_respect_bound(objective_coefficients, solution.assignments, direction, bound);

			// Space between terms
			write_output(" ");
			MIN_COUNT;
		}

		MIN_ENSURE_FALSE;
	));
}

void Certificate::print_all_solutions_within_bound(Direction direction, Number &bound) {
	print_op1<OP_AND>(LAMBDA(
		MIN_SET(2);

		for(Solution &solution: solutions) {
			print_respect_bound(objective_coefficients, solution.assignments, direction, bound);

			// Space between terms
			write_output(" ");
			MIN_COUNT;
		}

		MIN_ENSURE_TRUE;
	));
}

void Certificate::print_feas_individual(Solution &solution) {
	// TODO: Check: Note that I check for solution integrals only once. Can we reflect in the model?
	print_op1<OP_AND>(LAMBDA(
		MIN_SET(2);

		for(unsigned long i: variable_integral_vector) {
			print_bool(solution.assignments[i].is_integral);

			// Space between terms
			write_output(" ");
			MIN_COUNT;
		}

		for(int constraint_index = 0; constraint_index < number_problem_constraints; constraint_index++) {
			Constraint &constraint = constraints[constraint_index];

			print_op2<OP_IMPLICATION>(
				LAMBDA(print_op2<OP_GEQ>(
					LAMBDA(print_s(constraint.direction)),
					LITERAL(0)
				)),
				LAMBDA(
					print_respect_bound(constraint.coefficients, solution.assignments, Direction::GreaterEqual, constraint.target);
				)
			);
			MIN_COUNT;

			print_op2<OP_IMPLICATION>(
				LAMBDA(print_op2<OP_LEQ>(
					LAMBDA(print_s(constraint.direction)),
					LITERAL(0)
				)),
				LAMBDA(
					print_respect_bound(constraint.coefficients, solution.assignments, Direction::SmallerEqual, constraint.target);
				)
			);
			MIN_COUNT;

			// Space between terms
			write_output(" ");
		}

		MIN_ENSURE_TRUE;
	));
}

void Certificate::print_feas() {
	print_op1<OP_AND>(LAMBDA(
		MIN_SET(2);
		
		for(Solution &solution: solutions) {
			print_feas_individual(solution);

			// Space between terms
			write_output(" ");
			MIN_COUNT;
		}

		MIN_ENSURE_TRUE;
	));
}

void Certificate::print_pubimplication() {
	print_op2<OP_IMPLICATION>(
		LAMBDA(print_pub()),
		LAMBDA(print_one_solution_within_bound(Direction::SmallerEqual, get_U()))
	);
}

void Certificate::print_plbimplication() {
	print_op2<OP_IMPLICATION>(
		LAMBDA(print_plb()),
		LAMBDA(print_one_solution_within_bound(Direction::GreaterEqual, get_L()))
	);
}

void Certificate::print_sol() {
	auto task_print_sol = [&, this] {
		write_output("; Begin SOL\n");

		print_op1<OP_ASSERT>(LAMBDA(
			print_ifelse(
				LAMBDA(print_op1<OP_NOT>(feasible)),
				LAMBDA(print_op2<OP_EQ>(number_solutions, LITERAL(0))),
				LAMBDA(print_op2<OP_AND>(
					LAMBDA(print_feas()),
					LAMBDA(print_ifelse(
						minimization,
						LAMBDA(print_pubimplication()),
						LAMBDA(print_plbimplication())
					))
				))
			)
		));
	};

#ifdef PARALLEL
	threads.emplace_back([&, this] {
		// Open the file for SOL and print header
		string section_output_filename = output_filename + ".SOL";

		output_stream = new ofstream(section_output_filename);
		print_header();

		task_print_sol();

		// Print footer and close the file for block number
		print_footer();
		output_stream->close();

		// Dispatches the work to the execution manager
		remote_execution_manager.dispatch(section_output_filename, 0);
	});
#else
	task_print_sol();
#endif /* PARALLEL */
}

bool Certificate::calculate_Aij(unsigned long i, unsigned long j) {
	if(dependencies[i] == nullptr) {
		return false;
	}

	return (dependencies[i]->contains(j));
}

void Certificate::print_ASM(unsigned long k, Derivation &derivation) {
	for(unsigned long j = k + 1; j < number_total_constraints; j++) {
		if(get_derivation_from_offset(j).reason.type == ReasonType::TypeASM) {
			print_op1<OP_NOT>(LAMBDA(print_bool(calculate_Aij(k, j))));
		}
	}

	switch(derivation.reason.type) {
		case ReasonType::TypeASM:
			print_op2<OP_AND>(
				LAMBDA(print_bool(calculate_Aij(k, k))),
				LAMBDA(print_op1<OP_AND>(
					LAMBDA(
						MIN_SET(2);

						for(unsigned long j = number_problem_constraints; j < k; j++) {
							if(get_derivation_from_offset(j).reason.type == ReasonType::TypeASM) {
								print_op1<OP_NOT>(LAMBDA(print_bool(calculate_Aij(k, j))));
								MIN_COUNT;
							}
						}

						MIN_ENSURE_TRUE;
					)
				))
			);
			break;
		case ReasonType::TypeLIN:
		case ReasonType::TypeRND:
			print_op1<OP_AND>(LAMBDA(
				MIN_SET(2);

				for(unsigned long j = number_problem_constraints; j < k; j++) {
					if(get_derivation_from_offset(j).reason.type == ReasonType::TypeASM) {
						print_op2<OP_EQ>(
							LAMBDA(print_bool(calculate_Aij(k, j))),
							LAMBDA(print_op1<OP_OR>(
								LAMBDA(
									MIN_SET(2);

									for(unsigned long &i: derivation.reason.constraint_indexes) {
										if(j <= i && i < k) {
											print_bool(calculate_Aij(i, j));

											// Space between terms
											write_output(" ");
											MIN_COUNT;
										}
									}

									MIN_ENSURE_FALSE;
								)
							))
						);
						MIN_COUNT;
					}
				}

				MIN_ENSURE_TRUE;
			));
			break;
		case ReasonType::TypeUNS:
			print_op1<OP_AND>(LAMBDA(
				MIN_SET(2);

				for(unsigned long j = number_problem_constraints; j < k; j++) {
					if(get_derivation_from_offset(j).reason.type == ReasonType::TypeASM) {
						print_op2<OP_EQ>(
							LAMBDA(print_bool(calculate_Aij(k, j))),
							LAMBDA(print_op2<OP_OR>(
								LAMBDA(print_op2<OP_AND>(
									LAMBDA(print_bool(calculate_Aij(derivation.reason.get_i1(), j))),
									LAMBDA(print_op2<OP_NEQ>(j, derivation.reason.get_l1()))
								)),
								LAMBDA(print_op2<OP_AND>(
									LAMBDA(print_bool(calculate_Aij(derivation.reason.get_i2(), j))),
									LAMBDA(print_op2<OP_NEQ>(j, derivation.reason.get_l2()))
								))
							))
						);
						MIN_COUNT;
					}
				}

				MIN_ENSURE_TRUE;
			));
			break;
		case ReasonType::TypeSOL:
			print_op1<OP_AND>(LAMBDA(
				MIN_SET(2);

				for(unsigned long j = number_problem_constraints; j < k; j++) {
					if(get_derivation_from_offset(j).reason.type == ReasonType::TypeASM) {
						print_op1<OP_NOT>(
							LAMBDA(print_bool(calculate_Aij(k, j)))
						);
						MIN_COUNT;
					}
				}

				MIN_ENSURE_TRUE;
			));
			break;
	}
}

void Certificate::print_PRV(unsigned long k, Derivation &derivation) {
	print_op1<OP_AND>(LAMBDA(
		MIN_SET(2);

		for(unsigned long &j: derivation.reason.constraint_indexes) {
			print_op2<OP_L>(j, k);
			MIN_COUNT;
		}

		MIN_ENSURE_TRUE;
	));
}

template<typename PQ, typename PQP, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
void Certificate::print_DOM(PQ &&a, P0 &&b, P1 &&eq, P2 &&geq, P3 &&leq, PQP &&aP, P4 &&bP, P5 &&eqP, P6 &&geqP, P7 &&leqP) {
	print_op2<OP_OR>(
		LAMBDA(
			print_op2<OP_AND>(
				LAMBDA(
					for(unsigned long j = 0; j < number_variables; j++) {
						print_op2<OP_EQ>(LAMBDA(a(j)), LITERAL(0));
					}
				),
				LAMBDA(
					print_ifelse(
						eq,
						LAMBDA(
							print_op2<OP_NEQ>(b, LITERAL(0))
						),
						LAMBDA(print_ifelse(
							geq,
							LAMBDA(print_op2<OP_G>(b, LITERAL(0))),
							LAMBDA(print_ifelse(
								leq,
								LAMBDA(print_op2<OP_L>(b, LITERAL(0))),
								LAMBDA(print_bool(false))
							))
						))
					)
				)
			)
		),
		LAMBDA(
			print_op2<OP_AND>(
				LAMBDA(
					for(unsigned long j = 0; j < number_variables; j++) {
						print_op2<OP_EQ>(LAMBDA(a(j)), LAMBDA(aP(j)));
					}
				),
				LAMBDA(
					print_ifelse(
						eqP,
						LAMBDA(
							print_op2<OP_AND>(
								eq,
								LAMBDA(print_op2<OP_EQ>(b, bP))
							)
						),
						LAMBDA(print_ifelse(
							geqP,
							LAMBDA(
								print_op2<OP_AND>(
									geq,
									LAMBDA(print_op2<OP_GEQ>(b, bP))
								)
							),
							LAMBDA(print_ifelse(
								leqP,
								LAMBDA(
									print_op2<OP_AND>(
										leq,
										LAMBDA(print_op2<OP_LEQ>(b, bP))
									)
								),
								LAMBDA(print_bool(false))
							))
						))
					)
				)
			)
		)
	);
}

template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5>
void Certificate::print_DOM(P0 &&print_coefficientA, P1 &&print_directionA, P2 &&print_targetA, P3 &&print_coefficientB, P4 &&print_directionB, P5 &&print_targetB) {
	print_DOM(
		print_coefficientA,
		print_targetA,
		LAMBDA(print_op2<OP_EQ>(
			print_directionA,
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_GEQ>(
			print_directionA,
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_LEQ>(
			print_directionA,
			LITERAL(0)
		)),
		print_coefficientB,
		print_targetB,
		LAMBDA(print_op2<OP_EQ>(
			print_directionB,
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_GEQ>(
			print_directionB,
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_LEQ>(
			print_directionB,
			LITERAL(0)
		))
	);
}

void Certificate::print_DOM(Constraint &constraint1, Constraint &constraint2) {
	print_DOM(
		[&] (unsigned long j) {
			print_number(constraint1.coefficients[j]);
		},
		LAMBDA(print_number(constraint1.target)),
		LAMBDA(print_op2<OP_EQ>(
			LAMBDA(print_s(constraint1.direction)),
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_GEQ>(
			LAMBDA(print_s(constraint1.direction)),
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_LEQ>(
			LAMBDA(print_s(constraint1.direction)),
			LITERAL(0)
		)),
		[&] (unsigned long j) {
			print_number(constraint2.coefficients[j]);
		},
		LAMBDA(print_number(constraint2.target)),
		LAMBDA(print_op2<OP_EQ>(
			LAMBDA(print_s(constraint2.direction)),
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_GEQ>(
			LAMBDA(print_s(constraint2.direction)),
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_LEQ>(
			LAMBDA(print_s(constraint2.direction)),
			LITERAL(0)
		))
	);
}

template<typename P0, typename P1>
void Certificate::print_RND(const function<void(unsigned long)> &a, P0 &&b, P1 &&eq) {
	print_op1<OP_AND>(
		LAMBDA(
			MIN_SET(2);

			for(unsigned long j: variable_integral_vector) {
				print_op1<OP_INTEGRAL>(LAMBDA(a(j)));
				MIN_COUNT;
			}

			for(unsigned long j: variable_non_integral_vector) {
				print_op2<OP_EQ>(LAMBDA(a(j)), LITERAL(0));
				MIN_COUNT;
			}

			print_op1<OP_NOT>(eq);
			MIN_COUNT;

			MIN_ENSURE_TRUE;
		)
	);
}

void Certificate::print_DIS(Constraint &c_i, Constraint &c_j) {
	print_op1<OP_AND>(
		LAMBDA(
			// Not counting because the number of operations is always >= 2

			for(unsigned long k = 0; k < number_variables; k++) {
				print_op2<OP_EQ>(c_i.coefficients[k], c_j.coefficients[k]);
			}
			 
			for(unsigned long k: variable_integral_vector) {
				print_op1<OP_INTEGRAL>(c_i.coefficients[k]);
			}

			for(unsigned long k: variable_non_integral_vector) {
				print_op2<OP_EQ>(c_i.coefficients[k], LITERAL(0));
			}
			
			print_op1<OP_INTEGRAL>(c_i.target);
			print_op1<OP_INTEGRAL>(c_j.target);

			print_op2<OP_AND>(
				LAMBDA(print_op2<OP_NEQ>(
					LAMBDA(print_s(c_i.direction)),
					LITERAL(0)
				)),
				LAMBDA(print_op2<OP_EQ>(
					LAMBDA(print_op2<OP_PLUS>(
						LAMBDA(print_s(c_i.direction)),
						LAMBDA(print_s(c_j.direction))
					)),
					LITERAL(0)
				))
			);

			print_ifelse(
				LAMBDA(print_op2<OP_EQ>(
					LAMBDA(print_s(c_i.direction)),
					LITERAL(1)
				)),
				LAMBDA(print_op2<OP_EQ>(
					LAMBDA(print_number(c_i.target)),
					LAMBDA(print_op2<OP_PLUS>(
						LAMBDA(print_number(c_j.target)),
						LITERAL(1)
					))
				)),
				LAMBDA(print_op2<OP_EQ>(
					LAMBDA(print_number(c_i.target)),
					LAMBDA(print_op2<OP_MINUS>(
						LAMBDA(print_number(c_j.target)),
						LITERAL(1)
					))
				))
			);
		)
	);
}

void Certificate::print_asm_individual(unsigned long derivation_index, Derivation &derivation) {
	print_ASM(derivation_index, derivation);
}

void Certificate::print_LIN_RND_aj(unsigned long derivation_index, Derivation &derivation, unsigned long j) {
	vector<unsigned long> &data = derivation.reason.constraint_indexes;

	print_op1<OP_PLUS>(LAMBDA(
		MIN_SET(2);

		for(unsigned long data_position = 0; data_position < data.size(); data_position++) {
			// Using the same indexes as the definitions
			unsigned long i = derivation.reason.constraint_indexes[data_position];
			Number &data_i = derivation.reason.constraint_multipliers[data_position];

#ifndef FULL_MODEL
			if(data_i.is_zero() || constraints[i].coefficients[j].is_zero()) {
				continue;
			}
#endif /* FULL_MODEL */

			print_op2<OP_TIMES>(
				data_i,
				LAMBDA(print_number(constraints[i].coefficients[j]))
			);
			MIN_COUNT;
		}

		MIN_ENSURE_ZERO;
	));
}

void Certificate::print_LIN_RND_aPj(unsigned long derivation_index, Derivation &derivation, unsigned long j) {
	print_number(constraints[derivation_index].coefficients[j]);
}

void Certificate::print_LIN_RND_b(unsigned long derivation_index, Derivation &derivation) {
	vector<unsigned long> &data = derivation.reason.constraint_indexes;

	print_op1<OP_PLUS>(LAMBDA(
		MIN_SET(2);

		for(unsigned long data_position = 0; data_position < data.size(); data_position++) {
			// Using the same indexes as the definitions
			unsigned long i = derivation.reason.constraint_indexes[data_position];
			Number &data_i = derivation.reason.constraint_multipliers[data_position];

#ifndef FULL_MODEL
			if(data_i.is_zero() || constraints[i].target.is_zero()) {
				continue;
			}
#endif /* FULL_MODEL */

			print_op2<OP_TIMES>(
				data_i,
				LAMBDA(print_number(constraints[i].target))
			);
			MIN_COUNT;
		}

		MIN_ENSURE_ZERO;
	));
}

void Certificate::print_LIN_RND_bP(unsigned long derivation_index, Derivation &derivation) {
	print_number(constraints[derivation_index].target);
}

void Certificate::print_conjunction_eq_leq_geq(unsigned long derivation_index, Derivation &derivation, Direction direction) {
	vector<unsigned long> &data = derivation.reason.constraint_indexes;

	print_op1<OP_AND>(LAMBDA(
		MIN_SET(2);

		for(unsigned long data_position = 0; data_position < data.size(); data_position++) {
			// Using the same indexes as the definitions
			unsigned long i = derivation.reason.constraint_indexes[data_position];
			Number &data_i = derivation.reason.constraint_multipliers[data_position];

#ifndef FULL_MODEL
			if(data_i.is_zero() || constraints[i].direction == Direction::Equal) {
				continue;
			}
#endif /* FULL_MODEL */

			print_direction_op2(
				direction,
				LAMBDA(
					print_op2<OP_TIMES>(
						data_i,
						LAMBDA(print_s(constraints[i].direction))
					)
				),
				LITERAL(0)
			);
			MIN_COUNT;
		}

		MIN_ENSURE_TRUE;
	));
}

void Certificate::print_eq(unsigned long derivation_index, Derivation &derivation) {
	print_conjunction_eq_leq_geq(derivation_index, derivation, Direction::Equal);
}

void Certificate::print_geq(unsigned long derivation_index, Derivation &derivation) {
	print_conjunction_eq_leq_geq(derivation_index, derivation, Direction::GreaterEqual);
}

void Certificate::print_leq(unsigned long derivation_index, Derivation &derivation) {
	print_conjunction_eq_leq_geq(derivation_index, derivation, Direction::SmallerEqual);
}

void Certificate::print_lin_individual(unsigned long derivation_index, Derivation &derivation) {
	print_op1<OP_AND>(LAMBDA(
		// Not counting because the number of operations is always >= 2

		print_ASM(derivation_index, derivation);
		print_PRV(derivation_index, derivation);

		print_DOM(
			[&] (unsigned long j) {
				print_LIN_RND_aj(derivation_index, derivation, j);
			},
			LAMBDA(print_LIN_RND_b(derivation_index, derivation)),
			LAMBDA(print_eq(derivation_index, derivation)),
			LAMBDA(print_geq(derivation_index, derivation)),
			LAMBDA(print_leq(derivation_index, derivation)),
			[&] (unsigned long j) {
				print_LIN_RND_aPj(derivation_index, derivation, j);
			},
			LAMBDA(print_LIN_RND_bP(derivation_index, derivation)),
			LAMBDA(print_op2<OP_EQ>(
				LAMBDA(print_s(constraints[derivation_index].direction)),
				LITERAL(0)
			)),
			LAMBDA(print_op2<OP_GEQ>(
				LAMBDA(print_s(constraints[derivation_index].direction)),
				LITERAL(0)
			)),
			LAMBDA(print_op2<OP_LEQ>(
				LAMBDA(print_s(constraints[derivation_index].direction)),
				LITERAL(0)
			))
		);
	));
}

template<typename PQ, typename PQP, typename P0, typename P1, typename P2, typename P3, typename P4>
void Certificate::print_rnd_individual_part2(PQ &&a, P0 &&b, P1 &&eq, P2 &&geq, P3 &&leq, PQP &&aP, P4 &&bP, unsigned long derivation_index) {
	print_op2<OP_OR>(
		LAMBDA(
			print_op2<OP_AND>(
				LAMBDA(
					for(unsigned long j = 0; j < number_variables; j++) {
						print_op2<OP_EQ>(LAMBDA(a(j)), LITERAL(0));
					}
				),
				LAMBDA(
					print_ifelse(
						geq,
						LAMBDA(print_op2<OP_G>(b, LITERAL(0))),
						LAMBDA(print_ifelse(
							leq,
							LAMBDA(print_op2<OP_L>(b, LITERAL(0))),
							LAMBDA(print_bool(false))
						))
					)
				)
			)
		),
		LAMBDA(
			print_op2<OP_AND>(
				LAMBDA(
					for(unsigned long j = 0; j < number_variables; j++) {
						print_op2<OP_EQ>(LAMBDA(a(j)), LAMBDA(aP(j)));
					}
				),
				LAMBDA(
					print_ifelse(
						LAMBDA(
							print_op2<OP_EQ>(
								LAMBDA(print_s(constraints[derivation_index].direction)),
								LITERAL(1)
							)
						),
						LAMBDA(
							print_op2<OP_AND>(
								geq,
								LAMBDA(print_op2<OP_GEQ>(
									LAMBDA(print_ceil(b)),
									bP
								))
							)
						),
						LAMBDA(
							print_op2<OP_AND>(
								leq,
								LAMBDA(print_op2<OP_LEQ>(
									LAMBDA(print_op1<OP_RND_DOWN>(b)),
									bP
								))
							)
						)
					)
				)
			)
		)
	);
}

void Certificate::print_rnd_individual(unsigned long derivation_index, Derivation &derivation) {
	print_op1<OP_AND>(LAMBDA(
		// Not counting because the number of operations is always >= 2

		print_ASM(derivation_index, derivation);
		print_PRV(derivation_index, derivation);

		print_RND(
			[&] (unsigned long j) {
				print_LIN_RND_aj(derivation_index, derivation, j);
			},
			LAMBDA(print_LIN_RND_b(derivation_index, derivation)),
			LAMBDA(print_eq(derivation_index, derivation))
		);

		print_op2<OP_NEQ>(
			LAMBDA(print_s(constraints[derivation_index].direction)),
			LITERAL(0)
		);

		print_rnd_individual_part2(
			[&] (unsigned long j) {
				print_LIN_RND_aj(derivation_index, derivation, j);
			},
			LAMBDA(print_LIN_RND_b(derivation_index, derivation)),
			LAMBDA(print_eq(derivation_index, derivation)),
			LAMBDA(print_geq(derivation_index, derivation)),
			LAMBDA(print_leq(derivation_index, derivation)),
			[&] (unsigned long j) {
				print_LIN_RND_aPj(derivation_index, derivation, j);
			},
			LAMBDA(print_LIN_RND_bP(derivation_index, derivation)),
			derivation_index
		);
	));
}

void Certificate::print_uns_individual(unsigned long derivation_index, Derivation &derivation) {
	print_op1<OP_AND>(LAMBDA(
		// Not counting because the number of operations is always >= 2

		print_ASM(derivation_index, derivation);
		write_output(" ");
		print_op2<OP_G>(derivation_index, derivation.reason.get_i1());
		write_output(" ");
		print_op2<OP_G>(derivation_index, derivation.reason.get_i2());
		write_output(" ");
		print_DOM(constraints[derivation.reason.get_i1()], derivation.get_constraint(constraints));
		write_output(" ");
		print_DOM(constraints[derivation.reason.get_i2()], derivation.get_constraint(constraints));
		write_output(" ");
		print_bool(calculate_Aij(derivation.reason.get_i1(), derivation.reason.get_l1()));
		write_output(" ");
		print_bool(calculate_Aij(derivation.reason.get_i2(), derivation.reason.get_l2()));
		write_output(" ");
		print_DIS(constraints[derivation.reason.get_l1()], constraints[derivation.reason.get_l2()]);
	));
}

void Certificate::print_sol_individual_dom(Solution &solution, Direction direction, Constraint &constraint2) {
	print_DOM(
		[&] (unsigned long j) {
			print_number(objective_coefficients[j]);
		},
		LAMBDA(
			print_op1<OP_PLUS>(LAMBDA(
#ifndef FULL_MODEL
				MIN_SET(2);
#endif /* !FULL_MODEL */

				for(unsigned long i = 0; i < number_variables; i++) {
#ifndef FULL_MODEL
					if(objective_coefficients[i].is_zero() || solution.assignments[i].is_zero()) {
						continue;
					}

					MIN_COUNT;
#endif /* !FULL_MODEL */
					print_op2<OP_TIMES>(
						objective_coefficients[i],
						solution.assignments[i]
					);
				}

#ifndef FULL_MODEL
				MIN_ENSURE_ZERO;
#endif /* !FULL_MODEL */
			))
		),
		LAMBDA(print_op2<OP_EQ>(
			LAMBDA(print_s(direction)),
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_GEQ>(
			LAMBDA(print_s(direction)),
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_LEQ>(
			LAMBDA(print_s(direction)),
			LITERAL(0)
		)),
		[&] (unsigned long j) {
			print_number(constraint2.coefficients[j]);
		},
		LAMBDA(print_number(constraint2.target)),
		LAMBDA(print_op2<OP_EQ>(
			LAMBDA(print_s(constraint2.direction)),
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_GEQ>(
			LAMBDA(print_s(constraint2.direction)),
			LITERAL(0)
		)),
		LAMBDA(print_op2<OP_LEQ>(
			LAMBDA(print_s(constraint2.direction)),
			LITERAL(0)
		))
	);
}

void Certificate::print_sol_individual(unsigned long derivation_index, Derivation &derivation) {
	print_op2<OP_AND>(
		LAMBDA(
			print_ASM(derivation_index, derivation);
		),
		LAMBDA(
			print_ifelse(
				minimization,
				LAMBDA(
					print_op1<OP_OR>(LAMBDA(
						MIN_SET(2);

						for(Solution &solution: solutions) {
							print_sol_individual_dom(
								solution,
								Direction::SmallerEqual,
								derivation.get_constraint(constraints)
							);
							MIN_COUNT;
						}

						MIN_ENSURE_FALSE;
					));
				),
				LAMBDA(
					print_op1<OP_OR>(LAMBDA(
						MIN_SET(2);

						for(Solution &solution: solutions) {
							print_sol_individual_dom(
								solution,
								Direction::GreaterEqual,
								derivation.get_constraint(constraints)
							);
							MIN_COUNT;
						}

						MIN_ENSURE_FALSE;
					));
				)
			);
		)
	);
}

void Certificate::print_der_individual(unsigned long derivation_index, Derivation &derivation) {
	switch(derivation.reason.type) {
		case ReasonType::TypeASM:
			print_asm_individual(derivation_index, derivation);
			break;
		case ReasonType::TypeLIN:
			print_lin_individual(derivation_index, derivation);
			break;
		case ReasonType::TypeRND:
			print_rnd_individual(derivation_index, derivation);
			break;
		case ReasonType::TypeUNS:
			print_uns_individual(derivation_index, derivation);
			break;
		case ReasonType::TypeSOL:
			print_sol_individual(derivation_index, derivation);
			break;
	}
}

void Certificate::print_der() {
	auto task_der_part1 = [&, this] (unsigned long j) {
		Derivation &derivation = get_derivation_from_offset(j);

		write_output("; DER for constraint ");
		write_output(derivation.get_constraint(constraints).name);
		write_output("\n");

		print_op1<OP_ASSERT>(LAMBDA(
			print_op1<OP_AND>(LAMBDA(
				print_der_individual(j, derivation);
			));
		));

		// Lines between assertions
		write_output("\n");
	};

#ifdef PARALLEL
	unsigned long number_blocks = std::ceil(static_cast<float>(number_derived_constraints) / block_size);

	unsigned long total_cores = std::min(static_cast<unsigned long>(std::thread::hardware_concurrency()), number_blocks);

	fprintf(stderr, "Running DER generation with %lu parallel cores and block size %lu\n", total_cores, block_size);

	for(unsigned long core = 0; core < total_cores; core++) {
		threads.emplace_back([&, this] (unsigned long core) {
			for(unsigned long derived_index = (core * block_size); derived_index < number_derived_constraints; derived_index += (total_cores * block_size)) {
				// Calculate global indexes
				unsigned long global_index_start = derived_index + number_problem_constraints;
				unsigned long global_index_finish = std::min(global_index_start + block_size, number_total_constraints) - 1;

				// Open the file for block number and print header
				string section_output_filename = output_filename + ".DER-" + std::to_string(global_index_start - number_problem_constraints + 1) + "-" + std::to_string(global_index_finish - number_problem_constraints + 1);
				
				output_stream = new ofstream(section_output_filename);
				print_header();

				for(unsigned long j = global_index_start; j <= global_index_finish; j++) {
					task_der_part1(j);
				}

				// Print footer and close the file for block number
				print_footer();
				output_stream->close();

				// Dispatches the work to the execution manager
				remote_execution_manager.dispatch(section_output_filename, 0);
			}
		},
		core);
	}
#else
	for(unsigned long i = number_problem_constraints; i < number_total_constraints; i++) {
		task_der_part1(i);
	}
#endif /* PARALLEL */

	auto task_der_part2 = [&, this] {
		write_output("; Begin DER (solution check)\n");

		print_op1<OP_ASSERT>(LAMBDA(
			unsigned long last_constraint_index = number_total_constraints - 1;
			Constraint &last_constraint = constraints[last_constraint_index];

			print_ifelse(
				LAMBDA(print_op1<OP_NOT>(feasible)),
				LAMBDA(print_op2<OP_AND>(
					LAMBDA(print_DOM(
						[&] (unsigned long j) {
							print_number(last_constraint.coefficients[j]);
						},
						LAMBDA(print_s(last_constraint.direction)),
						LAMBDA(print_number(last_constraint.target)),
						[&] (unsigned long j) {
							print_integral_string("0");
						},
						LAMBDA(print_s(Direction::GreaterEqual)),
						LAMBDA(print_integral_string("1"))
					)),
					LAMBDA(
						for(unsigned long j = number_problem_constraints; j < number_total_constraints; j++) {
							if(get_derivation_from_offset(j).reason.type == ReasonType::TypeASM) {
								print_op1<OP_NOT>(LAMBDA(print_bool(calculate_Aij(last_constraint_index, j))));
							}
						}
					)
				)),
				LAMBDA(print_op2<OP_AND>(
					LAMBDA(print_op2<OP_IMPLICATION>(
						LAMBDA(print_op2<OP_AND>(
							minimization,
							LAMBDA(print_plb())
						)),
						LAMBDA(print_op2<OP_AND>(
							LAMBDA(print_DOM(
								[&] (unsigned long j) {
									print_number(last_constraint.coefficients[j]);
								},
								LAMBDA(print_s(last_constraint.direction)),
								LAMBDA(print_number(last_constraint.target)),
								[&] (unsigned long j) {
									print_number(objective_coefficients[j]);
								},
								LAMBDA(print_s(Direction::GreaterEqual)),
								LAMBDA(print_number(get_L()))
							)),
							LAMBDA(
								for(unsigned long j = number_problem_constraints; j < number_total_constraints; j++) {
									if(get_derivation_from_offset(j).reason.type == ReasonType::TypeASM) {
										print_op1<OP_NOT>(LAMBDA(print_bool(calculate_Aij(last_constraint_index, j))));
									}
								}
							)
						))
					)),
					LAMBDA(print_op2<OP_IMPLICATION>(
						LAMBDA(print_op2<OP_AND>(
							LAMBDA(print_op1<OP_NOT>(
								minimization
							)),
							LAMBDA(print_pub())
						)),
						LAMBDA(print_op2<OP_AND>(
							LAMBDA(print_DOM(
								[&] (unsigned long j) {
									print_number(last_constraint.coefficients[j]);
								},
								LAMBDA(print_s(last_constraint.direction)),
								LAMBDA(print_number(last_constraint.target)),
								[&] (unsigned long j) {
									print_number(objective_coefficients[j]);
								},
								LAMBDA(print_s(Direction::SmallerEqual)),
								LAMBDA(print_number(get_U()))
							)),
							LAMBDA(
								for(unsigned long j = number_problem_constraints; j < number_total_constraints; j++) {
									if(get_derivation_from_offset(j).reason.type == ReasonType::TypeASM) {
										print_op1<OP_NOT>(LAMBDA(print_bool(calculate_Aij(last_constraint_index, j))));
									}
								}
							)
						))
					))
				))
			)
		));
	};

#ifdef PARALLEL
	threads.emplace_back([&, this] {
		// Open the file for SOL and print header
		string section_output_filename = output_filename + ".DER-solcheck";
		
		output_stream = new ofstream(section_output_filename);
		print_header();

		task_der_part2();

		// Print footer and close the file for block number
		print_footer();
		output_stream->close();

		// Dispatches the work to the execution manager
		remote_execution_manager.dispatch(section_output_filename, 0);
	});
#else
	task_der_part2();
#endif /* PARALLEL */
}

void Certificate::setup_output(string output_filename, bool expected_sat, unsigned long block_size) {
	this->output_filename = output_filename;
	this->expected_sat = expected_sat;
	this->block_size = block_size;
}

void Certificate::print_formula() {
#ifndef PARALLEL
	// Open the single output file and print header
	output_stream = new ofstream(output_filename);
	print_header();	
#endif /* !PARALLEL */

	print_sol();
	print_der();

#ifndef PARALLEL
	// Print footer and close the single output file
	print_footer();	
	output_stream->close();
#endif /* !PARALLEL */

#ifdef PARALLEL
	for(auto &thread : threads) {
		thread.join();
	}
#else
	remote_execution_manager.dispatch(output_filename, 0);
#endif /* PARALLEL */
}

////////////////////////////////////
// Print in human-readable format //
////////////////////////////////////

string get_string_numbers(vector<Number> &coefficients) {
	string result;
	bool first = true;

	for(int i = 0; i < coefficients.size(); i++) {
		if(strcmp(coefficients[i].numerator, "0") == 0) {
			continue;
		}

		if(first) {
			first = false;
		}
		else {
			result += " + ";
		}

		result += "(";
		result += coefficients[i].get_string();
		result += " x_";
		result += std::to_string(i);
		result += ")";
	}

	return result;
}

void Certificate::print() {
	for(auto i = 0; i < number_variables; i++) {
		fprintf(stdout, "%s: %s\n", variable_names[i], (variable_integral_flags[i] ? "Integral" : "Fraction"));
	}

	fprintf(stdout, "Objective coefficients: \n");
	get_string_numbers(objective_coefficients);

	fprintf(stdout, "Constraints: \n");
	for(auto &constraint: constraints) {
		fprintf(stdout, "%s\n", constraint.get_string().c_str());
	}

	fprintf(stdout, "Solutions: \n");
	for(auto &solution: solutions) {
		fprintf(stdout, "%s\n", solution.get_string().c_str());
	}

	fprintf(stdout, "Derivations: \n");
	for(auto &derivation: derivations) {
		fprintf(stdout, "%s\n", derivation.get_string(constraints).c_str());
	}
}

Certificate::Certificate() {
}

Certificate::~Certificate() {
}

bool Certificate::get_evaluation_result() {
	// Clears all pending dispatches and leaves the program if one of them fails
	RemoteExecutionManager::ClearingResult result;

	while((result = remote_execution_manager.clear_dispatches()) != RemoteExecutionManager::ClearingResult::Done) {
		if(result == RemoteExecutionManager::ClearingResult::Unsat && expected_sat == true) {
			// Expected sat, did not get sat in all dispatches
			return false;
		}
		
		if(result == RemoteExecutionManager::ClearingResult::Unsat && expected_sat == false) {
			// Expected unsat, did not get sat in all dispatches
			return true;
		}
	}

	if(expected_sat) {
		// Expected sat, got sat in all dispatches
		return true;
	}
	else {
		// Expected unsat, got sat in all dispatches
		return false;
	}
}