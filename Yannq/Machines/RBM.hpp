#pragma once
#include <random>
#include <bitset>
#include <fstream>
#include <ios>
#include <string>
#include <cassert>
#include <Eigen/Eigen>

#include <tbb/tbb.h>

#include <nlohmann/json.hpp>

#include "Utilities/type_traits.hpp"
#include "Utilities/Utility.hpp"
#include "Serializers/SerializeEigen.hpp"

namespace yannq
{
//! \ingroup Machines
//! RBM machine
template<typename T>
class RBM
{
	static_assert(std::is_floating_point<T>::value || is_complex_type<T>::value, "T must be floating or complex");
public:
	using Scalar = T;
	using RealScalar = typename yannq::remove_complex<T>::type;

	using Matrix = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>;
	using Vector = Eigen::Matrix<T, Eigen::Dynamic, 1>;
	using VectorRef = Eigen::Ref<Vector>;
	using VectorConstRef = Eigen::Ref<const Vector>;

	using RealVector = Eigen::Matrix<RealScalar, Eigen::Dynamic, 1>;
	
	using DataT = std::tuple<Eigen::VectorXi, Vector>;

private:
	uint32_t n_; //# of qubits
	uint32_t m_; //# of hidden units

	bool useBias_;

	Matrix W_; //W should be m by n
	Vector a_; //a is length n
	Vector b_; //b is length m

public:
	RBM(uint32_t n, uint32_t m, bool useBias = true) noexcept
		: n_(n), m_(m), useBias_(useBias), W_(m, n), a_(n), b_(m) 
	{
		a_.setZero();
		b_.setZero();
		W_.setZero();
	}

	RBM() noexcept
		: useBias_{true}
	{
	}


	template<typename U, std::enable_if_t<std::is_convertible_v<U, T> && !std::is_same_v<U, T>, int> = 0>
	RBM(const RBM<U>& rhs) 
		: n_{rhs.getN()}, m_{rhs.getM()}, useBias_{rhs.useBias()}
	{
		W_ = rhs.getW().template cast<T>();
		a_ = rhs.getA().template cast<T>();
		b_ = rhs.getB().template cast<T>();
	}

	RBM(const RBM& rhs) /* noexcept */ = default;
	RBM(RBM&& rhs) /* noexcept */ = default;

	template<typename U, std::enable_if_t<std::is_convertible_v<U, T> && !std::is_same_v<U, T>, int> = 0>
	RBM& operator=(const RBM<U>& rhs) 
	{
		n_ = rhs.n_;
		m_ = rhs.m_;
		useBias_ = rhs.useBias_;

		W_ = rhs.W_.template cast<T>();
		a_ = rhs.a_.template cast<T>();
		b_ = rhs.b_.template cast<T>();

		return *this;
	}

	RBM& operator=(const RBM& rhs) /* noexcept */ = default;
	RBM& operator=(RBM&& rhs) /* noexcept */ = default;


	template<typename U, std::enable_if_t<std::is_convertible_v<T, U>, int> = 0>
	RBM<U> cast() const
	{
		RBM<U> res(n_, m_, useBias_);
		res.setA(a_.template cast<U>());
		res.setB(b_.template cast<U>());
		res.setW(W_.template cast<U>());
		return res;
	}

	nlohmann::json desc() const
	{
		return nlohmann::json
		{
			{"name", "RBM"},
			{"useBias", useBias_},
			{"n", n_},
			{"m", m_}
		};
	}

	inline uint32_t getN() const
	{
		return n_;
	}
	inline uint32_t getM() const
	{
		return m_;
	}

	inline uint32_t getDim() const
	{
		if(useBias_)
			return n_*m_ + n_ + m_;
		else
			return n_*m_;
	}

	inline bool useBias() const
	{
		return useBias_;
	}

	inline Vector calcTheta(const Eigen::VectorXi& sigma) const
	{
		Vector s = sigma.cast<T>();
		return W_*s + b_;
	}
	inline Vector calcGamma(const Eigen::VectorXi& hidden) const
	{
		Vector h = hidden.cast<T>();
		return W_.transpose()*h + a_;
	}
	
	void setUseBias(bool newBias)
	{
		useBias_ = newBias;
	}

	void resize(uint32_t n, uint32_t m)
	{
		n_ = n;
		m_ = m;

		a_.resize(n);
		b_.resize(m);
		W_.resize(m,n);

		if(!useBias_)
		{
			a_.setZero();
			b_.setZero();
		}
	}

	void conservativeResize(uint32_t newM)
	{
		Vector newB = Vector::Zero(newM);
		newB.head(m_) = b_;

		Matrix newW = Matrix::Zero(newM, n_);
		newW.topRows(m_) = W_;

		m_ = newM;
		b_ = std::move(newB);
		W_ = std::move(newW);
	}

	void setW(const Eigen::Ref<const Matrix>& m)
	{
		assert(m.rows() == W_.rows() && m.cols() == W_.cols());
		W_ = m;
	}

	void setA(const VectorConstRef& A)
	{
		assert(A.size() == a_.size());
		if(!useBias_)
			return ;
		a_ = A;
	}

	void setB(const VectorConstRef& B)
	{
		assert(B.size() == b_.size());
		if(!useBias_)
			return ;
		b_ = B;
	}


	inline const T& W(uint32_t j, uint32_t i) const
	{
		return W_.coeff(j,i);
	}
	inline const T& A(uint32_t i) const
	{
		return a_.coeff(i);
	}
	inline const T& B(uint32_t j) const
	{
		return b_.coeff(j);
	}

	inline T& W(uint32_t j, uint32_t i) 
	{
		return W_.coeffRef(j,i);
	}
	inline T& A(uint32_t i) 
	{
		return a_.coeffRef(i);
	}
	inline T& B(uint32_t j) 
	{
		return b_.coeffRef(j);
	}

	
	const Matrix& getW() const & { return W_; } 
	Matrix getW() && { return std::move(W_); } 

	const Vector& getA() const & { return a_; } 
	Vector getA() && { return std::move(a_); } 

	const Vector& getB() const & { return b_; } 
	Vector getB() && { return std::move(b_); } 


	//! update Bias A by adding v
	void updateA(const VectorConstRef& v)
	{
		assert(useBias_);
		a_ += v;
	}
	//! update Bias B by adding v
	void updateB(const VectorConstRef& v)
	{
		assert(useBias_);
		b_ += v;
	}
	//! update the weight W by adding m
	void updateW(const Eigen::Ref<const Matrix>& m)
	{
		W_ += m;
	}

	//! update all parameters.
	void updateParams(const VectorConstRef& m)
	{
		assert(m.size() == getDim());
		W_ += Eigen::Map<const Matrix>(m.data(), m_, n_);
		if(!useBias_)
			return ;
		a_ += Eigen::Map<const Vector>(m.data() + m_*n_, n_);
		b_ += Eigen::Map<const Vector>(m.data() + m_*n_ + n_, m_);
	}

	Vector getParams() const
	{
		Vector res(getDim());
		res.head(n_*m_) = Eigen::Map<const Vector>(W_.data(), W_.size());
		if(!useBias_)
			return res;

		res.segment(n_*m_, n_) = a_;
		res.segment(n_*m_ + n_, m_) = b_;
		return res;
	}

	void setParams(const VectorConstRef& r)
	{
		assert(r.size() == getDim());
		Eigen::Map<Vector>(W_.data(), W_.size()) = r.head(n_*m_);
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
		std::normal_distribution<T> nd{0, sigma};
		if(useBias_)
		{
			for(uint32_t i = 0u; i < n_; i++)
			{
				a_.coeffRef(i) = nd(re);
			}
			for(uint32_t i = 0u; i < m_; i++)
			{
				b_.coeffRef(i) = nd(re);
			}
		}
		for(uint32_t j = 0u; j < n_; j++)
		{
			for(uint32_t i = 0u; i < m_; i++)
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
			for(uint32_t i = 0u; i < n_; i++)
			{
				a_.coeffRef(i) = T{nd(re), nd(re)};
			}
			for(uint32_t i = 0u; i < m_; i++)
			{
				b_.coeffRef(i) = T{nd(re), nd(re)};
			}
		}
		for(uint32_t j = 0; j < n_; j++)
		{
			for(uint32_t i = 0u; i < m_; i++)
			{
				W_.coeffRef(i, j) = T{nd(re), nd(re)};
			}
		}
	}

	bool operator==(const RBM<T>& rhs) const
	{
		if(n_ != rhs.n_ || m_ != rhs.m_)
			return false;
		bool equalW = (W_ == rhs.W_);
		if(useBias_)
			return equalW && (a_ == rhs.a_) && (b_ == rhs.b_);
		else
			return equalW;
	}

	std::tuple<Eigen::VectorXi, Vector> makeData(const Eigen::VectorXi& sigma) const
	{
		return std::make_tuple(sigma, calcTheta(sigma));
	}

	T logCoeff(const std::tuple<Eigen::VectorXi, Vector>& t) const
	{
		using std::cosh;

		Vector ss = std::get<0>(t).template cast<T>();
		T s = a_.transpose()*ss;
		for(uint32_t j = 0u; j < m_; j++)
		{
			s += logCosh(std::get<1>(t).coeff(j));
		}
		return s;
	}

	T coeff(const std::tuple<Eigen::VectorXi, Vector>& t) const
	{
		using std::cosh;

		Vector ss = std::get<0>(t).template cast<T>();
		T s = a_.transpose()*ss;
		T p = exp(s) * std::get<1>(t).array().cosh().prod();
		return p;
	}

	Vector logDeriv(const std::tuple<Eigen::VectorXi, Vector>& t) const 
	{ 
		Vector res(getDim()); 

		Vector tanhs = std::get<1>(t).array().tanh(); 
		Vector sigma = std::get<0>(t).template cast<Scalar>();
		
		for(uint32_t i = 0u; i < n_; i++) 
		{ 
			res.segment(i*m_, m_) = sigma(i)*tanhs; 
		}
		if(!useBias_)
			return res;
		res.segment(n_*m_, n_) = sigma;
		res.segment(n_*m_ + n_, m_) = tanhs; 
		return res; 
	} 
};


template<typename T>
typename RBM<T>::Vector getPsi(const RBM<T>& qs, bool normalize)
{
	const uint32_t n = qs.getN();
	typename RBM<T>::Vector psi(1u<<n);
	tbb::parallel_for(0u, (1u << n),
		[n, &qs, &psi](uint32_t idx)
	{
		auto s = toSigma(n, idx);
		psi(idx) = qs.coeff(qs.makeData(s));
	});
	if(normalize)
		psi.normalize();
	return psi;
}

template<typename T, typename Iterable> //Iterable must be random access iterable
typename RBM<T>::Vector getPsi(const RBM<T>& qs, Iterable&& basis, bool normalize)
{
	const uint32_t n = qs.getN();
	typename RBM<T>::Vector psi(basis.size());

	tbb::parallel_for(std::size_t(0u), basis.size(),
		[n, &qs, &psi, &basis](std::size_t idx)
	{
		auto s = toSigma(n, basis[idx]);
		psi(idx) = qs.coeff(qs.makeData(s));
	});
	if(normalize)
		psi.normalize();
	return psi;
}
template<typename T>
typename RBM<T>::RealVector getProbs(const RBM<T>& qs, bool normalize)
{
	return getPsi(qs, normalize).cwiseAbs2();
}

template<typename T, typename Iterable> //Iterable must be random access iterable
typename RBM<T>::RealVector getProbs(const RBM<T>& qs, Iterable&& basis, bool normalize)
{
	return getPsi(qs, std::forward<Iterable>(basis), normalize).cwiseAbs2();
}
}//namespace yannq
