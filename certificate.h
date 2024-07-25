#ifndef CERTIFICATE_H
#define CERTIFICATE_H

#include <cstdint>
#include <set>
#include <vector>
#include <unordered_set>

#include <functional>

#include "basic_types.h"

using std::set;
using std::vector;
using std::unordered_set;

using std::function;

// Used in Certificate::print()
string get_string_numbers(vector<Number> &coefficients);

enum Direction {
	SmallerEqual,
	Equal,
	GreaterEqual
};

struct Constraint {
	char *name;
	vector<Number> coefficients;
	Direction direction;
	Number target;

	Constraint(char *name, vector<Number> &coefficients, Direction direction, Number target):
		name{name}, coefficients{coefficients}, direction{direction}, target{target} {}
	
	string get_string() {
		string result = string(name) + ": ";

		result += get_string_numbers(coefficients);

		switch(direction) {
			case Direction::SmallerEqual:
				result += " <= ";
				break;
			case Direction::Equal:
				result += " = ";
				break;
			case Direction::GreaterEqual:
				result += " >= ";
				break;
		}

		result += target.get_string();

		return result;
	}
};

struct Solution {
	char *name;
	vector<Number> assignments;

	Solution(char *name, vector<Number> &assignments): name{name}, assignments{assignments} {}
	
	string get_string() {
		string result = string(name) + ": ";

		result += get_string_numbers(assignments);

		return result;
	}
};

enum ReasonType {
	TypeASM,
	TypeLIN,
	TypeRND,
	TypeUNS,
	TypeSOL
};

struct Reason {
	ReasonType type;

	vector<unsigned long> constraint_indexes;
	vector<Number> constraint_multipliers;

	Reason(ReasonType type, vector<unsigned long> &constraint_indexes, vector<Number> &constraint_multipliers):
		type{type}, constraint_indexes{constraint_indexes}, constraint_multipliers{constraint_multipliers} {}
	
	unsigned long &get_i1() {
		return constraint_indexes[0];
	}

	unsigned long &get_l1() {
		return constraint_indexes[1];
	}

	unsigned long &get_i2() {
		return constraint_indexes[2];
	}

	unsigned long &get_l2() {
		return constraint_indexes[3];
	}

	string get_string() {
		string result = "{ ";

		switch(type) {
			case ReasonType::TypeASM:
				result += "asm";
				break;
			case ReasonType::TypeLIN:
				result += "lin";
				break;
			case ReasonType::TypeRND:
				result += "rnd";
				break;
			case ReasonType::TypeUNS:
				result += "uns";
				break;
			case ReasonType::TypeSOL:
				result += "sol";
				break;
		}

		result += " } [ ";
		
		for(auto &index: constraint_indexes) {
			result += std::to_string(index);
			result += " ";
		}

		result += "] [ ";
		
		for(auto &multiplier: constraint_multipliers) {
			result += multiplier.get_string();
			result += " ";
		}

		result += "]";
		
		return result;
	}
};

struct Derivation {
	unsigned long constraint_index;
	Reason reason;
	long largest_index;

	Derivation(unsigned long constraint_index, Reason &reason, long largest_index):
		constraint_index{constraint_index}, reason{reason}, largest_index{largest_index} {}
	
	string get_string(vector<Constraint> &constraints) {
		string result = "Derivation ";

		result += get_constraint(constraints).get_string();
		result += " ";
		result += reason.get_string();
		result += " last_index ";
		result += std::to_string(largest_index);

		return result;
	}

	inline Constraint &get_constraint(vector<Constraint> &constraints) {
		return constraints[constraint_index];
	}
};

struct Certificate {
	bool feasible;
	Number feasible_lower_bound;
	Number feasible_upper_bound;

	bool minimization;
	
	unsigned long number_variables;
	unsigned long number_integral_variables;

	unsigned long number_problem_constraints;
	unsigned long number_derived_constraints;
	unsigned long number_total_constraints; // Precomputed based on parser's input

	unsigned long number_solutions;

	vector<char *> variable_names;
	vector<bool> variable_integral_flags;

	vector<unsigned long> variable_integral_vector; // Precomputed based on parser's input
	vector<unsigned long> variable_non_integral_vector; // Precomputed based on parser's input

	vector<Number> objective_coefficients;

	vector<Constraint> constraints;
	vector<Solution> solutions;

	vector<Derivation> derivations;

	vector<unordered_set<unsigned long> *> dependencies;

	void precompute();
	void print_formula();

	void print();

private:
	bool get_PUB();
	bool get_PLB();
	Number &get_L();
	Number &get_U();

	void calculate_dependencies();

	void print_pub();
	void print_plb();

	// Begin SOL predicate
	void print_respect_bound(vector<Number> &coefficients, vector<Number> &assignments, Direction direction, Number &target);
	void print_one_solution_within_bound(Direction direction, Number &bound);
	void print_all_solutions_within_bound(Direction direction, Number &bound);

	void print_feas_individual(Solution &solution);
	void print_feas();

	void print_pubimplication();
	void print_plbimplication();

	void print_sol();
	// End SOL predicate

	// Begin DER predicate
	bool calculate_Aij(unsigned long i, unsigned long j);

	void print_ASM(unsigned long k, Derivation &derivation);
	void print_PRV(unsigned long k, Derivation &derivation);

	template<typename PQ, typename PQP, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
		void print_DOM(PQ &&a, P0 &&b, P1 &&eq, P2 &&geq, P3 &&leq, PQP &&aP, P4 &&bP, P5 &&eqP, P6 &&geqP, P7 &&leqP);

	template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5>
		void print_DOM(P0 &&print_coefficientA, P1 &&print_directionA, P2 &&print_targetA, P3 &&print_coefficientB, P4 &&print_directionB, P5 &&print_targetB);

	void print_DOM(Constraint &constraint1, Constraint &constraint2);

	template<typename P0, typename P1>
		void print_RND(const function<void(unsigned long)> &a, P0 &&b, P1 &&eq);

	void print_DIS(Constraint &c_i, Constraint &c_j);

	// ASM
	void print_asm_individual(unsigned long derivation_index, Derivation &derivation);

	// Used in LIN & RND
	void print_LIN_RND_aj(unsigned long derivation_index, Derivation &derivation, unsigned long j);
	void print_LIN_RND_b(unsigned long derivation_index, Derivation &derivation);
	void print_LIN_RND_aPj(unsigned long derivation_index, Derivation &derivation, unsigned long j);
	void print_LIN_RND_bP(unsigned long derivation_index, Derivation &derivation);

	// LIN
	void print_conjunction_eq_leq_geq(unsigned long derivation_index, Derivation &derivation, Direction direction);
	void print_eq(unsigned long derivation_index, Derivation &derivation);
	void print_leq(unsigned long derivation_index, Derivation &derivation);
	void print_geq(unsigned long derivation_index, Derivation &derivation);
	void print_lin_individual(unsigned long derivation_index, Derivation &derivation);

	// RND
	template<typename PQ, typename PQP, typename P0, typename P1, typename P2, typename P3, typename P4>
		void print_rnd_individual_part2(PQ &&a, P0 &&b, P1 &&eq, P2 &&geq, P3 &&leq, PQP &&aP, P4 &&bP, unsigned long derivation_index);
	void print_rnd_individual(unsigned long derivation_index, Derivation &derivation);

	// UNS
	void print_uns_individual(unsigned long derivation_index, Derivation &derivation);

	// SOL
	void print_sol_individual_dom(Solution &solution, Direction direction, Constraint &constraint2);
	void print_sol_individual(unsigned long derivation_index, Derivation &derivation);

	void print_der_individual(unsigned long derivation_index, Derivation &derivation);
	void print_der();
	// End DER predicate
};

#endif /* CERTIFICATE_H */