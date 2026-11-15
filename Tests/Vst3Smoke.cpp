#define NOMINMAX
#include <windows.h>
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/gui/iplugview.h>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace Steinberg;
using namespace Steinberg::Vst;
int main(int argc, char** argv) {
    auto require = [](bool ok, const char* name) {
        std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl;
        if (!ok) throw std::runtime_error(name);
    };
    try {
        require(argc == 2, "VST3 binary supplied");
        auto module = LoadLibraryA(argv[1]); require(module != nullptr, "Load Windows x64 VST3 DLL");
        auto init = reinterpret_cast<bool(*)()>(GetProcAddress(module, "InitDll"));
        auto exit = reinterpret_cast<bool(*)()>(GetProcAddress(module, "ExitDll"));
        require(init && exit && init(), "VST3 module initialization");
        auto getFactory = reinterpret_cast<IPluginFactory*(PLUGIN_API*)()>(GetProcAddress(module, "GetPluginFactory"));
        require(getFactory != nullptr, "VST3 factory export");
        auto* factory = getFactory(); require(factory != nullptr, "Plugin factory instantiated");
        IComponent* component = nullptr;
        for (int32 index = 0; index < factory->countClasses(); ++index) {
            PClassInfo info{}; factory->getClassInfo(index, &info);
            if (std::strcmp(info.category, kVstAudioEffectClass) == 0)
                factory->createInstance(info.cid, IComponent::iid, reinterpret_cast<void**>(&component));
        }
        require(component && component->initialize(nullptr) == kResultOk, "VST3 component instantiated and initialized");
        require(component->getBusCount(kAudio, kInput) == 2 && component->getBusCount(kAudio, kOutput) == 1,
            "Main input, external sidechain and main output exported");
        BusInfo auxInfo{};
        require(component->getBusInfo(kAudio, kInput, 1, auxInfo) == kResultOk
            && auxInfo.busType == kAux && (auxInfo.flags & BusInfo::kDefaultActive),
            "Actual VST3 exports an auxiliary bus enabled by default");
        IAudioProcessor* audio = nullptr;
        require(component->queryInterface(IAudioProcessor::iid, reinterpret_cast<void**>(&audio)) == kResultOk, "Audio processor interface");
        std::array<SpeakerArrangement, 2> arrangements{SpeakerArr::kStereo, SpeakerArr::kStereo};
        auto stereo = SpeakerArr::kStereo;
        require(audio->setBusArrangements(arrangements.data(), 2, &stereo, 1) == kResultOk, "Host stereo bus negotiation");
        component->activateBus(kAudio, kInput, 0, true);
        component->activateBus(kAudio, kInput, 1, true);
        component->activateBus(kAudio, kOutput, 0, true);
        ProcessSetup setup{kRealtime, kSample32, 512, 48000};
        require(audio->setupProcessing(setup) == kResultOk, "Host prepares 48 kHz processing");
        require(component->setActive(true) == kResultOk, "Activate component");
        audio->setProcessing(true);
        std::array<float, 512> inL{}, inR{}, scL{}, scR{}, outL{}, outR{};
        float* main[]{inL.data(), inR.data()}; float* side[]{scL.data(), scR.data()}; float* output[]{outL.data(), outR.data()};
        std::array<AudioBusBuffers, 2> inputs;
        inputs[0].numChannels = inputs[1].numChannels = 2;
        inputs[0].channelBuffers32 = main; inputs[1].channelBuffers32 = side;
        AudioBusBuffers outputs; outputs.numChannels = 2; outputs.channelBuffers32 = output;
        ProcessData data; data.processMode = kRealtime; data.symbolicSampleSize = kSample32;
        data.numSamples = 512; data.numInputs = 2; data.numOutputs = 1; data.inputs = inputs.data(); data.outputs = &outputs;
        double dryPower = 0;
        std::array<double, 3> wetPower{};
        for (int block = 0; block < 400; ++block) {
            for (int n = 0; n < 512; ++n) {
                const auto v = .3f * static_cast<float>(std::sin(6.283185307179586 * 1000 * (block * 512 + n) / 48000));
                inL[n] = inR[n] = v;
                scL[n] = (block >= 100 && block < 200) || block >= 300 ? v : 0;
                scR[n] = block >= 300 ? -v : block >= 200 ? v : 0;
            }
            if (audio->process(data) != kResultOk) require(false, "VST3 process block");
            for (int n = 0; n < 512; ++n) {
                if (!std::isfinite(outL[n]) || !std::isfinite(outR[n])) require(false, "Output is finite");
                if (block >= 80 && block < 100) dryPower += outL[n] * outL[n];
                if (block >= 100 && block % 100 >= 80)
                    wetPower[static_cast<size_t>(block / 100 - 1)] += outL[n] * outL[n];
            }
        }
        require(dryPower > 100 && wetPower[0] < dryPower * .2, "VST3 left-only SC drives linked reduction");
        require(wetPower[1] < dryPower * .2, "VST3 right-only SC drives linked reduction");
        require(wetPower[2] < dryPower * .2, "VST3 anti-phase SC does not cancel the detector");
        require(audio->getLatencySamples() == 0, "VST3 latency is zero");
        audio->setProcessing(false); component->setActive(false); audio->release();
        component->terminate(); component->release(); factory->release();
        require(exit(), "Module shutdown"); FreeLibrary(module);
        std::cout << "Windows VST3 binary smoke test passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
