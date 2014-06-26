////////////////////////////////////////////////////////////////////////////////
// vanilla_convergence_table.cpp
// -----------------------------
//
// Outputs the rate of convergence for computing the price of a
// European/American call/put.
//
// Author: Parsiad Azimzadeh
////////////////////////////////////////////////////////////////////////////////

#include <QuantPDE/Core>
#include <QuantPDE/Modules/Payoffs>

// TODO: Change these includes; shouldn't include src directory explicitly
#include <QuantPDE/src/Modules/BlackScholes.hpp>

using namespace QuantPDE;
using namespace QuantPDE::Modules;

///////////////////////////////////////////////////////////////////////////////

#include <iostream>  // cout, cerr
#include <iomanip>   // setw
#include <memory>    // unique_ptr
#include <unistd.h>  // getopt

using namespace std;

///////////////////////////////////////////////////////////////////////////////

/**
 * Prints help to stderr.
 */
void help() {
	cerr <<
"vanilla_convergence_table [OPTIONS]" << endl << endl <<
"Outputs the rate of convergence for computing the price of a European/American" << endl <<
"call/put." << endl <<
endl <<
"-A" << endl <<
endl <<
"    American option (default is European)" << endl <<
endl <<
"-d REAL" << endl <<
endl <<
"    sets the dividend rate (default is 0.)" << endl <<
endl <<
"-K REAL" << endl <<
endl <<
"    sets the strike price (default is 100.)" << endl <<
endl <<
"-N POSITIVE_INTEGER" << endl <<
endl <<
"    sets the initial number of steps to take in time (default is 25)" << endl <<
endl <<
"-p" << endl <<
endl <<
"    computes the price of a European put (default is call)" << endl <<
endl <<
"-r REAL" << endl <<
endl <<
"    sets interest rate (default is 0.04)" << endl <<
endl <<
"-R NONNEGATIVE_INTEGER" << endl <<
endl <<
"    sets the maximum number of refinement steps in the computation (default is" << endl <<
"    5)" << endl <<
endl <<
"-S REAL" << endl <<
endl <<
"    sets the initial stock price (default is 100.)" << endl <<
endl <<
"-T POSITIVE_REAL" << endl <<
endl <<
"    sets the expiry time (default is 1.)" << endl <<
endl <<
"-v REAL" << endl <<
endl <<
"    sets the volatility" << endl <<
endl <<
"-V" << endl <<
"    uses variable-size timestepping" << endl << endl;
}

int main(int argc, char **argv) {

	// Default options
	Real expiry         = 1.;
	Real interest       = 0.04;
	Real volatility     = 0.2;
	Real dividends      = 0.;
	Real asset          = 100.;
	Real strike         = 100.;
	int  refinement     = 5;
	int  steps          = 25;
	bool call           = true;
	//bool variable       = false;
	bool american       = false;

	// Setting options with getopt
	{ char c;
	while((c = getopt(argc, argv, "Ad:hK:N:pr:R:S:T:v:V")) != -1) {
		switch(c) {
			case 'A':
				american = true;
				break;
			case 'd':
				dividends = atof(optarg);
				break;
			case 'h':
				help();
				return 0;
			case 'K':
				strike = atof(optarg);
				break;
			case 'N':
				steps = atoi(optarg);
				if(steps <= 0) {
					cerr <<
"error: the number of steps must be positive" << endl;
					return 1;
				}
				break;
			case 'p':
				call = false;
				break;
			case 'r':
				interest = atof(optarg);
				break;
			case 'R':
				refinement = atoi(optarg);
				if(refinement < 0) {
					cerr <<
"error: the maximum level of refinement must be nonnegative" << endl;
					return 1;
				}
				break;
			case 'S':
				asset = atof(optarg);
				break;
			case 'T':
				if((expiry = atof(optarg)) <= 0.) {
					cerr <<
"error: expiry time must be positive" << endl;
					return 1;
				}
				break;
			case 'v':
				volatility = atof(optarg);
				break;
			case 'V':
				// TODO: Remove this
				cerr <<
"warning: variable timestepping not implemented (-V ignored)" << endl;
				//variable = true;
				break;
			case ':':
			case '?':
				cerr << endl;
				help();
				return 1;
		}
	} }

	// Setting up the table
	const int td = 20;
	cout
		<< setw(td) << "Nodes"                  << "\t"
		<< setw(td) << "Steps"                  << "\t"
		<< setw(td) << "Mean Inner Iterations"  << "\t"
		<< setw(td) << "Value"                  << "\t"
		<< setw(td) << "Change"                 << "\t"
		<< setw(td) << "Ratio"
		<< endl
	;
	cout.precision(6);
	Real previousValue = nan(""), previousChange = nan("");

	// Initial discretization
	// TODO: Create grid based on initial stock price and strike price
	RectilinearGrid1 grid(
		Axis {
			0., 10., 20., 30., 40., 50., 60., 70.,
			75., 80.,
			84., 88., 92.,
			94., 96., 98., 100., 102., 104., 106., 108., 110.,
			114., 118.,
			123.,
			130., 140., 150.,
			175.,
			225.,
			300.,
			750.,
			2000.,
			10000.
		}
	);

	// Payoff function
	Function1 payoff = bind(
		call ? Payoffs::call : Payoffs::put,
		placeholders::_1,
		strike
	);

	// Alternatively, we could have used...
	//auto payoff = QUANT_PDE_MODULES_PAYOFFS_CALL_FIXED_STRIKE(strike);
	//auto payoff = QUANT_PDE_MODULES_PAYOFFS_PUT_FIXED_STRIKE(strike);

	unsigned pow2l  = 1.; // 2^l
	for(unsigned l = 0; l < refinement; l++) {

		///////////////////////////////////////////////////////////////
		// Build spatial grid
		///////////////////////////////////////////////////////////////

		// Refine the grid
		grid.refine( RectilinearGrid1::NewTickBetweenEachPair() );

		///////////////////////////////////////////////////////////////
		// Solve problem
		///////////////////////////////////////////////////////////////

		unsigned outer;
		Real inner = nan("");
		Real value;
		{
			// Black-Scholes operator (L in V_t = LV)
			BlackScholes bs(
				grid,
				interest, volatility, dividends
			);

			// Timestepping method
			unique_ptr<Iteration> stepper(//variable
				//? (Iteration *) new ReverseVariableStepper(
				//	0.,                       // startTime
				//	expiry,                   // endTime
				//	dt,                       // dt
				//	target / pow2l            // target
				//)
				//:
				(Iteration *) new ReverseConstantStepper(
					0.,            // startTime
					expiry,        // endTime
					steps * pow2l  // steps
				)
			);

			// Time discretization method
			ReverseRannacher discretization(grid, bs);
			discretization.setIteration(*stepper);

			// American-specific components; penalty method or not?
			LinearSystemIteration *root;
			unique_ptr<ToleranceIteration> tolerance;
			unique_ptr<PenaltyMethodDifference1> penalty;
			if(american) {
				// American case
				penalty = unique_ptr<PenaltyMethodDifference1>(
					new PenaltyMethodDifference1(
						grid,
						discretization,
						[&payoff] (Real t, Real x) {
							return payoff(x);
						}
					)
				);

				tolerance = unique_ptr<ToleranceIteration>(
					new ToleranceIteration()
				);

				penalty->setIteration(*tolerance);
				stepper->setInnerIteration(*tolerance);

				root = penalty.get();
			} else {
				// European case
				root = &discretization;
			}

			// Linear system solver
			BiCGSTABSolver solver;

			// Compute solution
			auto solution = stepper->solve(
				PointwiseMap1(grid),
				PiecewiseLinear1::Factory(grid),
				payoff,
				*root,
				solver
			);

			// Outer iterations
			outer = stepper->iterations()[0];

			// Average number of inner iterations
			if(american) {
				inner = tolerance->meanIterations();
			}

			// Solution at S = stock (default is 100.)
			// Linear interpolation is used to get the value off
			// the grid
			value = solution(asset);
		}

		///////////////////////////////////////////////////////////////
		// Table
		///////////////////////////////////////////////////////////////

		// Change and ratio between successive solutions
		Real
			change = value - previousValue,
			ratio = previousChange / change
		;

		// Print out row of table
		cout
			<< scientific
			<< setw(td) << grid.size() << "\t"
			<< setw(td) << outer       << "\t"
			<< setw(td) << inner       << "\t"
			<< setw(td) << value       << "\t"
			<< setw(td) << change      << "\t"
			<< setw(td) << ratio
			<< endl
		;

		previousChange = change;
		previousValue = value;

		pow2l *= american ? 4 : 2;
	}

	return 0;
}

