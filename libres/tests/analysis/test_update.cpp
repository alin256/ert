#include <string>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>

#include <catch2/catch.hpp>

#include <ert/util/rng.h>
#include <ert/enkf/enkf_util.hpp>
#include <ert/res_util/matrix_blas.hpp>

#include <ert/analysis/update.hpp>
#include <ert/analysis/std_enkf.hpp>

namespace analysis {
void run_analysis_update_without_rowscaling(analysis_module_type *module,
                                            const bool_vector_type *ens_mask,
                                            const meas_data_type *forecast,
                                            obs_data_type *obs_data,
                                            rng_type *shared_rng,
                                            matrix_type *E, matrix_type *A);
}

const double a_true = 1.0;
const double b_true = 5.0;

struct model {
    double a;
    double b;

    model(double a, double b) : a(a), b(b) {}

    model(rng_type *rng) {
        double a_std = 2.0;
        double b_std = 2.0;
        // Priors with bias
        double a_bias = 0.5 * a_std;
        double b_bias = -0.5 * b_std;
        this->a = enkf_util_rand_normal(a_true + a_bias, a_std, rng);
        this->b = enkf_util_rand_normal(b_true + b_bias, b_std, rng);
    }

    double eval(double x) const { return this->a * x + this->b; }

    int size() { return 2; }
};

SCENARIO("Running analysis update without row scaling on linear model",
         "[analysis]") {

    GIVEN("Fixed prior and measurements") {

        analysis_module_type *enkf_module = analysis_module_alloc("STD_ENKF");
        analysis_module_set_var(enkf_module, ENKF_TRUNCATION_KEY_, "1.0");
        rng_type *rng = rng_alloc(MZRAN, INIT_DEFAULT);

        int ens_size = GENERATE(10, 100, 1000);

        bool_vector_type *ens_mask = bool_vector_alloc(ens_size, true);
        meas_data_type *meas_data = meas_data_alloc(ens_mask);
        obs_data_type *obs_data = obs_data_alloc(1.0);

        model true_model{a_true, b_true};

        std::vector<model> ens;
        for (int iens = 0; iens < ens_size; iens++)
            ens.emplace_back(rng);

        // prior
        int nparam = true_model.size();
        matrix_type *A = matrix_alloc(nparam, ens_size);
        for (int iens = 0; iens < ens_size; iens++) {
            const auto &model = ens[iens];
            matrix_iset(A, 0, iens, model.a);
            matrix_iset(A, 1, iens, model.b);
        }
        double a_avg_prior = matrix_get_row_sum(A, 0) / ens_size;
        double b_avg_prior = matrix_get_row_sum(A, 1) / ens_size;

        // observations and measurements
        int obs_size = 45;
        std::vector<double> sd_obs_values{10000.0, 100.0, 10.0,   1.0,
                                          0.1,     0.01,  0.00001};
        const char *obs_key = "OBS1";
        meas_block_type *mb =
            meas_data_add_block(meas_data, obs_key, 1, obs_size);
        obs_block_type *ob =
            obs_data_add_block(obs_data, obs_key, obs_size, nullptr, false);
        std::vector<double> xarg(obs_size);
        for (int i = 0; i < obs_size; i++) {
            xarg[i] = i;
        }
        for (int iens = 0; iens < ens_size; iens++) {
            const auto &m = ens[iens];
            // This is equivalent to M*psi_f in Data Assimilation: The Ensemble Kalman Filter, Geir Evensen, 2009.
            for (int iobs = 0; iobs < obs_size; iobs++)
                meas_block_iset(mb, iens, iobs, m.eval(xarg[iobs]));
        }
        std::vector<double> measurements(obs_size);
        for (int iobs = 0; iobs < obs_size; iobs++) {
            // When measurements != true model, then ml estimates != true parameters
            // This gives both a more advanced and realistic test
            // Standard normal N(0,1) noise is added to obtain this
            // The randomness ensures we are not gaming the test
            // But the difference could in principle be any non-zero scalar
            measurements[iobs] = true_model.eval(xarg[iobs]) +
                                 enkf_util_rand_normal(0.0, 1.0, rng);
        }

        // Leading to fixed Maximum likelihood estimate
        // It will equal true values when measurements are sampled without noise
        // It will also stay the same over beliefs
        double obs_mean = 0.0;
        double xarg_sum = 0.0;
        double xarg_sum_squared = 0.0;
        for (int iobs = 0; iobs < obs_size; iobs++) {
            obs_mean += measurements[iobs];
            xarg_sum += xarg[iobs];
            xarg_sum_squared += std::pow(xarg[iobs], 2);
        }
        obs_mean /= obs_size;
        double iobs_mean = xarg_sum / obs_size;
        double a_ml_numerator = 0.0;
        for (int iobs = 0; iobs < obs_size; iobs++) {
            a_ml_numerator += xarg[iobs] * (measurements[iobs] - obs_mean);
        }
        double a_ml =
            a_ml_numerator / (xarg_sum_squared - iobs_mean * xarg_sum);
        double b_ml = obs_mean - a_ml * iobs_mean;

        // Store posterior results when iterating over belief in measurements
        int n_sd = sd_obs_values.size();
        std::vector<double> a_avg_posterior(n_sd);
        std::vector<double> b_avg_posterior(n_sd);
        std::vector<double> d_posterior_ml(n_sd);
        std::vector<double> d_prior_posterior(n_sd);

        WHEN("Iterating over belief in measurements") {
            for (int i_sd = 0; i_sd < n_sd; i_sd++) {
                double obs_std = sd_obs_values[i_sd];
                for (int iobs = 0; iobs < obs_size; iobs++) {
                    // The improtant part: measurement observations stay the same
                    // What is iterated is the belief in them
                    obs_block_iset(ob, iobs, measurements[iobs], obs_std);
                }
                matrix_type *E =
                    obs_data_allocE(obs_data, rng, ens_size); // Evensen (9.19)
                matrix_type *A_iter = matrix_alloc_copy(A);   // Preserve prior

                // Create posterior sample (exact estimate, sample covariance)
                analysis::run_analysis_update_without_rowscaling(
                    enkf_module, ens_mask, meas_data, obs_data, rng, E, A_iter);

                // Extract estimates
                a_avg_posterior[i_sd] =
                    matrix_get_row_sum(A_iter, 0) / ens_size;
                b_avg_posterior[i_sd] =
                    matrix_get_row_sum(A_iter, 1) / ens_size;

                // Calculate distances
                d_posterior_ml[i_sd] =
                    std::sqrt(std::pow((a_avg_posterior[i_sd] - a_ml), 2) +
                              std::pow((b_avg_posterior[i_sd] - b_ml), 2));
                d_prior_posterior[i_sd] = std::sqrt(
                    std::pow((a_avg_prior - a_avg_posterior[i_sd]), 2) +
                    std::pow((b_avg_prior - b_avg_posterior[i_sd]), 2));

                matrix_free(E);
                matrix_free(A_iter);
            }

            // Test everything to some small (but generous) numeric precision
            double eps = 1e-2;

            // Compare with the prior-ml distance
            double d_prior_ml = std::sqrt(std::pow((a_avg_prior - a_ml), 2) +
                                          std::pow((b_avg_prior - b_ml), 2));

            THEN("All posterior estimates lie between prior and ml estimate") {
                for (int i_sd = 0; i_sd < n_sd; i_sd++) {
                    REQUIRE(d_posterior_ml[i_sd] - d_prior_ml < eps);
                    REQUIRE(d_prior_posterior[i_sd] - d_prior_ml < eps);
                }
            }

            THEN("Posterior parameter estimates improve with increased "
                 "trust in observations") {
                for (int i_sd = 1; i_sd < n_sd; i_sd++) {
                    REQUIRE(d_posterior_ml[i_sd] - d_posterior_ml[i_sd - 1] <
                            eps);
                }
            }

            THEN("At week beliefs, we should be close to the prior estimate") {
                REQUIRE(d_prior_posterior[0] < eps);
            }

            THEN("At strong beliefs, we should be close to the ml-estimate") {
                REQUIRE(d_posterior_ml[n_sd - 1] < eps);
            }
        }
        matrix_free(A);
        rng_free(rng);
        obs_data_free(obs_data);
        meas_data_free(meas_data);
        bool_vector_free(ens_mask);
        analysis_module_free(enkf_module);
    }
}