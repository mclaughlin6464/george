#ifndef _GEORGE_H_
#define _GEORGE_H_

#include <iostream>
#include <vector>
#include <Eigen/Dense>

#define TWOLNPI 1.8378770664093453

using std::vector;
using Eigen::VectorXd;
using Eigen::MatrixXd;
using Eigen::LDLT;
using Eigen::Success;

namespace George {

    class Kernel {

    public:
        Kernel () {};
        Kernel (VectorXd pars) {
            pars_ = pars;
        };

        int npars () const { return pars_.rows(); }
        virtual double evaluate (VectorXd x1, VectorXd x2) {
            return 0.0;
        };
        virtual VectorXd gradient (VectorXd x1, VectorXd x2) {
            VectorXd g = VectorXd::Zero(pars_.rows());
            return g;
        };

    protected:
        VectorXd pars_;

    };

    class IsotropicGaussianKernel : public Kernel {

    public:

        IsotropicGaussianKernel() {};
        IsotropicGaussianKernel(VectorXd pars) : Kernel (pars) {};

        virtual double evaluate (VectorXd x1, VectorXd x2) {
            VectorXd d = x1 - x2;
            double chi2 = d.dot(d) / pars_[1];
            return pars_[0] * exp(-0.5 * chi2);
        };

        virtual VectorXd gradient (VectorXd x1, VectorXd x2) {
            VectorXd d = x1 - x2, grad(pars_.rows());;
            double e = -0.5 * d.dot(d) / pars_[1], value = exp(e);
            grad(0) = value;
            grad(1) = -e / pars_[1] * pars_[0] * value;
            return grad;
        };

    };

    template <class KernelType>
    class GaussianProcess {

    private:
        KernelType kernel_;
        int info_;
        bool computed_;
        MatrixXd x_;
        LDLT<MatrixXd> L_;

    public:
        GaussianProcess (KernelType& kernel) {
            info_ = 0;
            computed_ = false;
            kernel_ = kernel;
        };

        int info () const { return info_; };
        int computed () const { return computed_; };

        int compute (MatrixXd x, VectorXd yerr)
        {
            int i, j, nsamples = x.rows();
            double value;
            MatrixXd Kxx(nsamples, nsamples);
            x_ = x;

            for (i = 0; i < nsamples; ++i) {
                for (j = i + 1; j < nsamples; ++j) {
                    value = kernel_.evaluate(x.row(i), x.row(j));
                    Kxx(i, j) = value;
                    Kxx(j, i) = value;
                }
                Kxx(i, i) = kernel_.evaluate(x.row(i), x.row(i))
                            + yerr[i]*yerr[i];
            }

            L_ = LDLT<MatrixXd>(Kxx);
            if (L_.info() != Success) return -1;

            computed_ = true;
            return 0;
        };

        double lnlikelihood (VectorXd y)
        {
            double logdet;
            VectorXd alpha;

            if (!computed_ || y.rows() != x_.rows())
                return -INFINITY;

            alpha = L_.solve(y);
            if (L_.info() != Success)
                return -INFINITY;

            logdet = log(L_.vectorD().array()).sum();
            return -0.5 * (y.transpose() * alpha + logdet + y.rows() * TWOLNPI);
        };

        VectorXd gradlnlikelihood (VectorXd y)
        {
            int i, j, k, nsamples = y.rows(), npars = kernel_.npars();
            VectorXd grad(npars), alpha;
            vector<MatrixXd> dkdt(npars);

            if (!computed_ || y.rows() != x_.rows()) {
                info_ = -1;
                return grad;
            }

            alpha = L_.solve(y);
            if (L_.info() != Success) {
                info_ = -2;
                return grad;
            }

            // Initialize the gradient matrices.
            for (i = 0; i < npars; ++i) dkdt[i] = MatrixXd(nsamples, nsamples);

            // Compute the gradient matrices.
            for (i = 0; i < nsamples; ++i)
                for (j = i; j < nsamples; ++j) {
                    grad = kernel_.gradient(x_.row(i), x_.row(j));
                    for (k = 0; k < npars; ++k) {
                        dkdt[k](i, j) = grad(k);
                        if (j > i) dkdt[k](j, i) = grad(k);
                    }
                }

            // Compute the gradient.
            for (k = 0; k < npars; ++k) {
                grad(k) = L_.solve(dkdt[k]).trace();
                for (i = 0; i < nsamples; ++i)
                    grad(k) -= alpha(i) * alpha.dot(dkdt[k].row(i));
                grad(k) *= -0.5;
            }

            return grad;
        };

        /* int predict (VectorXd y, MatrixXd x, VectorXd *mu, MatrixXd *cov); */

    };
}

#endif
// /_GEORGE_H_