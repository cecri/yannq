#ifndef YANNQ_MACHINES_RBM_HPP
#define YANNQ_MACHINES_RBM_HPP
#include <random>
#include <bitset>
#include <fstream>
#include <ios>
#include <string>
#include <cassert>
#include <Eigen/Eigen>

#include <cereal/access.hpp> 
#include <cereal/types/memory.hpp>

#include <nlohmann/json.hpp>

#include "Utilities/type_traits.hpp"
#include "Utilities/Utility.hpp"
#include "Serializers/SerializeEigen.hpp"

namespace yannq
{
//! \ingroup Machines
//! RBM machine that uses biases
template<typename T>
class RBM
{
	static_assert(std::is_floating_point<T>::value || is_complex_type<T>::value, "T must be floating or complex");
public:
	using ScalarType = T;
	using RealScalarType = typename yannq::remove_complex<T>::type;

	using MatrixType = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>;
	using VectorType = Eigen::Matrix<T, Eigen::Dynamic, 1>;
	using VectorRefType = Eigen::Ref<VectorType>;
	using VectorConstRefType = Eigen::Ref<const VectorType>;

private:
	int n_; //# of qubits
	int m_; //# of hidden units

	bool useBias_;

	MatrixType W_; //W should be m by n
	VectorType a_; //a is length n
	VectorType b_; //b is length m

public:

	nlohmann::json params() const
	{
		return nlohmann::json
		{
			{"name", "RBM"},
			{"useBias", true},
			{"n", n_},
			{"m", m_}
		};
	}
	inline int getN() const
	{
		return n_;
	}
	inline int getM() const
	{
		return m_;
	}

	inline int getDim() const
	{
		return n_*m_ + n_ + m_;
	}

	inline VectorType calcTheta(const Eigen::VectorXi& sigma) const
	{
		VectorType s = sigma.cast<T>();
		return W_*s + b_;
	}
	inline VectorType calcGamma(const Eigen::VectorXi& hidden) const
	{
		VectorType h = hidden.cast<T>();
		return W_.transpose()*h + a_;
	}
	
	RBM(int n, int m, bool useBias = true)
		: n_(n), m_(m), useBias_(useBias),
		W_(m,n), a_(n), b_(m)
	{
		if(!useBias)
		{
			a_.setZero();
			b_.setZero();
		}
	}

	void resize(int n, int m)
	{
		n_ = n;
		m_ = m;

		a_.resize(n);
		b_.resize(m);
		W_.resize(m,n);
	}

	void conservativeResize(int newM)
	{
		VectorType newB = VectorType::Zero(newM);
		newB.head(m_) = b_;

		MatrixType newW = MatrixType::Zero(newM, n_);
		newW.topLeftCorner(m_, n_) = W_;

		m_ = newM;
		b_ = std::move(newB);
		W_ = std::move(newW);
	}

	template<typename U>
	RBM(const RBM<U>& rhs)
		: n_(rhs.getN()), m_(rhs.getM()), W_(rhs.getW()), a_(rhs.getA()), b_(rhs.getB())
	{
		static_assert(std::is_convertible<U,T>::value, "U should be convertible to T");
	}

	RBM(const RBM& rhs)
		: n_(rhs.getN()), m_(rhs.getM()), W_(rhs.getW()), a_(rhs.getA()), b_(rhs.getB())
	{
	}
	
	RBM(RBM<T>&& rhs)
		: n_(rhs.n_), m_(rhs.m_), W_(std::move(rhs.W_)), a_(std::move(rhs.a_)), b_(std::move(rhs.b_))
	{
	}

	void setW(const Eigen::Ref<MatrixType>& m)
	{
		assert(m.rows() == W_.rows() && m.cols() == W_.cols());
		W_ = m;
	}

	void setA(const VectorConstRefType& A)
	{
		assert(A.size() == a_.size());
		a_ = A;
	}

	void setB(const VectorConstRefType& B)
	{
		assert(B.size() == b_.size());
		b_ = B;
	}

	template<typename U>
	RBM& operator=(const RBM<U>& rhs)
	{
		static_assert(std::is_convertible<U,T>::value, "U should be convertible to T");

		if(this == &rhs)
			return *this;

		n_ = rhs.n_;
		m_ = rhs.m_;
		useBias_ = rhs.useBias_;

		W_ = rhs.W_;
		a_ = rhs.a_;
		b_ = rhs.b_;

		return *this;
	}

	~RBM() = default;

	T W(int j, int i) const
	{
		return W_.coeff(j,i);
	}
	T A(int i) const
	{
		return a_.coeff(i);
	}
	T B(int j) const
	{
		return b_.coeff(j);
	}
	
	const MatrixType& getW() const & { return W_; } 
	MatrixType getW() && { return std::move(W_); } 

	const VectorType& getA() const & { return a_; } 
	VectorType getA() && { return std::move(a_); } 

	const VectorType& getB() const & { return b_; } 
	VectorType getB() && { return std::move(b_); } 


	//! update Bias A by adding v
	void updateA(const VectorConstRefType& v)
	{
		assert(!useBias_);
		a_ += v;
	}
	//! update Bias B by adding v
	void updateB(const VectorConstRefType& v)
	{
		assert(!useBias_);
		b_ += v;
	}
	//! update the weight W by adding m
	void updateW(const Eigen::Ref<const Eigen::MatrixXd>& m)
	{
		W_ += m;
	}

	//! update all parameters.
	void updateParams(const VectorConstRefType& m)
	{
		assert(m.size() == getDim());
		W_ += Eigen::Map<const MatrixType>(m.data(), m_, n_);
		if(!useBias_)
			return ;
		a_ += Eigen::Map<const VectorType>(m.data() + m_*n_, n_);
		b_ += Eigen::Map<const VectorType>(m.data() + m_*n_ + n_, m_);
	}

	VectorType getParams() const
	{
		VectorType res(getDim());
		res.head(n_*m_) = Eigen::Map<const VectorType>(W_.data(), W_.size());
		if(!useBias_)
			return res;

		res.segment(n_*m_, n_) = a_;
		res.segment(n_*m_ + n_, m_) = b_;
		return res;
	}

	void setParams(const VectorConstRefType& r)
	{
		Eigen::Map<VectorType>(W_.data(), W_.size()) = r.head(n_*m_);
		if(!useBias_)
			return ;
		a_ = r.segment(n_*m_, n_);
		b_ = r.segment(n_*m_ + n_, m_);
	}

	bool hasNaN() const
	{
		return a_.hasNaN() || b_.hasNaN() || W_.hasNaN();
	}

	/* When T is real type */
	template <typename RandomEngine, class U=T,
            	std::enable_if_t < !is_complex_type<U>::value, int > = 0 >
	void initializeRandom(RandomEngine& re, T sigma = 1e-3)
	{
		std::normal_distribution<double> nd{0, sigma};
		if(useBias_)
		{
			for(int i = 0; i < n_; i++)
			{
				a_.coeffRef(i) = nd(re);
			}
			for(int i = 0; i < m_; i++)
			{
				b_.coeffRef(i) = nd(re);
			}
		}
		for(int j = 0; j < n_; j++)
		{
			for(int i = 0; i < m_; i++)
			{
				W_.coeffRef(i, j) = nd(re);
			}
		}
	}

	/* When T is complex type */
	template <typename RandomEngine, class U=T,
               std::enable_if_t < is_complex_type<U>::value, int > = 0 >
	void initializeRandom(RandomEngine& re, typename remove_complex<T>::type sigma = 1e-3)
	{
		std::normal_distribution<typename remove_complex<T>::type> nd{0, sigma};
		
		if(useBias_)
		{
			for(int i = 0; i < n_; i++)
			{
				a_.coeffRef(i) = T{nd(re), nd(re)};
			}
			for(int i = 0; i < m_; i++)
			{
				b_.coeffRef(i) = T{nd(re), nd(re)};
			}
		}
		for(int j = 0; j < n_; j++)
		{
			for(int i = 0; i < m_; i++)
			{
				W_.coeffRef(i, j) = T{nd(re), nd(re)};
			}
		}
	}

	bool operator==(const RBM<T>& rhs) const
	{
		if(n_ != rhs.n_ || m_ != rhs.m_)
			return false;
		return (a_ == rhs.a_) || (b_ == rhs.b_) || (W_ == rhs.W_);
	}

	std::tuple<Eigen::VectorXi, VectorType> makeData(const Eigen::VectorXi& sigma) const
	{
		return std::make_tuple(sigma, calcTheta(sigma));
	}

	T logCoeff(const std::tuple<Eigen::VectorXi, VectorType>& t) const
	{
		using std::cosh;

		VectorType ss = std::get<0>(t).template cast<T>();
		T s = a_.transpose()*ss;
		for(int j = 0; j < m_; j++)
		{
			s += logCosh(std::get<1>(t).coeff(j));
		}
		return s;
	}

	T coeff(const std::tuple<Eigen::VectorXi, VectorType>& t) const
	{
		using std::cosh;

		VectorType ss = std::get<0>(t).template cast<T>();
		T s = a_.transpose()*ss;
		T p = exp(s) * std::get<1>(t).array().cosh().prod();
		return p;
	}

	VectorType logDeriv(const std::tuple<Eigen::VectorXi, VectorType>& t) const 
	{ 
		VectorType res(getDim()); 

		VectorType tanhs = std::get<1>(t).array().tanh(); 
		VectorType sigma = std::get<0>(t).template cast<ScalarType>();
		
		for(int i = 0; i < n_; i++) 
		{ 
			res.segment(i*m_, m_) = sigma(i)*tanhs; 
		}
		if(!useBias_)
			return res;
		res.segment(n_*m_, n_) = sigma;
		res.segment(n_*m_ + n_, m_) = tanhs; 
		return res; 
	} 

	uint32_t widx(int i, int j) const
	{
		return i*m_ + j;
	}


	/* Serialization using cereal */
	template<class Archive>
	void serialize(Archive& ar)
	{
		ar(useBias_);
		ar(n_,m_);
		ar(W_);
		if(!useBias_)
			return ;
		ar(a_,b_);
	}

};


template<typename T>
typename RBM<T>::VectorType getPsi(const RBM<T>& qs, bool normalize)
{
	const int n = qs.getN();
	typename RBM<T>::VectorType psi(1<<n);
#pragma omp parallel for schedule(dynamic, 8)
	for(uint64_t i = 0; i < (1u<<n); i++)
	{
		auto s = toSigma(n, i);
		psi(i) = qs.coeff(std::make_tuple(s, qs.calcTheta(s)));
	}
	if(normalize)
		psi.normalize();
	return psi;
}

template<typename T>
typename RBM<T>::VectorType getPsi(const RBM<T>& qs, const std::vector<uint32_t>& basis, bool normalize)
{
	const int n = qs.getN();
	typename RBM<T>::VectorType psi(basis.size());
#pragma omp parallel for schedule(dynamic, 8)
	for(uint64_t i = 0; i < basis.size(); i++)
	{
		auto s = toSigma(n, basis[i]);
		psi(i) = qs.coeff(qs.makeData(s));
	}
	if(normalize)
		psi.normalize();
	return psi;
}
}//namespace yannq

namespace cereal
{
	template <typename T>
	struct LoadAndConstruct<yannq::RBM<T> >
	{
		template<class Archive>
		static void load_and_construct(Archive& ar, cereal::construct<yannq::RBM<T> >& construct)
		{
			bool useBias;
			ar(useBias);

			int n,m;
			ar(n, m);

			construct(n, m, useBias);
			
			Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> W;
			ar(W);
			construct->setW(W);

			if(!useBias)
				return ;
			Eigen::Matrix<T, Eigen::Dynamic, 1> A;
			Eigen::Matrix<T, Eigen::Dynamic, 1> B;
			ar(A, B);

			construct->setA(A);
			construct->setB(B);
		}
	};
}//namespace cereal
#endif//YANNQ_MACHINES_RBM_HPP
