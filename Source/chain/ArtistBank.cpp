#include "ArtistBank.h"
#include "PresetPackage.h"
#include <juce_cryptography/juce_cryptography.h>
#include <set>
#include <vector>
#if defined(SKELL_EMBEDDED_ARTIST_BANK)
#include "SkellArtistBankData.h"
#endif

namespace skellpreset::artistbank {
namespace {
constexpr juce::int64 maxManifest=1024*1024;
constexpr int maxEntries=1000;
struct Entry {
    juce::String channel, file, hash;
    Metadata meta;
    juce::MemoryBlock bytes;
    juce::StringArray history;
};
bool id(const juce::String& s) {
    return s.isNotEmpty() && s.length()<=64 && s.containsOnly("abcdefghijklmnopqrstuvwxyz0123456789_-");
}
bool label(const juce::String& s,int max) {
    return s.isNotEmpty() && s.length()<=max && !s.containsAnyOf("\r\n\t") && !s.containsChar('\0');
}
bool hash(const juce::String& s) {return s.length()==64 && s.containsOnly("0123456789abcdef");}
Metadata readMeta(const juce::var& item) {
    Metadata m{item.getProperty("artist",{}).toString(),item.getProperty("title",{}).toString(),item.getProperty("type",{}).toString(),item.getProperty("pair",{}).toString()};
    if(!label(m.artist,60) || !label(m.title,80) || (m.type!="main" && m.type!="adlib") || !id(m.pair))return {};
    return m;
}
MetadataMap installedMetadata(const juce::File& folder,const juce::var& installed) {
    MetadataMap map;
    if(const auto* obj=installed.getDynamicObject()){
        for(const auto& prop:obj->getProperties()){
            const auto& item=prop.value;const auto file=item.getProperty("file",{}).toString();
            const auto meta=readMeta(item);
            if(meta.artist.isEmpty() || !hash(item.getProperty("sha256",{}).toString()) || file.length()>155 || !file.endsWith(" [Trial].skellpreset") || file.containsAnyOf("/\\:\r\n\t") || file.contains(".."))continue;
            const auto target=folder.getChildFile(file);
            if(target.existsAsFile() && !target.isSymbolicLink())map.emplace("user:"+file.toLowerCase(),meta);
        }
    }
    return map;
}
juce::String digest(const juce::MemoryBlock& b) {return juce::SHA256(b).toHexString();}
juce::var object() {return juce::var(new juce::DynamicObject());}
bool atomicWrite(const juce::File& f,const juce::MemoryBlock& bytes) {
    if(f.isSymbolicLink() || f.getParentDirectory().createDirectory().failed())return false;
    juce::TemporaryFile pending(f);
    return pending.getFile().replaceWithData(bytes.getData(),bytes.getSize()) && pending.overwriteTargetFileWithTemporary();
}
bool readBytes(const juce::File& f,juce::MemoryBlock& b) {
    return !f.isSymbolicLink() && f.existsAsFile() && f.getSize()>0 && f.getSize()<=4*1024*1024 && f.loadFileAsData(b);
}
bool writeManifest(const juce::File& f,const juce::var& v) {
    const auto json=juce::JSON::toString(v,true);
    return json.getNumBytesAsUTF8()<=maxManifest && atomicWrite(f,juce::MemoryBlock(json.toRawUTF8(),json.getNumBytesAsUTF8()));
}
}
LibraryMutationLock::LibraryMutationLock(const juce::File& root)
    : lock("SkellPresetArtistBank-"+juce::SHA256(root.getFullPathName().toRawUTF8(),root.getFullPathName().getNumBytesAsUTF8()).toHexString().substring(0,32)),locked(lock.enter(2000)) {}
LibraryMutationLock::~LibraryMutationLock(){if(locked)lock.exit();}
SyncResult sync(const juce::File& root,const juce::String& json) {
    SyncResult out;
    auto fail=[&](const juce::String& error){out.error=error;return out;};
    if(json.isEmpty()){
        // Public/non-bundled builds can still browse previously seeded entries.
        const auto folder=root.getChildFile("Presets");
        if(root.isSymbolicLink() || folder.isSymbolicLink())return fail("Unsafe artist library path");
        const auto f=root.getChildFile("SoundMatchInstalled.json");
        if(f.existsAsFile() && !f.isSymbolicLink() && f.getSize()>0 && f.getSize()<=maxManifest){
            const auto m=juce::JSON::parse(f.loadFileAsString());
            if(m.isObject() && (int)m.getProperty("schema",0)==1)out.metadata=installedMetadata(root.getChildFile("Presets"),m.getProperty("installed",{}));
        }
        return out;
    }
    if(json.getNumBytesAsUTF8()>8*1024*1024)return fail("Artist bank exceeds size limit");
    const auto bank=juce::JSON::parse(json);
    const auto gen=bank.getProperty("generation",0);
    const auto generation=(int)gen;
    const auto entries=bank.getProperty("entries",{});
    const auto* rows=entries.getArray();
    if(!bank.isObject() || (int)bank.getProperty("schema",0)!=1 || !gen.isInt() || generation<=0 || !rows || rows->isEmpty() || rows->size()>maxEntries)
        return fail("Invalid artist bank header");
    std::vector<Entry> prepared;std::set<juce::String> channels,files;
    for(const auto& row:*rows){
        const auto preset=row.getProperty("id",{}).toString(),artistId=row.getProperty("artistId",{}).toString();
        Entry e;
        e.meta={row.getProperty("artist",{}).toString(),row.getProperty("title",{}).toString(),row.getProperty("type",{}).toString(),row.getProperty("pair",{}).toString()};
        e.hash=row.getProperty("sha256",{}).toString();
        if(!id(preset) || !id(artistId) || !label(e.meta.artist,60) || !label(e.meta.title,72) || (e.meta.type!="main" && e.meta.type!="adlib") || !id(e.meta.pair) || !hash(e.hash))return fail("Invalid artist bank identity");
        e.channel=preset+"--trial";e.file=artistId+"--"+preset+" [Trial].skellpreset";
        e.meta.title+=" [Trial]";
        if(!channels.insert(e.channel).second || !files.insert(e.file.toLowerCase()).second)return fail("Duplicate artist bank identity");
        const auto encoded=row.getProperty("payload",{}).toString();
        juce::MemoryOutputStream stream;
        if(encoded.isEmpty() || encoded.length()>350000 || !juce::Base64::convertFromBase64(stream,encoded))return fail("Invalid artist bank payload");
        e.bytes=stream.getMemoryBlock();
        if(digest(e.bytes)!=e.hash)return fail("Artist bank checksum mismatch");
        package::Document doc;
        if(package::readMemory(e.bytes,doc).failed())return fail("Invalid native preset in artist bank: "+preset);
        const auto history=row.getProperty("history",{});
        if(const auto* list=history.getArray()){
            if(list->size()>256)return fail("Artist revision history exceeds limit");
            for(const auto& value:*list){const auto h=value.toString();if(!hash(h))return fail("Invalid historical revision");e.history.addIfNotAlreadyThere(h);}
        }else return fail("Missing artist revision history");
        if(!e.history.contains(e.hash))return fail("Latest revision absent from history");
        prepared.push_back(std::move(e));
    }
    if(root.isSymbolicLink() || root.createDirectory().failed())return fail("Artist library unavailable");
    // Across editors/processes. Never acquire this on the audio or message thread.
    LibraryMutationLock lock(root);
    if(!lock.acquired())return fail("Artist library is busy");
    const auto folder=root.getChildFile("Presets"),manifestFile=root.getChildFile("SoundMatchInstalled.json");
    if(folder.isSymbolicLink() || folder.createDirectory().failed() || manifestFile.isSymbolicLink())return fail("Unsafe artist library path");
    auto manifest=object();auto installed=object();
    if(manifestFile.existsAsFile()){
        if(manifestFile.getSize()<=0 || manifestFile.getSize()>maxManifest)return fail("Invalid installed revision manifest");
        manifest=juce::JSON::parse(manifestFile.loadFileAsString());installed=manifest.getProperty("installed",{});
        if(!manifest.isObject() || (int)manifest.getProperty("schema",0)!=1 || !installed.isObject())return fail("Invalid installed revision manifest");
    }else {manifest.getDynamicObject()->setProperty("schema",1);manifest.getDynamicObject()->setProperty("installed",installed);}
    out.metadata=installedMetadata(folder,installed);
    for(const auto& e:prepared){
        const auto dest=folder.getChildFile(e.file); const auto key="user:"+e.file.toLowerCase();
        const auto old=installed.getProperty(juce::Identifier(e.channel),{});
        const auto oldHash=old.getProperty("sha256",{}).toString();
        const bool owned=old.isObject() && old.getProperty("file",{}).toString()==e.file && hash(oldHash);
        const int oldGeneration=(int)old.getProperty("generation",0);
        if(dest.isSymbolicLink()){++out.conflicts;continue;}
        juce::MemoryBlock existing;
        const bool exists=dest.exists();
        if(exists && !readBytes(dest,existing)){++out.conflicts;continue;}
        const auto actual=exists?digest(existing):juce::String{};
        if(owned && exists && out.metadata.find(key)==out.metadata.end())out.metadata.emplace(key,e.meta); // local edits retain artist identity
        if(owned && (oldGeneration>generation || (oldGeneration==generation && oldHash!=e.hash))){++out.conflicts;continue;} // older binary must never roll back a newer bank
        const auto oldMeta=readMeta(old);
        // A generation identifies the whole catalog revision, not only its audio bytes.
        // Reject conflicting labels even if the managed file needs to be restored.
        if(owned && oldGeneration==generation && oldMeta.artist.isNotEmpty() &&
           (oldMeta.artist!=e.meta.artist || oldMeta.title!=e.meta.title || oldMeta.type!=e.meta.type || oldMeta.pair!=e.meta.pair)){
            ++out.conflicts;continue;
        }
        if(exists && actual!=e.hash){
            if(!owned || actual!=oldHash || oldGeneration==generation || (oldGeneration<=0 && !e.history.contains(actual))){++out.conflicts;continue;}
            const auto history=root.getChildFile("SoundMatchHistory"),channel=history.getChildFile(e.channel);
            const auto backup=channel.getChildFile(actual+".skellpreset");
            if(history.isSymbolicLink() || channel.isSymbolicLink() || backup.isSymbolicLink()){++out.conflicts;continue;}
            juce::MemoryBlock saved;
            if(backup.exists() ? (!readBytes(backup,saved) || digest(saved)!=actual) : !atomicWrite(backup,existing)){++out.conflicts;continue;}
            juce::MemoryBlock check;
            if(!readBytes(dest,check) || digest(check)!=actual){++out.conflicts;continue;}
        }
        if(owned && exists && actual==e.hash && oldGeneration==generation && !oldMeta.artist.isEmpty()){++out.current;out.metadata[key]=oldMeta;continue;}
        if(actual!=e.hash && !atomicWrite(dest,e.bytes)){++out.conflicts;continue;}
        auto item=object();item.getDynamicObject()->setProperty("file",e.file);item.getDynamicObject()->setProperty("sha256",e.hash);item.getDynamicObject()->setProperty("generation",generation);
        item.getDynamicObject()->setProperty("artist",e.meta.artist);item.getDynamicObject()->setProperty("title",e.meta.title);item.getDynamicObject()->setProperty("type",e.meta.type);item.getDynamicObject()->setProperty("pair",e.meta.pair);
        installed.getDynamicObject()->setProperty(juce::Identifier(e.channel),item);
        if(!writeManifest(manifestFile,manifest)){
            // Keep the exact new bytes, never overwrite an intervening local edit.
            // The next scan recognizes an identical payload and repairs the manifest.
            out.error="Could not save installed artist revision manifest";return out;
        }
        out.metadata[key]=e.meta;
        if(!exists)++out.installed;else if(actual==e.hash)++out.current;else ++out.updated;
    }
    return out;
}
SyncResult syncEmbedded(const juce::File& root){
#if defined(SKELL_EMBEDDED_ARTIST_BANK)
    return sync(root,juce::String::fromUTF8(SkellArtistBankData::skell_artist_bank_json,SkellArtistBankData::skell_artist_bank_jsonSize));
#else
    return sync(root,{});
#endif
}
} // namespace skellpreset::artistbank
