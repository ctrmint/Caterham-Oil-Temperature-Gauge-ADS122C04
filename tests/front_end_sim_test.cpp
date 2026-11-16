// Transient simulation of both front-end options, checked against the firmware's own fault logic.
//
// Option A has no capacitors; option B adds C1 across A0-A1 and C2 across A1-A2 (hardware/WIRING.md).
// The circuit is stepped with the driver's conversion sequence and timing (Ads122c04Source::read),
// each conversion is the filter-weighted average of its input over the 22.27 ms window, and each
// cycle's Sample goes through gauge::convertSample. The checks:
//   - a healthy circuit reads the true resistance (both options alike) and never trips a check;
//   - every modelled wiring or part fault is a fault from the first cycle that starts after it,
//     and the cycle it happens in is a fault or still the correct reading;
//   - the bench expectations WIRING.md states (step 11) hold.
// Model assumptions: the IDAC holds its current up to AVDD - 0.9 V and falls to zero at AVDD - 0.1 V;
// the 10 uA burn-out source holds up to AVDD - 0.3 V and falls to zero at AVDD; 10 pF per pin,
// 200 pF of harness between the signal wire and the tank; no ADC input current.
//
// Default: the CI matrix. `front_end_sim_tests full` runs the full sweep (a few minutes).
#include "core/Temperature.h"
#include "Config.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using gauge::Sample;
using gauge::Status;

namespace {
enum { A0, A1, A2, S, N };   // circuit nodes; GND is 0 V and the tank an ideal source at vt

struct Circuit {
  double avdd = 3.3, vt = 0.0;
  double rref = 2000, r1 = 2200, r2 = 2200, rs = 89;   // rs <= 0: sender open
  bool rref_open = false, r1_open = false, r2_open = false;
  double c1 = 0, c2 = 0;              // option B
  double c1_short = 0, c2_short = 0;  // > 0: that pin pair shorted (a failed capacitor or a solder bridge)
  double idac = 250e-6, bcs = 10e-6;
};

// A current source with a soft compliance limit: full current up to vk, falling to zero at vz.
void source(double i0, double vk, double vz, double v, double &i, double &didv) {
  if (v <= vk) { i = i0; didv = 0; }
  else if (v >= vz) { i = 0; didv = 0; }
  else { i = i0 * (vz - v) / (vz - vk); didv = -i0 / (vz - vk); }
}

// Backward-Euler nodal solver; the current sources are linearised about the previous step.
struct Sim {
  double v[N] = {0, 0, 0, 0};
  double G[N][N], b[N];
  void cond(int i, int j, double g) {   // j < 0: to GND
    G[i][i] += g;
    if (j >= 0) { G[j][j] += g; G[i][j] -= g; G[j][i] -= g; }
  }
  void condTank(int i, double g, double vt) { G[i][i] += g; b[i] += g * vt; }
  void cap(int i, int j, double c, double dt) {
    const double g = c / dt, vp = v[i] - (j >= 0 ? v[j] : 0.0);
    cond(i, j, g);
    b[i] += g * vp;
    if (j >= 0) b[j] -= g * vp;
  }
  void capTank(int i, double c, double dt) { G[i][i] += c / dt; b[i] += c / dt * v[i]; }
  void inject(int i, double i0, double vk, double vz) {
    double cur, didv;
    source(i0, vk, vz, v[i], cur, didv);
    G[i][i] -= didv; b[i] += cur - didv * v[i];
  }
  void solve() {
    double a[N][N + 1];
    for (int i = 0; i < N; ++i) { for (int j = 0; j < N; ++j) a[i][j] = G[i][j]; a[i][N] = b[i]; }
    for (int c = 0; c < N; ++c) {
      int p = c;
      for (int r = c + 1; r < N; ++r) if (std::fabs(a[r][c]) > std::fabs(a[p][c])) p = r;
      for (int k = 0; k <= N; ++k) std::swap(a[c][k], a[p][k]);
      for (int r = 0; r < N; ++r) {
        if (r == c) continue;
        const double f = a[r][c] / a[c][c];
        for (int k = c; k <= N; ++k) a[r][k] -= f * a[c][k];
      }
    }
    for (int i = 0; i < N; ++i) v[i] = a[i][N] / a[i][i];
  }
  void step(const Circuit &c, int bcs_node, double dt) {
    std::memset(G, 0, sizeof(G)); std::memset(b, 0, sizeof(b));
    for (int i : {A0, A1, A2, S}) cond(i, -1, 1e-12);   // keeps floating nodes defined
    for (int i : {A0, A1, A2}) cap(i, -1, 10e-12, dt);
    capTank(S, 200e-12, dt);
    if (!c.rref_open) cond(A0, S, 1.0 / c.rref);
    if (!c.r1_open) cond(A1, S, 1.0 / c.r1);
    if (!c.r2_open) condTank(A2, 1.0 / c.r2, c.vt);
    if (c.rs > 0) condTank(S, 1.0 / c.rs, c.vt);
    if (c.c1 > 0) cap(A0, A1, c.c1, dt);
    if (c.c2 > 0) cap(A1, A2, c.c2, dt);
    if (c.c1_short > 0) cond(A0, A1, 1.0 / c.c1_short);
    if (c.c2_short > 0) cond(A1, A2, 1.0 / c.c2_short);
    inject(A0, c.idac, c.avdd - 0.9, c.avdd - 0.1);
    if (bcs_node >= 0) inject(bcs_node, c.bcs, c.avdd - 0.3, c.avdd);
    solve();
  }
};

// Symmetric conversion weightings bracketing the ADC's linear-phase FIR filter.
enum Weight { Boxcar, Sinc3 };
double weight(Weight w, double u) {
  if (w == Boxcar) return 1.0;
  const double x = 3.0 * u;
  if (x < 1) return x * x / 2;
  if (x < 2) return (-2 * x * x + 6 * x - 3) / 2;
  return (3 - x) * (3 - x) / 2;
}

// Ads122c04Source::read(): offset (shorted), A0-A1, A1-A2, A2 and A1 with the burn-out source on, AVDD/4.
struct Conv { int p, n, bcs; };   // p == -2: shorted inputs; p == -3: AVDD/4; n < 0: AVSS
const Conv kSeq[6] = {{-2, -1, -1}, {A0, A1, -1}, {A1, A2, -1}, {A2, -1, A2}, {A1, -1, A1}, {-3, -1, -1}};
constexpr double kPre = 0.2e-3;      // register write and read-back before START
constexpr double kWindow = 22.27e-3;  // 45 SPS single-shot
constexpr double kWait = 23e-3;       // the driver's delay before polling DRDY
constexpr double kAfter = 0.7e-3;     // polls and RDATA
constexpr double kCycle = 0.250;
constexpr double kDt = 10e-6;        // during conversions
constexpr double kIdleDt = 50e-6;    // between cycles, where nothing is measured
constexpr double kConvStep = kPre + kWait + kAfter;

// Runs whole cycles, switching from `pre` to `post` at t_fault; returns one Sample per cycle.
std::vector<Sample> run(const Circuit &pre, const Circuit &post, double t_fault, int cycles, Weight w) {
  Sim sim;
  std::vector<Sample> out;
  double t = 0;
  for (int cyc = 0; cyc < cycles; ++cyc) {
    while (t < cyc * kCycle) { sim.step(t < t_fault ? pre : post, -1, kIdleDt); t += kIdleDt; }
    double vals[6];
    for (int k = 0; k < 6; ++k) {
      const Conv &cv = kSeq[k];
      const double ws = t + kPre, we = ws + kWindow, end = t + kConvStep;
      double acc = 0, wsum = 0;
      while (t < end) {
        const Circuit &c = t < t_fault ? pre : post;
        sim.step(c, cv.bcs, kDt);
        t += kDt;
        if (t > ws && t <= we) {
          double x;
          if (cv.p == -2) x = 0.0;
          else if (cv.p == -3) x = c.avdd / 4.0;
          else x = sim.v[cv.p] - (cv.n >= 0 ? sim.v[cv.n] : 0.0);
          const double wt = weight(w, (t - ws) / kWindow);
          acc += wt * x; wsum += wt;
        }
      }
      vals[k] = std::max(-2.048, std::min(2.048 * (1.0 - 1.0 / 8388608.0), acc / wsum));   // full scale
    }
    out.push_back({true, (float)vals[0], (float)vals[1], (float)vals[2], (float)vals[3], (float)vals[4],
                   (float)(vals[5] * 4.0)});
  }
  return out;
}

// A reading carries a resistance (uncalibrated builds call it CALIBRATION REQUIRED).
bool isReading(Status s) {
  return !(s == Status::NoAdc || s == Status::BadSupply || s == Status::GroundFault ||
           s == Status::ShortCircuit || s == Status::OpenCircuit);
}

int failures = 0;
void fail(const char *what) { if (++failures <= 20) std::printf("FAIL %s\n", what); }

struct Fault { const char *name; bool pin_pair; void (*apply)(Circuit &); };
const Fault kFaults[] = {
  {"R1 open", false, [](Circuit &c) { c.r1_open = true; }},
  {"R2 or sense wire open", false, [](Circuit &c) { c.r2_open = true; }},
  {"Rref open", false, [](Circuit &c) { c.rref_open = true; }},
  {"sender open", false, [](Circuit &c) { c.rs = 0; }},
  {"sender shorted", false, [](Circuit &c) { c.rs = 0.5; }},
  {"A0-A1 shorted", true, [](Circuit &c) { c.c1_short = 0.1; }},
  {"A1-A2 shorted", true, [](Circuit &c) { c.c2_short = 0.1; }},
};

struct Matrix {
  std::vector<double> caps, rss, vts, avdds, bcss, idacs;
  std::vector<Weight> weights;
  std::vector<double> fault_rss, fault_vts, fault_avdds, fault_bcss;
  std::vector<Weight> fault_weights;
};

void healthy(const Matrix &m) {
  for (double c : m.caps) {
    int runs = 0; double worst_ppm = 0, worst_tap = 0;
    for (double rs : m.rss) for (double vt : m.vts) for (double avdd : m.avdds) for (double bcs : m.bcss)
    for (double idac : m.idacs) for (Weight w : m.weights) {
      Circuit cc; cc.rs = rs; cc.vt = vt; cc.avdd = avdd; cc.bcs = bcs; cc.idac = idac; cc.c1 = cc.c2 = c;
      const Sample s = run(cc, cc, 1e9, 2, w).back();
      const gauge::Reading r = gauge::convertSample(s);
      ++runs;
      char m2[200];
      std::snprintf(m2, sizeof m2, "healthy C=%gn Rs=%g Vt=%g AVDD=%g bcs=%g idac=%g w=%d -> %s %.4f",
                    c * 1e9, rs, vt, avdd, bcs, idac, (int)w, gauge::statusText(r.status), r.resistance_ohms);
      if (!isReading(r.status)) { fail(m2); continue; }
      const double ppm = std::fabs(r.resistance_ohms / rs - 1.0) * 1e6;
      if (ppm > 2.0) fail(m2);
      worst_ppm = std::max(worst_ppm, ppm);
      worst_tap = std::max(worst_tap, (double)(s.tap_volts - s.ground_volts - r.sensor_volts));
    }
    std::printf("healthy, C1 = C2 = %3.0f nF: %4d runs, worst resistance error %.2f ppm, worst tap check %.3f V (limit %.2f)\n",
                c * 1e9, runs, worst_ppm, worst_tap, config::kSenderTapMaxV);
  }
}

void faults(const Matrix &m) {
  // Fault times in the third cycle: just before it, after its Vs conversion, after its tap conversion.
  const double t3 = 2 * kCycle;
  const double tfs[] = {t3 - 1e-3, t3 + 3 * kConvStep + 1e-3, t3 + 5 * kConvStep + 1e-3};
  for (double c : m.caps) for (const Fault &f : kFaults) {
    int runs = 0;
    for (double rs : m.fault_rss) for (double vt : m.fault_vts) for (double avdd : m.fault_avdds)
    for (double bcs : m.fault_bcss) for (Weight w : m.fault_weights) for (double tf : tfs) {
      Circuit pre; pre.rs = rs; pre.vt = vt; pre.avdd = avdd; pre.bcs = bcs; pre.c1 = pre.c2 = c;
      Circuit post = pre; f.apply(post);
      const auto smp = run(pre, post, tf, 5, w);
      ++runs;
      for (int k = (int)(tf / kCycle); k < 5; ++k) {
        const gauge::Reading r = gauge::convertSample(smp[k]);
        const bool after = k * kCycle >= tf;
        const bool ok = !isReading(r.status) ||
                        (!after && std::fabs(r.resistance_ohms / rs - 1.0) < 5e-4);
        if (!ok) {
          char m2[240];
          std::snprintf(m2, sizeof m2, "%s C=%gn Rs=%g Vt=%g AVDD=%g bcs=%g w=%d fault at %.4f s, cycle %d -> %s %.2f ohm",
                        f.name, c * 1e9, rs, vt, avdd, bcs, (int)w, tf, k, gauge::statusText(r.status),
                        r.resistance_ohms);
          fail(m2);
        }
      }
    }
    std::printf("fault %-22s C1 = C2 = %3.0f nF: %4d runs\n", f.name, c * 1e9, runs);
  }
}

// WIRING.md bench step 11, with the 120 ohm test resistor and the tank point at the gauge's GND.
void benchExpectations() {
  const struct { int fault; Status expect; } cases[] = {
    {0, Status::OpenCircuit}, {1, Status::GroundFault}, {5, Status::OpenCircuit}, {6, Status::ShortCircuit}};
  for (double c : {0.0, 47e-9}) for (const auto &e : cases) {
    Circuit pre; pre.rs = 120; pre.c1 = pre.c2 = c;
    Circuit post = pre; kFaults[e.fault].apply(post);
    const Status got = gauge::convertSample(run(pre, post, 0.0, 2, Sinc3).back()).status;
    if (got != e.expect) {
      char m2[160];
      std::snprintf(m2, sizeof m2, "bench step 11: %s, C=%gn -> %s, expected %s", kFaults[e.fault].name,
                    c * 1e9, gauge::statusText(got), gauge::statusText(e.expect));
      fail(m2);
    }
  }
  std::puts("bench step 11 expectations checked (120 ohm, both options)");
}
}  // namespace

int main(int argc, char **argv) {
  const bool full = argc > 1 && std::strcmp(argv[1], "full") == 0;
  Matrix m;
  if (full) {
    m = {{0.0, 47e-9, 100e-9}, {44, 89, 120, 470, 1158, 2500, 4750}, {-0.095, -0.05, 0.0, 0.1, 0.195},
         {2.9, 3.3, 3.5}, {9.5e-6, 10.5e-6}, {235e-6, 265e-6}, {Boxcar, Sinc3},
         {44, 89, 470, 1158, 4750}, {-0.095, 0.0, 0.195}, {2.9, 3.3, 3.5}, {9.5e-6, 10.5e-6}, {Boxcar, Sinc3}};
  } else {
    // Corners: the extremes of the sender, tank offset and supply, the weaker burn-out source, and
    // the largest capacitors allowed.
    m = {{0.0, 47e-9, 100e-9}, {44, 4750}, {-0.095, 0.195}, {2.9, 3.5}, {9.5e-6}, {235e-6, 265e-6}, {Boxcar},
         {44, 4750}, {-0.095, 0.195}, {2.9}, {9.5e-6}, {Boxcar}};
  }
  healthy(m);
  faults(m);
  benchExpectations();
  if (failures) { std::printf("front-end simulation: %d FAILURES\n", failures); return 1; }
  std::puts("front-end simulation tests passed");
  return 0;
}
