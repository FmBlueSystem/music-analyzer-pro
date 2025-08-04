#include <nan.h>
#include "../../src-tauri/cpp/metadata_handler.h"
#include <memory>

using namespace MusicPlayerPro;

class MetadataWorker : public Nan::AsyncWorker {
public:
    MetadataWorker(Nan::Callback *callback, const std::string& filePath)
        : Nan::AsyncWorker(callback), filePath(filePath) {}
    
    void Execute() override {
        MetadataHandler handler;
        if (!handler.readMetadata(filePath, metadata)) {
            SetErrorMessage(handler.getLastError().c_str());
        }
    }
    
    void HandleOKCallback() override {
        Nan::HandleScope scope;
        
        v8::Local<v8::Object> result = Nan::New<v8::Object>();
        
        // Convert LLMMetadata to JavaScript object
        if (!metadata.energy.empty()) {
            Nan::Set(result, Nan::New("energy").ToLocalChecked(), Nan::New(metadata.energy).ToLocalChecked());
        }
        if (!metadata.era.empty()) {
            Nan::Set(result, Nan::New("era").ToLocalChecked(), Nan::New(metadata.era).ToLocalChecked());
        }
        if (!metadata.subgenre.empty()) {
            Nan::Set(result, Nan::New("subgenre").ToLocalChecked(), Nan::New(metadata.subgenre).ToLocalChecked());
        }
        if (!metadata.danceability.empty()) {
            Nan::Set(result, Nan::New("danceability").ToLocalChecked(), Nan::New(metadata.danceability).ToLocalChecked());
        }
        if (!metadata.valence.empty()) {
            Nan::Set(result, Nan::New("valence").ToLocalChecked(), Nan::New(metadata.valence).ToLocalChecked());
        }
        if (!metadata.occasion.empty()) {
            Nan::Set(result, Nan::New("occasion").ToLocalChecked(), Nan::New(metadata.occasion).ToLocalChecked());
        }
        if (!metadata.characteristics.empty()) {
            Nan::Set(result, Nan::New("characteristics").ToLocalChecked(), Nan::New(metadata.characteristics).ToLocalChecked());
        }
        if (!metadata.mood.empty()) {
            Nan::Set(result, Nan::New("mood").ToLocalChecked(), Nan::New(metadata.mood).ToLocalChecked());
        }
        if (!metadata.crowd_response.empty()) {
            Nan::Set(result, Nan::New("crowd_response").ToLocalChecked(), Nan::New(metadata.crowd_response).ToLocalChecked());
        }
        if (!metadata.drop_time.empty()) {
            Nan::Set(result, Nan::New("drop_time").ToLocalChecked(), Nan::New(metadata.drop_time).ToLocalChecked());
        }
        if (!metadata.tempo_stability.empty()) {
            Nan::Set(result, Nan::New("tempo_stability").ToLocalChecked(), Nan::New(metadata.tempo_stability).ToLocalChecked());
        }
        if (!metadata.production_quality.empty()) {
            Nan::Set(result, Nan::New("production_quality").ToLocalChecked(), Nan::New(metadata.production_quality).ToLocalChecked());
        }
        if (!metadata.mastering_loudness.empty()) {
            Nan::Set(result, Nan::New("mastering_loudness").ToLocalChecked(), Nan::New(metadata.mastering_loudness).ToLocalChecked());
        }
        if (!metadata.dynamic_range.empty()) {
            Nan::Set(result, Nan::New("dynamic_range").ToLocalChecked(), Nan::New(metadata.dynamic_range).ToLocalChecked());
        }
        
        // Handle custom tags array
        if (!metadata.custom_tags.empty()) {
            v8::Local<v8::Array> tags = Nan::New<v8::Array>(metadata.custom_tags.size());
            for (size_t i = 0; i < metadata.custom_tags.size(); i++) {
                Nan::Set(tags, i, Nan::New(metadata.custom_tags[i]).ToLocalChecked());
            }
            Nan::Set(result, Nan::New("custom_tags").ToLocalChecked(), tags);
        }
        
        v8::Local<v8::Value> argv[] = { Nan::Null(), result };
        callback->Call(2, argv, async_resource);
    }
    
private:
    std::string filePath;
    LLMMetadata metadata;
};

class MetadataWriteWorker : public Nan::AsyncWorker {
public:
    MetadataWriteWorker(Nan::Callback *callback, const std::string& filePath, const LLMMetadata& metadata)
        : Nan::AsyncWorker(callback), filePath(filePath), metadata(metadata) {}
    
    void Execute() override {
        MetadataHandler handler;
        if (!handler.writeMetadata(filePath, metadata)) {
            SetErrorMessage(handler.getLastError().c_str());
        }
    }
    
    void HandleOKCallback() override {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = { Nan::Null(), Nan::New(true) };
        callback->Call(2, argv, async_resource);
    }
    
private:
    std::string filePath;
    LLMMetadata metadata;
};

// Read metadata async
NAN_METHOD(ReadMetadata) {
    if (info.Length() < 2) {
        return Nan::ThrowError("Expected 2 arguments: filePath, callback");
    }
    
    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("First argument must be a string");
    }
    
    if (!info[1]->IsFunction()) {
        return Nan::ThrowTypeError("Second argument must be a function");
    }
    
    std::string filePath(*v8::String::Utf8Value(info.GetIsolate(), info[0]));
    Nan::Callback *callback = new Nan::Callback(info[1].As<v8::Function>());
    
    Nan::AsyncQueueWorker(new MetadataWorker(callback, filePath));
}

// Write metadata async
NAN_METHOD(WriteMetadata) {
    if (info.Length() < 3) {
        return Nan::ThrowError("Expected 3 arguments: filePath, metadata, callback");
    }
    
    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("First argument must be a string");
    }
    
    if (!info[1]->IsObject()) {
        return Nan::ThrowTypeError("Second argument must be an object");
    }
    
    if (!info[2]->IsFunction()) {
        return Nan::ThrowTypeError("Third argument must be a function");
    }
    
    std::string filePath(*v8::String::Utf8Value(info.GetIsolate(), info[0]));
    v8::Local<v8::Object> jsMetadata = info[1].As<v8::Object>();
    Nan::Callback *callback = new Nan::Callback(info[2].As<v8::Function>());
    
    // Convert JavaScript object to LLMMetadata
    LLMMetadata metadata;
    
    auto getString = [&](const char* key) -> std::string {
        v8::Local<v8::String> keyStr = Nan::New(key).ToLocalChecked();
        if (Nan::Has(jsMetadata, keyStr).FromJust()) {
            v8::Local<v8::Value> val = Nan::Get(jsMetadata, keyStr).ToLocalChecked();
            if (val->IsString()) {
                return std::string(*v8::String::Utf8Value(info.GetIsolate(), val));
            }
        }
        return "";
    };
    
    metadata.energy = getString("energy");
    metadata.era = getString("era");
    metadata.subgenre = getString("subgenre");
    metadata.danceability = getString("danceability");
    metadata.valence = getString("valence");
    metadata.occasion = getString("occasion");
    metadata.characteristics = getString("characteristics");
    metadata.mood = getString("mood");
    metadata.crowd_response = getString("crowd_response");
    metadata.drop_time = getString("drop_time");
    metadata.tempo_stability = getString("tempo_stability");
    metadata.production_quality = getString("production_quality");
    metadata.mastering_loudness = getString("mastering_loudness");
    metadata.dynamic_range = getString("dynamic_range");
    
    Nan::AsyncQueueWorker(new MetadataWriteWorker(callback, filePath, metadata));
}

// Check if custom fields can be written
NAN_METHOD(CanWriteCustomFields) {
    if (info.Length() < 1) {
        return Nan::ThrowError("Expected 1 argument: filePath");
    }
    
    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("Argument must be a string");
    }
    
    std::string filePath(*v8::String::Utf8Value(info.GetIsolate(), info[0]));
    MetadataHandler handler;
    bool canWrite = handler.canWriteCustomFields(filePath);
    
    info.GetReturnValue().Set(Nan::New(canWrite));
}

NAN_MODULE_INIT(Init) {
    Nan::Set(target, Nan::New("readMetadata").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(ReadMetadata)).ToLocalChecked());
    Nan::Set(target, Nan::New("writeMetadata").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(WriteMetadata)).ToLocalChecked());
    Nan::Set(target, Nan::New("canWriteCustomFields").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(CanWriteCustomFields)).ToLocalChecked());
}

NODE_MODULE(metadata_addon, Init)