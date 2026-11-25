#include "../Source/ui/StageClock.h"
#include <cmath>
#include <cstdio>
#include <limits>
using skellpreset::stage::StageClock;
int main() {
    StageClock a,b;
    a.tick(0.,.5f);b.tick(0.,.5f);
    double x=0.,y=0.;
    for(int i=1;i<=30;++i)x=a.tick(i*1000./30.,.5f);
    for(int i=1;i<=60;++i)y=b.tick(i*1000./60.,.5f);
    if(std::abs(x-y)>1.e-10||std::abs(x-3.60)>1.e-10)return 1;
    const double stalled=a.tick(2000.,.5f);
    if(std::abs(stalled-x-.360)>1.e-10)return 2;
    a.pause();if(a.tick(100000.,1.f)!=stalled)return 3;
    if(!std::isfinite(a.tick(std::numeric_limits<double>::quiet_NaN(),0.f)))return 4;
    if(a.tick(100001.,0.f)!=stalled)return 5;
    std::puts("PASS elapsed animation: 30/60 Hz parity, 100ms stall bound, hidden/resume, invalid clock");
}
