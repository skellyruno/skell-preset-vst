#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

// File-system discovery only. Never execute or instantiate a third-party
// plug-in while searching. Incremental enumeration runs on the GUI timer,
// NOT the audio thread; actual VST3 validation happens only on user load.
namespace skellpreset::host {
struct InstalledVST3Bundle {
    juce::File file;
    juce::String name;
};

class InstalledVST3Index final {
public:
    static constexpr std::size_t maxFolders=512;
    static constexpr std::size_t maxBundles=1024;
    static constexpr int maxDepth=5;

    static juce::Array<juce::File> defaultRoots() {
        juce::Array<juce::File> roots;
       #if JUCE_WINDOWS
        const auto common=juce::SystemStats::getEnvironmentVariable("COMMONPROGRAMFILES",{});
        const auto local=juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA",{});
        if(common.isNotEmpty())
            roots.add(juce::File(common).getChildFile("VST3"));
        if(local.isNotEmpty())
            roots.add(juce::File(local).getChildFile("Programs")
                          .getChildFile("Common").getChildFile("VST3"));
       #endif
        return roots;
    }

    void start(const juce::Array<juce::File>& roots) {
        queue.clear();
        visited.clear();
        bundles.clear();
        next=0;
        stoppedByLimit=false;
        for(const auto& root:roots)
            queueFolder(root,0);
    }

    // Traverse a few folders per GUI tick so normal scrolling, search and
    // dismissal stay responsive. Entries under an actual .vst3 bundle are
    // never traversed, even if that bundle is a directory.
    bool step(int foldersPerCall=2) {
        const auto previous=bundles.size();
        for(int count=0;count<foldersPerCall && next<queue.size();++count) {
            if(next>=maxFolders || bundles.size()>=maxBundles) {
                stoppedByLimit=true;
                next=queue.size();
                break;
            }
            const auto folder=queue[next++];
            juce::Array<juce::File> children;
            folder.file.findChildFiles(children,juce::File::findFilesAndDirectories,false);
            for(const auto& child:children) {
                if(child.hasFileExtension(".vst3")) {
                    if(!child.isDirectory() && !child.existsAsFile())continue;
                    if(std::none_of(bundles.begin(),bundles.end(),
                        [&child](const InstalledVST3Bundle& item) {
                            return item.file==child;
                        })) {
                        bundles.push_back({child,child.getFileNameWithoutExtension()});
                        if(bundles.size()>=maxBundles){stoppedByLimit=true;break;}
                    }
                } else if(child.isDirectory() && folder.depth<maxDepth) {
                    queueFolder(child,folder.depth+1);
                }
            }
        }
        if(bundles.size()!=previous) {
            std::sort(bundles.begin(),bundles.end(),
                [](const InstalledVST3Bundle& a,const InstalledVST3Bundle& b){
                    const int name=a.name.compareIgnoreCase(b.name);
                    return name==0?a.file.getFullPathName().compareIgnoreCase(b.file.getFullPathName())<0
                                  :name<0;
                });
        }
        return bundles.size()!=previous;
    }

    bool finished() const noexcept {
        return stoppedByLimit || next>=queue.size();
    }
    bool truncated() const noexcept {return stoppedByLimit;}
    const std::vector<InstalledVST3Bundle>& entries() const noexcept {return bundles;}

    std::vector<int> filter(juce::String query) const {
        query=query.trim();
        std::vector<int> result;
        for(int i=0;i<(int)bundles.size();++i) {
            const auto& item=bundles[(std::size_t)i];
            if(query.isEmpty() || item.name.containsIgnoreCase(query)
               || item.file.getFullPathName().containsIgnoreCase(query))
                result.push_back(i);
        }
        return result;
    }

private:
    struct Folder {juce::File file;int depth=0;};
    void queueFolder(const juce::File& folder,int depth) {
        if(!folder.isDirectory() || folder.hasFileExtension(".vst3"))return;
        if(queue.size()>=maxFolders){stoppedByLimit=true;return;}
        if(std::find(visited.begin(),visited.end(),folder)!=visited.end())return;
        visited.push_back(folder);
        queue.push_back({folder,depth});
    }
    std::vector<Folder> queue;
    std::vector<juce::File> visited;
    std::vector<InstalledVST3Bundle> bundles;
    std::size_t next=0;
    bool stoppedByLimit=false;
};
} // namespace skellpreset::host
