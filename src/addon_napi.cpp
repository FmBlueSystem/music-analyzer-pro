#include <napi.h>
#include "ai_algorithms.h"
#include <memory>
#include <fstream>
#include <string>
#include <vector>

using namespace MusicAnalysis;

// Simplified C++ addon - only AI analysis functionality
// Metadata reading/writing handled by JavaScript libraries





// AsyncWorker for audio analysis
class AudioAnalysisWorker : public Napi::AsyncWorker {
public:
    AudioAnalysisWorker(Napi::Function& callback, const std::string& filePath, const std::vector<std::string>& algorithms)
        : Napi::AsyncWorker(callback), filePath(filePath), algorithms(algorithms) {}
    
    void Execute() override {
        try {
            // REMOVED: Test audio buffer generation
            // Real audio loading must be implemented
            throw std::runtime_error("Real audio file loading not implemented. Cannot use test data.");
            
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }
    
    void OnOK() override {
        Napi::HandleScope scope(Env());
        
        Napi::Object jsResult = Napi::Object::New(Env());
        
        // Convert C++ AIAnalysisResult to JavaScript object
        jsResult.Set("AI_ACOUSTICNESS", Napi::Number::New(Env(), result.AI_ACOUSTICNESS));
        jsResult.Set("AI_ANALYZED", Napi::Boolean::New(Env(), result.AI_ANALYZED));
        jsResult.Set("AI_BPM", Napi::Number::New(Env(), result.AI_BPM));
        jsResult.Set("AI_CONFIDENCE", Napi::Number::New(Env(), result.AI_CONFIDENCE));
        jsResult.Set("AI_CULTURAL_CONTEXT", Napi::String::New(Env(), result.AI_CULTURAL_CONTEXT));
        jsResult.Set("AI_DANCEABILITY", Napi::Number::New(Env(), result.AI_DANCEABILITY));
        jsResult.Set("AI_ENERGY", Napi::Number::New(Env(), result.AI_ENERGY));
        jsResult.Set("AI_ERA", Napi::String::New(Env(), result.AI_ERA));
        jsResult.Set("AI_INSTRUMENTALNESS", Napi::Number::New(Env(), result.AI_INSTRUMENTALNESS));
        jsResult.Set("AI_KEY", Napi::String::New(Env(), result.AI_KEY));
        jsResult.Set("AI_LIVENESS", Napi::Number::New(Env(), result.AI_LIVENESS));
        jsResult.Set("AI_LOUDNESS", Napi::Number::New(Env(), result.AI_LOUDNESS));
        jsResult.Set("AI_MODE", Napi::String::New(Env(), result.AI_MODE));
        jsResult.Set("AI_MOOD", Napi::String::New(Env(), result.AI_MOOD));
        jsResult.Set("AI_SPEECHINESS", Napi::Number::New(Env(), result.AI_SPEECHINESS));
        jsResult.Set("AI_TIME_SIGNATURE", Napi::Number::New(Env(), result.AI_TIME_SIGNATURE));
        jsResult.Set("AI_VALENCE", Napi::Number::New(Env(), result.AI_VALENCE));
        
        // Convert arrays
        Napi::Array characteristics = Napi::Array::New(Env());
        for (size_t i = 0; i < result.AI_CHARACTERISTICS.size(); i++) {
            characteristics[i] = Napi::String::New(Env(), result.AI_CHARACTERISTICS[i]);
        }
        jsResult.Set("AI_CHARACTERISTICS", characteristics);
        
        Napi::Array occasion = Napi::Array::New(Env());
        for (size_t i = 0; i < result.AI_OCCASION.size(); i++) {
            occasion[i] = Napi::String::New(Env(), result.AI_OCCASION[i]);
        }
        jsResult.Set("AI_OCCASION", occasion);
        
        Napi::Array subgenres = Napi::Array::New(Env());
        for (size_t i = 0; i < result.AI_SUBGENRES.size(); i++) {
            subgenres[i] = Napi::String::New(Env(), result.AI_SUBGENRES[i]);
        }
        jsResult.Set("AI_SUBGENRES", subgenres);
        
        Callback().Call({Env().Null(), jsResult});
    }
    
private:
    std::string filePath;
    std::vector<std::string> algorithms;
    AIAnalysisResult result;
};

// Analyze audio with C++ algorithms
Napi::Value AnalyzeAudio(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 3) {
        Napi::TypeError::New(env, "Expected 3 arguments: filePath, algorithms, callback")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    
    if (!info[0].IsString() || !info[1].IsArray() || !info[2].IsFunction()) {
        Napi::TypeError::New(env, "Arguments must be: string, array, function")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    
    std::string filePath = info[0].As<Napi::String>().Utf8Value();
    Napi::Array algorithmsArray = info[1].As<Napi::Array>();
    Napi::Function callback = info[2].As<Napi::Function>();
    
    // Convert JavaScript array to C++ vector
    std::vector<std::string> algorithms;
    for (uint32_t i = 0; i < algorithmsArray.Length(); i++) {
        algorithms.push_back(algorithmsArray.Get(i).As<Napi::String>().Utf8Value());
    }
    
    AudioAnalysisWorker* worker = new AudioAnalysisWorker(callback, filePath, algorithms);
    worker->Queue();
    
    return env.Undefined();
}

// Initialize module - only AI analysis functionality
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "analyzeAudio"), 
                Napi::Function::New(env, AnalyzeAudio));
    
    return exports;
}

NODE_API_MODULE(metadata_addon, Init)