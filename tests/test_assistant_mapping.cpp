#include "VocalAssistantMapping.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace skellpreset::assistant;
void check(bool b){if(!b)std::abort();}
int main(){int n=0;struct Case{Control c;int id,p;double low,high;};Case cs[]={{Control::ChorusRate,20,0,.08,3.98},{Control::ChorusDepth,20,1,1,11},{Control::ChorusVoices,20,2,2,5},{Control::ChorusSpread,20,3,0,1},{Control::FlangerRate,21,0,.05,5},{Control::FlangerDepth,21,1,.3,9.3},{Control::FlangerFeedback,21,2,-.82,.82},{Control::FlangerSpread,21,3,0,1},{Control::DelayMs,7,0,55,905},{Control::DelayFeedback,7,1,0,.8},{Control::DelayCutHz,7,2,500,18000},{Control::DeEsserHz,46,0,2000,16000},{Control::DeEsserThreshold,46,1,-80,0},{Control::DeEsserRange,46,2,0,24}};
for(auto c:cs){for(double u:{0.,.25,.5,.75,1.}){double x=u==0?c.low:u==1?c.high:c.c==Control::DeEsserHz?c.low*std::pow(c.high/c.low,u):c.low+(c.high-c.low)*u;auto m=mapControl(c.c,x);check(m&&m->type==c.id&&m->parameter==c.p&&std::abs(m->value-u)<1e-6);++n;auto e=propose(2,c.id,.3f,c.c,x);check(e&&canApply(*e,true,true,c.id,.3f)&&canApply(*e,true,true,c.id,m->value,true));++n;check(!canApply(*e,false,true,c.id,.3f)&&!canApply(*e,true,false,c.id,.3f)&&!canApply(*e,true,true,12,.3f)&&!canApply(*e,true,true,c.id,.1f));++n;}check(!mapControl(c.c,c.high+1)&&!mapControl(c.c,c.low-1)&&!mapControl(c.c,NAN)&&!propose(16,c.id,.3f,c.c,c.low));++n;}
check(!validEdit({0,12,0,.3f,.4f}));++n;auto fractional=propose(0,20,.3f,Control::ChorusSpread,.123456);check(fractional&&std::abs(fractional->after-.123f)<1e-6f&&canApply(*fractional,true,true,20,.123f,true));++n;check(!validEdit({0,20,0,.3f,.123456f}));++n;std::cout<<n<<" mapping and edit guard cases passed\n";}
