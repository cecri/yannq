#include <iostream>
#include <iomanip>
#include <chrono>
#include <fstream>
#include <ios>
#include <boost/archive/binary_oarchive.hpp>

#include <nlohmann/json.hpp>

#include "Machines/RBM.hpp"
#include "States/RBMState.hpp"
#include "Samplers/SwapSamplerPT.hpp"
#include "Samplers/HamiltonianSamplerOldPT.hpp"
#include "Serializers/SerializeRBM.hpp"
#include "Hamiltonians/XXZ.hpp"
#include "Optimizers/SGD.hpp"
#include "Optimizers/Adam.hpp"

#include "SROptimizerCG.hpp"

using namespace nnqs;
using std::ios;

int main(int argc, char** argv)
{

	using namespace nnqs;
	using nlohmann::json;

	constexpr int N  = 12;
	constexpr int numChains = 8;
	
	std::random_device rd;
	std::default_random_engine re(rd());

	std::cout << std::setprecision(8);

	const double decaying = 0.9;
	const double lmax = 10.0;
	const double lmin = 1e-3;

	//const double adam_eta = 0.05;
	const double sgd_eta = 0.05;

	using ValT = std::complex<double>;

	if(argc != 3)
	{
		printf("Usage: %s [alpha] [Delta]\n", argv[0]);
		return 1;
	}
	int alpha;
	double Delta;
	sscanf(argv[1], "%d", &alpha);
	sscanf(argv[2], "%lf", &Delta);
	std::cout << "#Delta: " << Delta << std::endl;

	using Machine = RBM<ValT, true>;
	Machine qs(N, alpha*N);
	qs.initializeRandom(re);
	XXZ ham(N, 1.0, Delta);

	const int dim = qs.getDim();

	//SGD<ValT> opt(sgd_eta);
	Adam<ValT> opt{};

	{
		json j;
		j["Optimizer"] = opt.params();
		j["Hamiltonian"] = ham.params();
		
		json lambda = 
		{
			{"decaying", decaying},
			{"lmax", lmax},
			{"lmin", lmin},
		};
		j["lambda"] = lambda;
		j["numThreads"] = Eigen::nbThreads();
		j["machine"] = qs.params();


		std::ofstream fout("params.dat");
		fout << j;
		fout.close();
	}

	typedef std::chrono::high_resolution_clock Clock;

	std::vector<std::array<int,2> > flips;
	for(int i = 0; i < N; i++)
	{
		flips.emplace_back(std::array<int,2>{i, (i+1)%N});
	}


	SwapSamplerPT<Machine, std::default_random_engine> ss(qs, numChains);
	//HamiltonianSamplerPT<Machine, std::default_random_engine, 2> ss(qs, numChains, flips);
	SRMatFree<Machine> srm(qs);
	
	ss.initializeRandomEngine();

	using std::sqrt;
	using Vector = typename Machine::Vector;

	for(int ll = 0; ll <=  3000; ll++)
	{
		/*
		if(ll % 5 == 0)
		{
			char fileName[30];
			sprintf(fileName, "w%04d.dat",ll);
			std::fstream out(fileName, ios::binary|ios::out);
			{
				boost::archive::binary_oarchive oa(out);
				oa << qs;
			}
		}

		*/
		ss.randomizeSigma(N/2);
		//ss.randomizeSigma();

		//Sampling
		auto smp_start = Clock::now();
		auto sr = ss.sampling(2*dim, int(0.4*dim));
		auto smp_dur = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - smp_start).count();

		auto slv_start = Clock::now();

		srm.constructFromSampling(sr, ham);
		double currE = srm.getEloc();
		
		Vector v;
		Vector optV;
		/*

		Eigen::ConjugateGradient<SRMatFree<Machine>, Eigen::Lower|Eigen::Upper, Eigen::IdentityPreconditioner> cg;
		double lambda = std::max(lmax*pow(decaying,ll), lmin);
		srm.setShift(lambda);
		cg.compute(srm);
		cg.setTolerance(1e-4);
		v = cg.solve(srm.getF());
		optV = opt.getUpdate(v);
		*/

		v = srm.getF();
		optV = opt.getUpdate(v);

		auto slv_dur = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - slv_start).count();

		//double cgErr = (srm.apply(v)-srm.getF()).norm();
		double nv = v.norm();

		qs.updateParams(optV);
		
		/*
		std::cout << ll << "\t" << currE << "\t" << nv << "\t" << cgErr
			<< "\t" << smp_dur << "\t" << slv_dur << std::endl;
		*/
		std::cout << ll << "\t" << currE << "\t" << nv << std::endl;

	}

	return 0;
}
