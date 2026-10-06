// Standalone accuracy/determinism test for src/utility/FixedMath.{h,cpp}.
// Deterministic fixed-point exp/ln/pow/sqrt/logistic must track their real-valued
// counterparts within tolerance over the secular-cycle model's domain.
//
// Run from repo root:
//   g++ -std=c++17 -O2 -I. tests/fixed_math_test.cpp src/utility/FixedMath.cpp -o fixed_math_test && ./fixed_math_test
// Exit code = number of failures.

#include "src/utility/FixedMath.h"

#include <cmath>
#include <cstdio>
#include <cstdint>

using rota::Fixed64;
namespace fm = rota::fmath;

static int g_pass = 0, g_fail = 0;
static const double TOL = 1e-4;

static void check(const char* label, double got, double expected, double tol = TOL) {
    double err = std::fabs(got - expected);
    if (err <= tol) { ++g_pass; }
    else { ++g_fail; std::printf("FAIL: %-26s got %.8f  expected %.8f  (err %.2e)\n",
                                 label, got, expected, err); }
}

static Fixed64 F(double v) { return Fixed64::from_raw((int64_t)llround(v * Fixed64::SCALE)); }

int main() {
    // ---- sqrt ----
    check("sqrt(4)",      fm::sqrt(F(4.0)).to_double(),   2.0);
    check("sqrt(2)",      fm::sqrt(F(2.0)).to_double(),   std::sqrt(2.0));
    check("sqrt(0.25)",   fm::sqrt(F(0.25)).to_double(),  0.5);
    check("sqrt(1000)",   fm::sqrt(F(1000.0)).to_double(),std::sqrt(1000.0), 1e-2);
    check("sqrt(0)",      fm::sqrt(F(0.0)).to_double(),   0.0);

    // ---- exp ----
    for (double x : {-12.0, -3.0, -1.0, -0.5, 0.0, 0.5, 1.0, 3.0, 10.0, 12.0}) {
        char l[32]; std::snprintf(l, sizeof l, "exp(%.1f)", x);
        double tol = std::exp(x) > 100 ? std::exp(x) * 2e-3 : TOL;   // relative tol for big values
        check(l, fm::exp(F(x)).to_double(), std::exp(x), tol);
    }

    // ---- ln ----
    for (double x : {0.05, 0.5, 1.0, 2.0, std::exp(1.0), 10.0, 100.0}) {
        char l[32]; std::snprintf(l, sizeof l, "ln(%.3f)", x);
        check(l, fm::ln(F(x)).to_double(), std::log(x), 1e-3);
    }
    // round trip ln(exp(x)) ~ x
    for (double x : {-2.0, -0.5, 0.3, 1.0, 2.5}) {
        char l[40]; std::snprintf(l, sizeof l, "ln(exp(%.1f))", x);
        check(l, fm::ln(fm::exp(F(x))).to_double(), x, 1e-3);
    }

    // ---- pow (the two dynamics-feeding fractional powers) ----
    for (double b : {0.3, 0.6, 1.0, 1.26, 1.5}) {
        char l[32]; std::snprintf(l, sizeof l, "pow(%.2f,1.2)", b);
        check(l, fm::pow(F(b), F(1.2)).to_double(), std::pow(b, 1.2), 1e-3);
    }
    for (double b : {0.02, 0.2, 0.5, 0.98, 1.0}) {
        char l[32]; std::snprintf(l, sizeof l, "pow(%.2f,1.5)", b);
        check(l, fm::pow(F(b), F(1.5)).to_double(), std::pow(b, 1.5), 1e-3);
        // model uses U_e * sqrt(U_e) for ^1.5 — verify the identity holds in fixed point too
        char l2[40]; std::snprintf(l2, sizeof l2, "%.2f*sqrt = ^1.5", b);
        check(l2, (F(b) * fm::sqrt(F(b))).to_double(), std::pow(b, 1.5), 1e-3);
    }

    // ---- logistic (the wage-share curve: get_wage_share uses k=-10, d_crit=0.9; wage uses k~7.2) ----
    auto L = [](double x, double k, double x0) { return 1.0 / (1.0 + std::exp(-k * (x - x0))); };
    check("logistic(0,1,0)",        fm::logistic(F(0), F(1), F(0)).to_double(),      0.5);
    check("logistic(0.9,-10,0.9)",  fm::logistic(F(0.9), F(-10), F(0.9)).to_double(), L(0.9,-10,0.9));
    check("logistic(1.2,-10,0.9)",  fm::logistic(F(1.2), F(-10), F(0.9)).to_double(), L(1.2,-10,0.9));
    check("logistic(0.5,-10,0.9)",  fm::logistic(F(0.5), F(-10), F(0.9)).to_double(), L(0.5,-10,0.9));
    check("logistic(1.0,7.2,1.0)",  fm::logistic(F(1.0), F(7.2), F(1.0)).to_double(), L(1.0,7.2,1.0));

    // ---- underflow probe: smallest real per-tick increments must survive Fixed64 (no rescale needed) ----
    // k_attrition * U_e^1.5 * elites  ~ 0.01 * 0.3^1.5 * 0.02
    Fixed64 attrition = F(0.01) * fm::pow(F(0.3), F(1.5)) * F(0.02);
    check("attrition increment", attrition.to_double(), 0.01 * std::pow(0.3,1.5) * 0.02, 5e-6);
    // mu_0 * misery * commoner_N ~ 0.002 * 0.3 * 0.6
    Fixed64 mobility = F(0.002) * F(0.3) * F(0.6);
    check("mobility increment", mobility.to_double(), 0.002 * 0.3 * 0.6, 5e-6);

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
