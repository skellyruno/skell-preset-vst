#include "PresetPackage.h"
#include "EffectRegistry.h"
#include <cmath>
#include <memory>

namespace skellpreset::package {
namespace {
constexpr juce::int64 maxArchiveBytes = 4 * 1024 * 1024;
constexpr juce::int64 maxJsonBytes = 256 * 1024;

bool finiteRange(float value, float low, float high) noexcept {
    return std::isfinite(value) && value >= low && value <= high;
}
bool validText(const juce::String& s, int maxLength) {
    return s.isNotEmpty() && s.length() <= maxLength && ! s.containsChar('\0');
}
bool isNumber(const juce::var& v) noexcept {
    return v.isInt() || v.isInt64() || v.isDouble();
}
bool decodeFloat(const juce::var& v, float low, float high, float& out) {
    if (! isNumber(v)) return false;
    const auto number = static_cast<double>(v);
    if (! std::isfinite(number) || number < low || number > high) return false;
    out = static_cast<float>(number);
    return true;
}
bool decodeInteger(const juce::var& v, int lo, int hi, int& out) {
    if (! isNumber(v)) return false;
    const auto number = static_cast<double>(v);
    if (! std::isfinite(number) || number != std::floor(number)
        || number < lo || number > hi) return false;
    out = (int)number;
    return true;
}
bool hasJsonProperty(const juce::var& object, const char* name) {
    const auto* dynamic = object.getDynamicObject();
    return dynamic != nullptr && dynamic->hasProperty(juce::Identifier(name));
}

juce::Result validate(const Document& d) {
    if (! validText(d.name, 96) || ! validText(d.author, 96)
        || ! validText(d.bank, 64)) return juce::Result::fail("Invalid preset name, author or bank");
    if (d.mode < 0 || d.mode > 3 || d.activeSlotCount < legacySlotCount
        || d.activeSlotCount > slotCount || ! finiteRange(d.inputDb, -24, 24)
        || ! finiteRange(d.mix, 0, 1) || ! finiteRange(d.outputDb, -24, 24)
        || d.micPreInput < 0 || d.micPreInput > 3
        || d.micPreTarget < 0 || d.micPreTarget > 3
        || ! finiteRange(d.micPreAmount,0,1)
        || ! finiteRange(d.micPreTrim,0,1)
        || ! finiteRange(d.micGateThreshold,-80.f,-10.f)
        || ! finiteRange(d.micGateRelease,20.f,1000.f)
        || ! finiteRange(d.micGateHold,0.f,250.f)
         || ! finiteRange(d.micGateAttackMs,.5f,25.f)
         || ! finiteRange(d.micGateHysteresisDb,0.f,12.f)
         || ! finiteRange(d.micGateDepth,0.f,1.f)
         || ! finiteRange(d.micTubeGain,-12.f,12.f) || ! finiteRange(d.micTubeDrive,0.f,1.f)
         || ! finiteRange(d.micTubeAir,0.f,6.f) || ! finiteRange(d.micTubeTrim,-12.f,12.f))
        return juce::Result::fail("Invalid global parameter range");
    for (const auto v : d.macros)
        if (! finiteRange(v, 0, 1)) return juce::Result::fail("Invalid macro value");
    std::array<bool, slotCount> used {};
    for (const auto& slot : d.slots) {
        if (slot.slotId < 0 || slot.slotId >= slotCount || used[(size_t)slot.slotId]
            || slot.type < 0 || slot.type >= skellpreset::registry::count()
            || ! finiteRange(slot.wet, 0, 1))
            return juce::Result::fail("Invalid chain slot");
        used[(size_t)slot.slotId] = true;
        for (const auto v : slot.params)
            if (! finiteRange(v, 0, 1)) return juce::Result::fail("Invalid slot parameter");
        for (const auto q : slot.eqQ)
            if (! finiteRange(q,0,1))return juce::Result::fail("Invalid EQ bandwidth");
        for(int k=0;k<8;++k)
            if(slot.studioEqKind[(size_t)k]<0 || slot.studioEqKind[(size_t)k]>4)
                return juce::Result::fail("Invalid Studio EQ filter type");
        for(float depth:slot.studioEqDynamic)if(!finiteRange(depth,0.f,1.f))return juce::Result::fail("Invalid EQ dynamic depth");
        for(float v:slot.studioEqThreshold)if(!finiteRange(v,-80.f,0.f))return juce::Result::fail("Invalid EQ Threshold");
        for(float v:slot.studioEqAttack)if(!finiteRange(v,0.5f,200.f))return juce::Result::fail("Invalid EQ Attack");
        for(float v:slot.studioEqRelease)if(!finiteRange(v,5.f,2000.f))return juce::Result::fail("Invalid EQ Release");
        for(float v:slot.studioEqSlope)if(!finiteRange(v,0.f,2.f)||v!=std::round(v))return juce::Result::fail("Invalid EQ Slope");
        for(float v:slot.studioEqExtraShape)if(!finiteRange(v,0.f,2.f)||v!=std::round(v))return juce::Result::fail("Invalid EQ ExtraShape");
        if(slot.duckSource<0 || slot.duckSource>2)return juce::Result::fail("Invalid duck source");
        if(!finiteRange(slot.duckDepth,0.f,1.f) ||
           !finiteRange(slot.duckThreshold,-60.f,-12.f) ||
           !finiteRange(slot.duckRelease,50.f,1500.f) ||
           !finiteRange(slot.duckAttack,2.f,80.f) ||
           !finiteRange(slot.limitAttack,0.f,3.f) || !finiteRange(slot.limitRelease,5.f,500.f))
            return juce::Result::fail("Invalid wet sidechain controls");
        if(!finiteRange(slot.eqLowCut,0,1)||!finiteRange(slot.eqHighCut,0,1))
            return juce::Result::fail("Invalid EQ cut control");
    }
    return juce::Result::ok();
}

juce::var toJson(const Document& d) {
    auto* root = new juce::DynamicObject;
    root->setProperty("schemaVersion", schemaVersion);
    root->setProperty("activeSlotCount", d.activeSlotCount);
    root->setProperty("name", d.name);
    root->setProperty("author", d.author);
    root->setProperty("bank", d.bank);
    auto* global = new juce::DynamicObject;
    global->setProperty("mode", d.mode);
    global->setProperty("inputDb", d.inputDb);
    global->setProperty("mix", d.mix);
    global->setProperty("outputDb", d.outputDb);
    global->setProperty("micPreOn", d.micPreOn);
    global->setProperty("micPreInput", d.micPreInput);
    global->setProperty("micPreTarget", d.micPreTarget);
    global->setProperty("micPreAmount", d.micPreAmount);
    global->setProperty("micPreTrim", d.micPreTrim);
    global->setProperty("micGateOn", d.micGateOn);
    global->setProperty("micGateThreshold", d.micGateThreshold);
    global->setProperty("micGateRelease", d.micGateRelease);
    global->setProperty("micGateHold", d.micGateHold);
    global->setProperty("micGateAttackMs",d.micGateAttackMs);
    global->setProperty("micGateHysteresisDb",d.micGateHysteresisDb);
    global->setProperty("micGateDepth",d.micGateDepth);
    global->setProperty("stereoSeparation",d.stereoSeparation);
    global->setProperty("micTubeOn",d.micTubeOn);
    global->setProperty("micTubeGain",d.micTubeGain);global->setProperty("micTubeDrive",d.micTubeDrive);
    global->setProperty("micTubeAir",d.micTubeAir);global->setProperty("micTubeTrim",d.micTubeTrim);
    juce::Array<juce::var> macros;
    for (float value : d.macros) macros.add(value);
    global->setProperty("macros", juce::var(macros));
    root->setProperty("globals", juce::var(global));
    juce::Array<juce::var> chain;
    for (const auto& slot : d.slots) {
        auto* item = new juce::DynamicObject;
        item->setProperty("slotId", slot.slotId);
        item->setProperty("type", slot.type);
        item->setProperty("enabled", slot.enabled);
        item->setProperty("wet", slot.wet);
        juce::Array<juce::var> params;
        for (float value : slot.params) params.add(value);
        item->setProperty("params", juce::var(params));
        juce::Array<juce::var> eqQ;
        for(float value:slot.eqQ)eqQ.add(value);
        item->setProperty("eqQ",juce::var(eqQ));
        juce::Array<juce::var> kinds,enabled;
        for(int k=0;k<8;++k){
            kinds.add(slot.studioEqKind[(size_t)k]);
            enabled.add(slot.studioEqOn[(size_t)k]);
        }
        item->setProperty("studioEqKind",juce::var(kinds));
        item->setProperty("studioEqOn",juce::var(enabled));
        juce::Array<juce::var> dynamics;for(float depth:slot.studioEqDynamic)dynamics.add(depth);
        item->setProperty("studioEqDynamic",juce::var(dynamics));
        {juce::Array<juce::var> v;for(float x:slot.studioEqThreshold)v.add(x);item->setProperty("studioEqThreshold",juce::var(v));}
        {juce::Array<juce::var> v;for(float x:slot.studioEqAttack)v.add(x);item->setProperty("studioEqAttack",juce::var(v));}
        {juce::Array<juce::var> v;for(float x:slot.studioEqRelease)v.add(x);item->setProperty("studioEqRelease",juce::var(v));}
        {juce::Array<juce::var> v;for(float x:slot.studioEqSlope)v.add(x);item->setProperty("studioEqSlope",juce::var(v));}
        {juce::Array<juce::var> v;for(float x:slot.studioEqExtraShape)v.add(x);item->setProperty("studioEqExtraShape",juce::var(v));}
        item->setProperty("eqLowCut",slot.eqLowCut);
        item->setProperty("eqHighCut",slot.eqHighCut);
        item->setProperty("duckOn",slot.duckOn);
        item->setProperty("rightFirst",slot.rightFirst);
        item->setProperty("dubDuckOn",slot.dubDuckOn);
        item->setProperty("duckSource",slot.duckSource);
        item->setProperty("parallelReturn",slot.parallelReturn);
        item->setProperty("duckExternal",slot.duckExternal);
        item->setProperty("duckEffect",slot.duckEffect);
        item->setProperty("duckDepth",slot.duckDepth);
        item->setProperty("duckThreshold",slot.duckThreshold);
        item->setProperty("duckRelease",slot.duckRelease);
        item->setProperty("duckAttack",slot.duckAttack);
        item->setProperty("limitAttack",slot.limitAttack);item->setProperty("limitRelease",slot.limitRelease);
        chain.add(juce::var(item));
    }
    root->setProperty("chain", juce::var(chain));
    return juce::var(root);
}

juce::Result fromJson(const juce::var& root, Document& out) {
    if (! root.isObject()) return juce::Result::fail("preset.json must be an object");
    Document candidate;
    int version=0;
    if (! decodeInteger(root.getProperty("schemaVersion", {}), 1, schemaVersion, version))
        return juce::Result::fail("Unsupported schema version");
    if (version == 1) candidate.activeSlotCount = legacySlotCount;
    else if (! decodeInteger(root.getProperty("activeSlotCount", {}),
                             legacySlotCount, slotCount, candidate.activeSlotCount))
        return juce::Result::fail("Invalid rack slot count");
    const auto name=root.getProperty("name", {});
    const auto author=root.getProperty("author", {});
    const auto bank=root.getProperty("bank", {});
    if (! name.isString() || ! author.isString() || ! bank.isString())
        return juce::Result::fail("Missing preset identity");
    candidate.name=name.toString();candidate.author=author.toString();candidate.bank=bank.toString();
    const auto globals=root.getProperty("globals", {});
    if (! globals.isObject() || ! decodeInteger(globals.getProperty("mode", {}), 0, 3, candidate.mode)
        || ! decodeFloat(globals.getProperty("inputDb", {}), -24, 24, candidate.inputDb)
        || ! decodeFloat(globals.getProperty("mix", {}), 0, 1, candidate.mix)
        || ! decodeFloat(globals.getProperty("outputDb", {}), -24, 24, candidate.outputDb))
        return juce::Result::fail("Invalid global values");
    // Optional added fields: do not invalidate v1 or v2 user archives.
    // Distinguish an absent property from explicit JSON null. JUCE represents
    // JSON null as a void var, so isVoid() alone would silently accept null.
    if (hasJsonProperty(globals,"micPreOn")) {
        const auto value=globals.getProperty("micPreOn",{});
        if(!value.isBool())return juce::Result::fail("Invalid mic enabled value");
        candidate.micPreOn=(bool)value;
    }
    if (hasJsonProperty(globals,"micPreInput")
        && !decodeInteger(globals.getProperty("micPreInput",{}),0,3,candidate.micPreInput))
        return juce::Result::fail("Invalid mic input profile");
    if (hasJsonProperty(globals,"micPreTarget")
        && !decodeInteger(globals.getProperty("micPreTarget",{}),0,3,candidate.micPreTarget))
        return juce::Result::fail("Invalid mic target profile");
    if (hasJsonProperty(globals,"micPreAmount")
        && !decodeFloat(globals.getProperty("micPreAmount",{}),0,1,candidate.micPreAmount))
        return juce::Result::fail("Invalid mic amount");
    if (hasJsonProperty(globals,"micPreTrim")
        && !decodeFloat(globals.getProperty("micPreTrim",{}),0,1,candidate.micPreTrim))
        return juce::Result::fail("Invalid mic trim");
    // The new input gate is an optional v2 extension. Existing packages
    // without these fields remain OFF with APVTS defaults. Explicit null,
    // malformed or out-of-range values reject the whole ZIP transactionally.
    if (hasJsonProperty(globals,"micGateOn")) {
        const auto enabled=globals.getProperty("micGateOn",{});
        if(!enabled.isBool())return juce::Result::fail("Invalid mic gate enabled value");
        candidate.micGateOn=(bool)enabled;
    }
    if (hasJsonProperty(globals,"micGateThreshold") &&
        !decodeFloat(globals.getProperty("micGateThreshold",{}),
                     -80.f,-10.f,candidate.micGateThreshold))
        return juce::Result::fail("Invalid mic gate threshold");
    if (hasJsonProperty(globals,"micGateRelease") &&
        !decodeFloat(globals.getProperty("micGateRelease",{}),
                     20.f,1000.f,candidate.micGateRelease))
        return juce::Result::fail("Invalid mic gate release");
    if (hasJsonProperty(globals,"micGateHold") &&
        !decodeFloat(globals.getProperty("micGateHold",{}),
                     0.f,250.f,candidate.micGateHold))
        return juce::Result::fail("Invalid mic gate hold");
    if(hasJsonProperty(globals,"micGateAttackMs") &&
       !decodeFloat(globals.getProperty("micGateAttackMs",{}),.5f,25.f,candidate.micGateAttackMs))
        return juce::Result::fail("Invalid mic gate attack");
    if(hasJsonProperty(globals,"micGateHysteresisDb") &&
       !decodeFloat(globals.getProperty("micGateHysteresisDb",{}),0.f,12.f,candidate.micGateHysteresisDb))
        return juce::Result::fail("Invalid mic gate hysteresis");
    if(hasJsonProperty(globals,"micGateDepth") &&
       !decodeFloat(globals.getProperty("micGateDepth",{}),0.f,1.f,candidate.micGateDepth))
        return juce::Result::fail("Invalid mic gate reduction");
    if(hasJsonProperty(globals,"micTubeOn")) {
        const auto enabled=globals.getProperty("micTubeOn",{});
        if(!enabled.isBool())return juce::Result::fail("Invalid mic preamp enable");
        candidate.micTubeOn=(bool)enabled;
    }
    struct TubeField {const char* id;float lo,hi;float* destination;};
    for(const auto& field:std::array<TubeField,4>{{{"micTubeGain",-12.f,12.f,&candidate.micTubeGain},
        {"micTubeDrive",0.f,1.f,&candidate.micTubeDrive},{"micTubeAir",0.f,6.f,&candidate.micTubeAir},
        {"micTubeTrim",-12.f,12.f,&candidate.micTubeTrim}}})
        if(hasJsonProperty(globals,field.id) && !decodeFloat(globals.getProperty(field.id,{}),field.lo,field.hi,*field.destination))
            return juce::Result::fail("Invalid mic preamp parameter");
    // Stereo separation is a strict integer in the supplied raw range. Absent
    // in pre-R6 archives, where candidate.stereoSeparation stays at neutral 0.
    if(hasJsonProperty(globals,"stereoSeparation") &&
       !decodeInteger(globals.getProperty("stereoSeparation",{}),-96,96,
                      candidate.stereoSeparation))
        return juce::Result::fail("Invalid stereo separation");
    // Schema-v2 macros were added as an optional extension. Older v1/v2
    // archives without the property retain neutral 50% host defaults.
    // If present (including explicit null), the array remains strictly
    // validated and transactional.
    if (hasJsonProperty(globals,"macros")) {
        const auto macros=globals.getProperty("macros",{});
        if (!macros.isArray() || macros.getArray()->size() != (int)candidate.macros.size())
            return juce::Result::fail("Eight macros required");
        for (int k=0;k<(int)candidate.macros.size();++k)
            if (!decodeFloat((*macros.getArray())[k],0,1,candidate.macros[(size_t)k]))
                return juce::Result::fail("Invalid macro value");
    }
    const auto chain=root.getProperty("chain", {});
    const int encodedSlots=version==1?legacySlotCount:slotCount;
    if (! chain.isArray() || chain.getArray()->size()!=encodedSlots)
        return juce::Result::fail("Invalid chain slot count for schema");
    // V1 had ten physical IDs. New inactive slots are appended in stable order.
    for (int i=encodedSlots;i<slotCount;++i) {
        candidate.slots[(size_t)i].slotId=i;
        candidate.slots[(size_t)i].type=0;
        candidate.slots[(size_t)i].enabled=false;
        candidate.slots[(size_t)i].wet=1.f;
        candidate.slots[(size_t)i].params.fill(.5f);
    }
    for (int i=0;i<encodedSlots;++i) {
        const auto item=(*chain.getArray())[i];
        if (!item.isObject() || item.getDynamicObject()==nullptr)
            return juce::Result::fail("Invalid chain entry");
        auto& slot=candidate.slots[(size_t)i];
        const auto enabled=item.getProperty("enabled", {});
        if (! decodeInteger(item.getProperty("slotId", {}),0,encodedSlots-1,slot.slotId)
            || ! decodeInteger(item.getProperty("type", {}),0,
                            skellpreset::registry::count()-1,slot.type)
            || ! enabled.isBool()
            || ! decodeFloat(item.getProperty("wet", {}),0,1,slot.wet))
            return juce::Result::fail("Invalid chain metadata");
        slot.enabled=(bool)enabled;
        const auto params=item.getProperty("params", {});
        if (! params.isArray() || params.getArray()->size()!=parameterCount)
            return juce::Result::fail("Each slot requires sixteen parameters");
        for (int k=0;k<parameterCount;++k)
            if (! decodeFloat((*params.getArray())[k],0,1,slot.params[(size_t)k]))
                return juce::Result::fail("Invalid module parameter");
        // Optional extension is compatible with native schema-v1/v2 presets.
        // Malformed Q arrays are rejected transactionally instead of ignored.
        const auto eqQ=item.getProperty("eqQ",{});
        if(hasJsonProperty(item,"eqQ")) {
            if(!eqQ.isArray() || eqQ.getArray()->size()!=8)
                return juce::Result::fail("Invalid eight-band EQ Q array");
            for(int k=0;k<8;++k)
                if(!decodeFloat((*eqQ.getArray())[k],0,1,slot.eqQ[(size_t)k]))
                    return juce::Result::fail("Invalid EQ Q value");
        }
        // Optional Studio band types and enable flags. A bad array
        // rejects the whole ZIP before any destination state is modified.
        const auto kinds=item.getProperty("studioEqKind",{});
        const auto active=item.getProperty("studioEqOn",{});
        if(hasJsonProperty(item,"studioEqKind")){
            if(!kinds.isArray() || kinds.getArray()->size()!=8)
                return juce::Result::fail("Invalid Studio EQ type array");
            for(int k=0;k<8;++k)
                if(!decodeInteger((*kinds.getArray())[k],0,4,
                        slot.studioEqKind[(size_t)k]))
                    return juce::Result::fail("Invalid Studio EQ type value");
        }
        if(hasJsonProperty(item,"studioEqOn")){
            if(!active.isArray() || active.getArray()->size()!=8)
                return juce::Result::fail("Invalid Studio EQ enable array");
            for(int k=0;k<8;++k){
                if(!(*active.getArray())[k].isBool())
                    return juce::Result::fail("Invalid Studio EQ enable value");
                slot.studioEqOn[(size_t)k]=(bool)(*active.getArray())[k];
            }
        }
        if(hasJsonProperty(item,"studioEqDynamic")){
            const auto values=item.getProperty("studioEqDynamic",{});
            if(!values.isArray()||values.getArray()->size()!=8)return juce::Result::fail("Invalid EQ dynamic array");
            for(int k=0;k<8;++k)if(!decodeFloat((*values.getArray())[k],0.f,1.f,slot.studioEqDynamic[(size_t)k]))return juce::Result::fail("Invalid EQ dynamic depth");
        }
        if(hasJsonProperty(item,"studioEqThreshold")){
            const auto values=item.getProperty("studioEqThreshold",{});
            if(!values.isArray()||values.getArray()->size()!=8)return juce::Result::fail("Invalid EQ Threshold array");
            for(int k=0;k<8;++k)if(!decodeFloat((*values.getArray())[k],-80.f,0.f,slot.studioEqThreshold[(size_t)k]))return juce::Result::fail("Invalid EQ Threshold");
        }
        if(hasJsonProperty(item,"studioEqAttack")){
            const auto values=item.getProperty("studioEqAttack",{});
            if(!values.isArray()||values.getArray()->size()!=8)return juce::Result::fail("Invalid EQ Attack array");
            for(int k=0;k<8;++k)if(!decodeFloat((*values.getArray())[k],0.5f,200.f,slot.studioEqAttack[(size_t)k]))return juce::Result::fail("Invalid EQ Attack");
        }
        if(hasJsonProperty(item,"studioEqRelease")){
            const auto values=item.getProperty("studioEqRelease",{});
            if(!values.isArray()||values.getArray()->size()!=8)return juce::Result::fail("Invalid EQ Release array");
            for(int k=0;k<8;++k)if(!decodeFloat((*values.getArray())[k],5.f,2000.f,slot.studioEqRelease[(size_t)k]))return juce::Result::fail("Invalid EQ Release");
        }
        if(hasJsonProperty(item,"studioEqSlope")){
            const auto values=item.getProperty("studioEqSlope",{});
            if(!values.isArray()||values.getArray()->size()!=8)return juce::Result::fail("Invalid EQ Slope array");
            for(int k=0;k<8;++k)if(!decodeFloat((*values.getArray())[k],0.f,2.f,slot.studioEqSlope[(size_t)k])||slot.studioEqSlope[(size_t)k]!=std::round(slot.studioEqSlope[(size_t)k]))return juce::Result::fail("Invalid EQ Slope");
        }
        if(hasJsonProperty(item,"studioEqExtraShape")){
            const auto values=item.getProperty("studioEqExtraShape",{});
            if(!values.isArray()||values.getArray()->size()!=8)return juce::Result::fail("Invalid EQ extra shape array");
            for(int k=0;k<8;++k)if(!decodeFloat((*values.getArray())[k],0.f,2.f,slot.studioEqExtraShape[(size_t)k])||slot.studioEqExtraShape[(size_t)k]!=std::round(slot.studioEqExtraShape[(size_t)k]))return juce::Result::fail("Invalid EQ extra shape");
        }
        // If absent, old presets remain dry/FX identical. Bad values are
        // rejected before ANY live destination parameters are changed.
        if(hasJsonProperty(item,"duckOn")) {
            const auto duckEnabled=item.getProperty("duckOn",{});
            if(!duckEnabled.isBool())
                return juce::Result::fail("Invalid wet sidechain enable");
            slot.duckOn=(bool)duckEnabled;
        }
        if(hasJsonProperty(item,"rightFirst")) {
            const auto v=item.getProperty("rightFirst",{});
            if(!v.isBool())return juce::Result::fail("Invalid right-first send flag");
            slot.rightFirst=(bool)v;
        }
        if(hasJsonProperty(item,"dubDuckOn")) {
            const auto v=item.getProperty("dubDuckOn",{});
            if(!v.isBool())return juce::Result::fail("Invalid dub sidechain flag");
            slot.dubDuckOn=(bool)v;
        }
        if(hasJsonProperty(item,"duckSource") && !decodeInteger(item.getProperty("duckSource",{}),0,2,slot.duckSource))
            return juce::Result::fail("Invalid duck source");
        if(hasJsonProperty(item,"duckEffect")) {
            const auto flag=item.getProperty("duckEffect",{});
            if(!flag.isBool())return juce::Result::fail("Invalid effect sidechain flag");
            slot.duckEffect=(bool)flag;
        }
        if(hasJsonProperty(item,"duckExternal")) {
            const auto flag=item.getProperty("duckExternal",{});
            if(!flag.isBool())return juce::Result::fail("Invalid external sidechain flag");
            slot.duckExternal=(bool)flag;
        }
        if(hasJsonProperty(item,"parallelReturn")) {
            const auto flag=item.getProperty("parallelReturn",{});
            if(!flag.isBool())return juce::Result::fail("Invalid parallel return flag");
            slot.parallelReturn=(bool)flag;
        }
        if((hasJsonProperty(item,"duckDepth") &&
            !decodeFloat(item.getProperty("duckDepth",{}),0.f,1.f,slot.duckDepth)) ||
           (hasJsonProperty(item,"duckThreshold") &&
            !decodeFloat(item.getProperty("duckThreshold",{}),-60.f,-12.f,
                         slot.duckThreshold)) ||
           (hasJsonProperty(item,"duckRelease") &&
            !decodeFloat(item.getProperty("duckRelease",{}),50.f,1500.f,
                         slot.duckRelease)) ||
           (hasJsonProperty(item,"duckAttack") &&
            !decodeFloat(item.getProperty("duckAttack",{}),2.f,80.f,
                         slot.duckAttack)) ||
           (hasJsonProperty(item,"limitAttack") && !decodeFloat(item.getProperty("limitAttack",{}),0.f,3.f,slot.limitAttack)) ||
           (hasJsonProperty(item,"limitRelease") && !decodeFloat(item.getProperty("limitRelease",{}),5.f,500.f,slot.limitRelease)))
            return juce::Result::fail("Invalid slot dynamics parameter");
        // Optional extensions keep legacy .skellpreset v1 and v2 readable.
        const auto lo=item.getProperty("eqLowCut",{});
        const auto hi=item.getProperty("eqHighCut",{});
        if((hasJsonProperty(item,"eqLowCut") && !decodeFloat(lo,0,1,slot.eqLowCut))
            ||(hasJsonProperty(item,"eqHighCut") && !decodeFloat(hi,0,1,slot.eqHighCut)))
            return juce::Result::fail("Invalid EQ cut value");
    }
    if (auto status=validate(candidate);status.failed())return status;
    out=std::move(candidate); // transactional: no partial changes on bad packages
    return juce::Result::ok();
}
} // namespace

juce::Result write(const juce::File& target,const Document& document) {
    if (! target.hasFileExtension(".skellpreset"))
        return juce::Result::fail("Choose a .skellpreset file");
    if (auto status=validate(document);status.failed())return status;
    const auto json=juce::JSON::toString(toJson(document), true);
    if (json.getNumBytesAsUTF8() > maxJsonBytes)
        return juce::Result::fail("Preset JSON exceeds size limit");
    juce::ZipFile::Builder builder;
    builder.addEntry(new juce::MemoryInputStream(json.toRawUTF8(), (size_t)json.getNumBytesAsUTF8(), true),
                     6, "preset.json", juce::Time::getCurrentTime());
    juce::TemporaryFile temp(target);
    {
        auto output=temp.getFile().createOutputStream();
        if (! output)return juce::Result::fail("Unable to create preset file");
        if (! builder.writeToStream(*output,nullptr))return juce::Result::fail("ZIP writer failed");
        output->flush();
        if (output->getStatus().failed())return juce::Result::fail("Preset write failed");
    }
    if (! temp.overwriteTargetFileWithTemporary())return juce::Result::fail("Could not atomically save preset");
    return juce::Result::ok();
}

static juce::Result readArchive(juce::ZipFile& archive,Document& result) {
    if (archive.getNumEntries()!=1)return juce::Result::fail("Only preset.json is supported");
    const auto* entry=archive.getEntry(0);
    if (! entry || entry->filename!="preset.json" || entry->isSymbolicLink
        || entry->uncompressedSize<=0 || entry->uncompressedSize>maxJsonBytes)
        return juce::Result::fail("Invalid or unsafe archive entry");
    std::unique_ptr<juce::InputStream> stream(archive.createStreamForEntry(0));
    if (! stream)return juce::Result::fail("Corrupted preset archive");
    juce::MemoryBlock data;
    stream->readIntoMemoryBlock(data, (size_t)maxJsonBytes+1);
    if (data.getSize()!=(size_t)entry->uncompressedSize)
        return juce::Result::fail("Truncated or oversized preset JSON");
    const auto document=juce::JSON::parse(juce::String::fromUTF8((const char*)data.getData(),(int)data.getSize()));
    return fromJson(document,result);
}
juce::Result read(const juce::File& source,Document& result) {
    if (! source.existsAsFile() || source.getSize()>maxArchiveBytes
        || ! source.hasFileExtension(".skellpreset"))
        return juce::Result::fail("Not a valid .skellpreset file");
    juce::ZipFile archive(source);
    return readArchive(archive,result);
}
juce::Result readMemory(const juce::MemoryBlock& source,Document& result) {
    if(source.getSize()==0 || source.getSize()>(size_t)maxArchiveBytes)
        return juce::Result::fail("Invalid preset byte count");
    juce::MemoryInputStream input(source,false);
    juce::ZipFile archive(input);
    return readArchive(archive,result);
}
} // namespace skellpreset::package
