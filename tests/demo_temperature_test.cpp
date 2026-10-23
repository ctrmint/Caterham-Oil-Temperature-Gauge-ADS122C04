#include "core/Calibration.h"
#include "core/Temperature.h"
#include "Config.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static gauge::Sample at(float t) {
    const float r = gauge::resistanceFromTemperature(t);
    assert(std::isfinite(r));
    return {true, 3.30f*r/(config::kPullUpOhms+r), 3.30f};
}
int main() {
    assert(gauge::calibrationValid());
    float t=0;
    assert(gauge::temperatureFromResistance(120.f,t));
    assert(std::fabs(t-100.f)<0.01f);
    assert(!gauge::temperatureFromResistance(1500.f,t)); // no extrapolation
    gauge::TemperatureState sample;
    auto warm=sample.update(at(70),0);
    assert(warm.has_temperature && warm.status==gauge::Status::Normal);
    auto hot=sample.update(at(139),5000);
    // EMA prevents a sudden step from instantly triggering a red alarm.
    assert(hot.has_temperature && hot.status != gauge::Status::Red);
    auto sample2=gauge::TemperatureState{};
    for (unsigned ms=0;ms<=4000;ms+=250) hot=sample2.update(at(139),ms);
    assert(hot.status==gauge::Status::Red);
    assert(sample2.maximumC()>135.f);
    std::puts("Demo calibration, interpolation and alarm test passed");
}
