#include "../Source/chain/VocalFeelRecipes.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
using namespace skellpreset::dsp;
int main(){
    assert(vocalFeelRecipes.size()==11);
    assert(std::strcmp(vocalFeelRecipes[0].name,"Init")==0);
    assert(std::strcmp(vocalFeelRecipes[10].name,"Deep Male")==0);
    for(auto& recipe:vocalFeelRecipes){
        for(float value:recipe.values)assert(std::isfinite(value)&&value>=0.f&&value<=1.f);
        assert(recipe.values[9]==.7f&&recipe.values[15]==.7f);
    }
    assert(vocalFeelRecipes[1].values[10]==.5f);
    assert(vocalFeelRecipes[2].values[13]==.6f);
    assert(vocalFeelRecipes[6].values[0]==1.f);
    assert(vocalFeelRecipes[10].values[0]==.5f);
    assert(vocalFeelRecipes[7].values[11]==.7f);
    assert(vocalFeelRecipes[9].values[10]==.7f);
    std::puts("PASS: 11 user-supplied Vocal Feel style recipes, real 16-param mappings, original voice identities");
    return 0;
}
