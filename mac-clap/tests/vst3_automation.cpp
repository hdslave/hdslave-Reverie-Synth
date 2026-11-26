#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstevents.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace
{
using namespace Steinberg;
using namespace Steinberg::Vst;

#define CHECK(condition) do { if (!(condition)) { std::fprintf (stderr, "line %d: %s\n", __LINE__, #condition); std::exit (1); } } while (false)

template <typename Base, const char (&Id)[16]>
struct TestObject : Base
{
    uint32 PLUGIN_API addRef() override { return ++references; }
    uint32 PLUGIN_API release() override { return --references; }
    tresult PLUGIN_API queryInterface (const TUID id, void** object) override
    {
        *object = nullptr;
        if (std::memcmp (id, Id, 16) != 0 && std::memcmp (id, FUnknown_iid, 16) != 0)
            return kNoInterface;
        *object = static_cast<Base*> (this);
        addRef();
        return kResultOk;
    }

private:
    uint32 references = 1;
};

struct Host : TestObject<IHostApplication, IHostApplication_iid>
{
    tresult PLUGIN_API getName (String128 name) override
    {
        const char16_t label[] = u"Reverie test host";
        std::copy (std::begin (label), std::end (label), name);
        return kResultOk;
    }
    tresult PLUGIN_API createInstance (TUID, TUID, void** object) override { *object = nullptr; return kNoInterface; }
};

struct Points : TestObject<IParamValueQueue, IParamValueQueue_iid>
{
    struct Point { int32 offset; double value; };
    std::array<Point, 3> list {};
    int32 count = 0;
    ParamID PLUGIN_API getParameterId() override { return 79; } // Master Volume
    int32 PLUGIN_API getPointCount() override { return count; }
    tresult PLUGIN_API getPoint (int32 i, int32& offset, ParamValue& value) override
    {
        if (i < 0 || i >= count) return kResultFalse;
        offset = list[i].offset;
        value = list[i].value;
        return kResultOk;
    }
    tresult PLUGIN_API addPoint (int32, ParamValue, int32&) override { return kResultFalse; }
};

struct Changes : TestObject<IParameterChanges, IParameterChanges_iid>
{
    Points points;
    int32 PLUGIN_API getParameterCount() override { return 1; }
    IParamValueQueue* PLUGIN_API getParameterData (int32 i) override { return i == 0 ? &points : nullptr; }
    IParamValueQueue* PLUGIN_API addParameterData (const ParamID&, int32&) override { return nullptr; }
};

struct Note : TestObject<IEventList, IEventList_iid>
{
    int32 PLUGIN_API getEventCount() override { return 1; }
    tresult PLUGIN_API getEvent (int32 i, Event& event) override
    {
        if (i != 0) return kResultFalse;
        event = {};
        event.sampleOffset = 32;
        event.type = Event::kNoteOnEvent;
        event.noteOn.pitch = 36;
        event.noteOn.noteId = 100;
        event.noteOn.velocity = 0.8f;
        return kResultOk;
    }
    tresult PLUGIN_API addEvent (Event&) override { return kResultFalse; }
};

constexpr int frames = 512, transition = 128;
using Stereo = std::array<std::array<float, frames>, 2>;

Stereo render (IPluginFactory& factory, bool split)
{
    PClassInfo info {};
    CHECK (factory.getClassInfo (0, &info) == kResultOk);
    IComponent* component = nullptr;
    CHECK (factory.createInstance (info.cid, IComponent_iid, reinterpret_cast<void**> (&component)) == kResultOk && component);
    Host host;
    CHECK (component->initialize (&host) == kResultOk);
    IAudioProcessor* processor = nullptr;
    CHECK (component->queryInterface (IAudioProcessor_iid, reinterpret_cast<void**> (&processor)) == kResultOk && processor);
    ProcessSetup setup { kRealtime, kSample32, frames, 48000.0 };
    CHECK (processor->setupProcessing (setup) == kResultOk);
    CHECK (component->activateBus (kAudio, kOutput, 0, true) == kResultOk);
    CHECK (component->activateBus (kEvent, kInput, 0, true) == kResultOk);
    CHECK (component->setActive (true) == kResultOk && processor->setProcessing (true) == kResultOk);

    Stereo audio {};
    Note note;
    Changes changes;
    auto process = [&] (int start, int count, IEventList* events) {
        float* channels[] { audio[0].data() + start, audio[1].data() + start };
        AudioBusBuffers bus {};
        bus.numChannels = 2;
        bus.channelBuffers32 = channels;
        ProcessData data {};
        data.processMode = kRealtime;
        data.symbolicSampleSize = kSample32;
        data.numSamples = count;
        data.numOutputs = 1;
        data.outputs = &bus;
        data.inputEvents = events;
        data.inputParameterChanges = &changes;
        CHECK (processor->process (data) == kResultOk);
    };
    changes.points.list = {{{ 0, 0.0 }, { transition - 1, 0.0 }, { transition, 0.65 }}};
    changes.points.count = split ? 1 : 3;
    process (0, split ? transition : frames, &note);
    if (split)
    {
        changes.points.list[0] = { 0, 0.65 };
        changes.points.count = 1;
        process (transition, frames - transition, nullptr);
    }
    CHECK (processor->setProcessing (false) == kResultOk && component->setActive (false) == kResultOk);
    processor->release();
    CHECK (component->terminate() == kResultOk);
    component->release();
    return audio;
}
}

int main (int argc, char** argv)
{
    CHECK (argc == 2);
#ifdef _WIN32
    const auto library = LoadLibraryA (argv[1]);
    CHECK (library);
    const auto symbol = [library] (const char* name) { return GetProcAddress (library, name); };
    const auto entry = reinterpret_cast<bool (PLUGIN_API*)()> (symbol ("InitDll"));
    CHECK (entry && entry());
#else
    const auto url = CFURLCreateFromFileSystemRepresentation (nullptr, reinterpret_cast<const UInt8*> (argv[1]), std::strlen (argv[1]), true);
    CHECK (url);
    const auto library = CFBundleCreate (nullptr, url);
    CFRelease (url);
    CHECK (library && CFBundleLoadExecutable (library));
    const auto symbol = [library] (const char* name) {
        const auto key = CFStringCreateWithCString (nullptr, name, kCFStringEncodingUTF8);
        const auto address = CFBundleGetFunctionPointerForName (library, key);
        CFRelease (key);
        return address;
    };
    const auto entry = reinterpret_cast<bool (*)(CFBundleRef)> (symbol ("bundleEntry"));
    CHECK (entry && entry (library));
#endif
    const auto getFactory = reinterpret_cast<IPluginFactory* (PLUGIN_API*)()> (symbol ("GetPluginFactory"));
    CHECK (getFactory);
    auto* factory = getFactory();
    CHECK (factory);
    const auto together = render (*factory, false);
    const auto split = render (*factory, true);
    CHECK (together == split);
    CHECK (std::all_of (together[0].begin(), together[0].begin() + transition, [] (float x) { return x == 0.0f; }));
    CHECK (std::any_of (together[0].begin() + transition, together[0].end(), [] (float x) { return std::abs (x) > 1.0e-6f; }));
    factory->release();
    std::puts ("PASS: VST3 automation audio matches at both block sizes");
}
