#include "PresetPackage.h"
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

juce::Result validate(const Document& d) {
    if (! validText(d.name, 96) || ! validText(d.author, 96)
        || ! validText(d.bank, 64)) return juce::Result::fail("Invalid preset name, author or bank");
    if (d.mode < 0 || d.mode > 3 || d.activeSlotCount < legacySlotCount
        || d.activeSlotCount > slotCount || ! finiteRange(d.inputDb, -24, 24)
        || ! finiteRange(d.mix, 0, 1) || ! finiteRange(d.outputDb, -24, 24))
        return juce::Result::fail("Invalid global parameter range");
    for (const auto v : d.macros)
        if (! finiteRange(v, 0, 1)) return juce::Result::fail("Invalid macro value");
    std::array<bool, slotCount> used {};
    for (const auto& slot : d.slots) {
        if (slot.slotId < 0 || slot.slotId >= slotCount || used[(size_t)slot.slotId]
            || slot.type < 0 || slot.type > 30 || ! finiteRange(slot.wet, 0, 1))
            return juce::Result::fail("Invalid chain slot");
        used[(size_t)slot.slotId] = true;
        for (const auto v : slot.params)
            if (! finiteRange(v, 0, 1)) return juce::Result::fail("Invalid slot parameter");
        for (const auto q : slot.eqQ)
            if (! finiteRange(q,0,1))return juce::Result::fail("Invalid EQ bandwidth");
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
        item->setProperty("eqLowCut",slot.eqLowCut);
        item->setProperty("eqHighCut",slot.eqHighCut);
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
    const auto macros=globals.getProperty("macros", {});
    if (! macros.isArray() || macros.getArray()->size() != (int)candidate.macros.size())
        return juce::Result::fail("Eight macros required");
    for (int k=0;k<(int)candidate.macros.size();++k)
        if (! decodeFloat((*macros.getArray())[k], 0, 1, candidate.macros[(size_t)k]))
            return juce::Result::fail("Invalid macro value");
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
        if (! item.isObject()) return juce::Result::fail("Invalid chain entry");
        auto& slot=candidate.slots[(size_t)i];
        const auto enabled=item.getProperty("enabled", {});
        if (! decodeInteger(item.getProperty("slotId", {}),0,encodedSlots-1,slot.slotId)
            || ! decodeInteger(item.getProperty("type", {}),0,30,slot.type)
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
        if(!eqQ.isVoid()) {
            if(!eqQ.isArray() || eqQ.getArray()->size()!=8)
                return juce::Result::fail("Invalid eight-band EQ Q array");
            for(int k=0;k<8;++k)
                if(!decodeFloat((*eqQ.getArray())[k],0,1,slot.eqQ[(size_t)k]))
                    return juce::Result::fail("Invalid EQ Q value");
        }
        // Optional extensions keep legacy .skellpreset v1 and v2 readable.
        const auto lo=item.getProperty("eqLowCut",{});
        const auto hi=item.getProperty("eqHighCut",{});
        if((!lo.isVoid() && !decodeFloat(lo,0,1,slot.eqLowCut))
            ||(!hi.isVoid() && !decodeFloat(hi,0,1,slot.eqHighCut)))
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

juce::Result read(const juce::File& source,Document& result) {
    if (! source.existsAsFile() || source.getSize()>maxArchiveBytes
        || ! source.hasFileExtension(".skellpreset"))
        return juce::Result::fail("Not a valid .skellpreset file");
    juce::ZipFile archive(source);
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
} // namespace skellpreset::package
