#pragma once
#include "NativeVocalPreview.h"
#include <array>

namespace skellpreset::preview {
inline bool readStereo(juce::AudioFormatReader& reader,juce::AudioBuffer<float>& audio,
                       juce::int64 offset,int n){
    audio.clear();
    const int count=(int)std::clamp(reader.lengthInSamples-offset,(juce::int64)0,(juce::int64)n);
    if(!count)return true;
    if(!reader.read(&audio,0,count,offset,true,reader.numChannels==2))return false;
    if(reader.numChannels==1)audio.copyFrom(1,0,audio,0,0,count);
    return true;
}
inline juce::Result measureFile(juce::AudioFormatReader& reader,juce::int64 frames,Metrics& m){
    juce::AudioBuffer<float> buffer(2,512);
    for(juce::int64 pos=0;pos<frames;pos+=512){
        const int n=(int)std::min((juce::int64)512,frames-pos);
        if(!readStereo(reader,buffer,pos,n)||!measure(m,buffer,0,n))
            return juce::Result::fail("Invalid audition samples.");
    }
    return juce::Result::ok();
}
using Wave=std::array<float,256>;
inline juce::Result writeMatched(juce::AudioFormatReader& reader,const juce::File& file,
                                 juce::int64 length,double gain,Wave& peaks){
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    if(!stream)return juce::Result::fail("Cannot open audition output.");
    auto writer=wav.createWriterFor(stream,juce::AudioFormatWriter::Options{}
        .withSampleRate(reader.sampleRate).withNumChannels(2).withBitsPerSample(32)
        .withSampleFormat(juce::AudioFormatWriter::Options::SampleFormat::floatingPoint));
    if(!writer)return juce::Result::fail("Cannot create audition WAV.");
    juce::AudioBuffer<float> audio(2,512);
    for(juce::int64 pos=0;pos<length;pos+=512){
        const int n=(int)std::min((juce::int64)512,length-pos);
        if(!readStereo(reader,audio,pos,n))return juce::Result::fail("Audition read failed.");
        audio.applyGain(0,n,(float)gain);
        for(int i=0;i<n;++i){
            const float peak=std::max(std::abs(audio.getSample(0,i)),std::abs(audio.getSample(1,i)));
            if(!std::isfinite(peak)||peak>.891252f)return juce::Result::fail("Audition peak safety check failed.");
            const auto bin=(size_t)std::min((juce::int64)255,(pos+i)*256/length);
            peaks[bin]=std::max(peaks[bin],peak);
        }
        if(!writer->writeFromAudioSampleBuffer(audio,0,n))return juce::Result::fail("Audition write failed.");
    }
    return juce::Result::ok();
}
inline juce::String waveSvg(const Wave& peaks,const char* colour){
    juce::String s="<svg viewBox='0 0 512 100' preserveAspectRatio='none' aria-hidden='true'>";
    for(size_t i=0;i<peaks.size();++i){
        const float half=48.f*peaks[i];
        s+="<path d='M"+juce::String((int)i*2+1)+" "+juce::String(50.f-half,2)
            +"V"+juce::String(50.f+half,2)+"' stroke='"+colour+"' stroke-width='1.6'/>";
    }
    return s+"</svg>";
}
inline juce::String player(const Wave& dry,const Wave& wet,double a,double b){
    // No user-controlled strings or file paths are inserted into HTML.
    juce::String html=R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Skelly Sound Lab | A/B</title><style>
:root{color-scheme:dark}*{box-sizing:border-box}body{margin:auto;padding:28px;max-width:850px;background:#101010;color:#f0f1ef;font:16px system-ui}h1{font-size:26px}p{color:#b7bdb7;line-height:1.5}.wave{border:1px solid #343b34;border-radius:12px;background:#171b17;margin:15px 0;padding:14px}svg{width:100%;height:100px;display:block}button{font:inherit;color:inherit;background:#252b25;border:1px solid #4c594c;border-radius:7px;padding:10px 18px;cursor:pointer}button[aria-pressed=true]{background:#38653e;border-color:#8acb84}input{width:100%;margin:20px 0;accent-color:#9bd88b}small{color:#aeb7ae}
</style><h1>Skelly Sound Lab · Level-matched A/B</h1><p>Aligned audio files, attenuated to equal source-window RMS. No limiter or automatic boost. Select dry or processed, then listen at the same position.</p>
<div class="wave"><strong>A · Dry</strong>)HTML";
    html+=waveSvg(dry,"#a3aaa3")+"<small>Trim: "+juce::String(a,2)+" dB</small></div><div class='wave'><strong>B · Processed</strong>"+waveSvg(wet,"#8cce80")+"<small>Trim: "+juce::String(b,2)+" dB</small></div>";
    html+=R"HTML(<audio id="dry" src="A-dry.wav" preload="metadata"></audio><audio id="wet" src="B-processed.wav" preload="metadata"></audio>
<button id="a" aria-pressed="true">A · Dry</button> <button id="b" aria-pressed="false">B · Processed</button> <button id="play">Play</button> <button id="reset">Reset</button>
<input id="seek" type="range" min="0" max="1000" value="0" aria-label="Playback position"><p id="status" role="status">Ready</p><small>Local files only. The unmatched processed render is retained separately.</small>
<script>
const tracks=[document.getElementById('dry'),document.getElementById('wet')],buttons=[document.getElementById('a'),document.getElementById('b')],play=document.getElementById('play'),seek=document.getElementById('seek'),status=document.getElementById('status');let selected=0,version=0;
function current(){return tracks[selected]}
function update(){const t=current();play.textContent=t.paused?'Play':'Pause';if(Number.isFinite(t.duration)&&t.duration>0)seek.value=Math.round(1000*t.currentTime/t.duration)}
async function choose(i){if(i===selected)return;const old=current(),pos=old.currentTime,running=!old.paused,token=++version;old.pause();selected=i;buttons.forEach((b,k)=>b.setAttribute('aria-pressed',String(k===i)));const next=current();if(next.readyState<1){status.textContent='Loading audio…';await new Promise(resolve=>{next.addEventListener('loadedmetadata',resolve,{once:true});next.addEventListener('error',resolve,{once:true})})}if(token!==version)return;if(!Number.isFinite(next.duration)){status.textContent='Cannot read local WAV';return}next.currentTime=Math.min(pos,next.duration);if(running)await next.play().catch(()=>{status.textContent='Press Play to continue'});update()}
buttons.forEach((b,i)=>b.onclick=()=>choose(i));play.onclick=async()=>{const t=current();if(t.paused)await t.play().catch(()=>status.textContent='Cannot play local WAV');else t.pause();update()};document.getElementById('reset').onclick=()=>{++version;tracks.forEach(t=>{t.pause();if(t.readyState>=1)t.currentTime=0});update()};seek.oninput=()=>{const t=current();if(Number.isFinite(t.duration))t.currentTime=Number(seek.value)*t.duration/1000;update()};tracks.forEach(t=>{t.addEventListener('timeupdate',update);t.addEventListener('ended',update);t.addEventListener('loadedmetadata',update)});</script></html>)HTML";
    return html;
}
inline juce::Result renderMatchedPair(const juce::File& source,const juce::File& output,
                                      int starter,const juce::File& package={},
                                      double tailSeconds=0.,double bpm=120.,int variant=0){
    if(output.exists()||!output.getParentDirectory().isDirectory())
        return juce::Result::fail("Choose a new audition folder in an existing directory.");
    const auto staging=output.getParentDirectory().getNonexistentChildFile(".skelly-audition","",false);
    if(staging.createDirectory().failed())return juce::Result::fail("Cannot create audition staging folder.");
    struct Cleanup{juce::File folder;~Cleanup(){folder.deleteRecursively();}} cleanup{staging};
    const auto raw=staging.getChildFile("processed-raw.wav");
    if(auto result=render(source,raw,starter,package,tailSeconds,bpm,variant);result.failed())return result;
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> dry(formats.createReaderFor(source)),wet(formats.createReaderFor(raw));
    if(!dry||!wet||dry->sampleRate!=wet->sampleRate||wet->lengthInSamples<dry->lengthInSamples)
        return juce::Result::fail("Audition sources are not aligned.");
    Metrics a,b,full;
    if(measureFile(*dry,dry->lengthInSamples,a).failed()||
       measureFile(*wet,dry->lengthInSamples,b).failed()||
       measureFile(*wet,wet->lengthInSamples,full).failed())
        return juce::Result::fail("Cannot measure audition source and tail.");
    auto gain=audition::match(a,b);
    if(!gain.valid)return juce::Result::fail("Cannot match silence or a level difference greater than 12 dB.");
    const double peak=std::max(a.peak*gain.dry,full.peak*gain.wet);
    const double trim=peak>0.?std::min(1.,.891250938/peak):1.;
    gain.dry*=trim;gain.wet*=trim;
    Wave aw{},bw{};
    if(auto result=writeMatched(*dry,staging.getChildFile("A-dry.wav"),wet->lengthInSamples,gain.dry,aw);result.failed())return result;
    if(auto result=writeMatched(*wet,staging.getChildFile("B-processed.wav"),wet->lengthInSamples,gain.wet,bw);result.failed())return result;
    dry.reset();wet.reset();
    if(!staging.getChildFile("listen.html").replaceWithText(player(aw,bw,audition::db(gain.dry),audition::db(gain.wet))))
        return juce::Result::fail("Cannot create local audition player.");
    if(output.exists()||!staging.moveFileTo(output))return juce::Result::fail("Cannot publish audition folder.");
    std::puts("Matched audition exported: open listen.html. Original source unchanged.");
    return juce::Result::ok();
}
}
