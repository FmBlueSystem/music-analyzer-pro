// Master AI Analyzer - Coordinates all algorithms

#include "ai_algorithms.h"
#include <iostream>

namespace MusicAnalysis {

// ========================================
// 🚀 MASTER ANALYZER IMPLEMENTATION
// ========================================

AIAnalysisResult AIMetadataAnalyzer::analyzeAudio(const AudioBuffer& audio) {
    initializeAnalyzers();
    return combineResults(audio);
}

void AIMetadataAnalyzer::initializeAnalyzers() {
    keyDetector = std::make_unique<KeyDetector>();
    bpmDetector = std::make_unique<BPMDetector>();
    loudnessAnalyzer = std::make_unique<LoudnessAnalyzer>();
    acousticnessAnalyzer = std::make_unique<AcousticnessAnalyzer>();
    instrumentalnessDetector = std::make_unique<InstrumentalnessDetector>();
    speechinessDetector = std::make_unique<SpeechinessDetector>();
    livenessDetector = std::make_unique<LivenessDetector>();
    energyAnalyzer = std::make_unique<EnergyAnalyzer>();
    danceabilityAnalyzer = std::make_unique<DanceabilityAnalyzer>();
    valenceAnalyzer = std::make_unique<ValenceAnalyzer>();
    modeDetector = std::make_unique<ModeDetector>();
    timeSignatureDetector = std::make_unique<TimeSignatureDetector>();
    characteristicsExtractor = std::make_unique<CharacteristicsExtractor>();
    confidenceCalculator = std::make_unique<ConfidenceCalculator>();
    genreClassifier = std::make_unique<GenreClassifier>();
    moodAnalyzer = std::make_unique<MoodAnalyzer>();
}

AIAnalysisResult AIMetadataAnalyzer::combineResults(const AudioBuffer& audio) {
    AIAnalysisResult result;
    
    try {
        std::cout << "🎵 Starting AI analysis..." << std::endl;
        
        // Core analysis
        result.AI_KEY = keyDetector->detectKey(audio);
        std::cout << "🎹 Key detected: " << result.AI_KEY << std::endl;
        
        result.AI_BPM = bpmDetector->detectBPM(audio);
        std::cout << "🥁 BPM detected: " << result.AI_BPM << std::endl;
        
        result.AI_LOUDNESS = loudnessAnalyzer->calculateLUFS(audio);
        std::cout << "🔊 Loudness: " << result.AI_LOUDNESS << " LUFS" << std::endl;
        
        result.AI_ACOUSTICNESS = acousticnessAnalyzer->calculateAcousticness(audio);
        std::cout << "🎸 Acousticness: " << result.AI_ACOUSTICNESS << std::endl;
        
        result.AI_INSTRUMENTALNESS = instrumentalnessDetector->detectInstrumentalness(audio);
        std::cout << "🎤 Instrumentalness: " << result.AI_INSTRUMENTALNESS << std::endl;
        
        result.AI_SPEECHINESS = speechinessDetector->detectSpeechiness(audio);
        std::cout << "🗣️ Speechiness: " << result.AI_SPEECHINESS << std::endl;
        
        result.AI_LIVENESS = livenessDetector->detectLiveness(audio);
        std::cout << "🎪 Liveness: " << result.AI_LIVENESS << std::endl;
        
        result.AI_ENERGY = energyAnalyzer->calculateEnergy(audio);
        std::cout << "⚡ Energy: " << result.AI_ENERGY << std::endl;
        
        result.AI_DANCEABILITY = danceabilityAnalyzer->calculateDanceability(audio);
        std::cout << "🕺 Danceability: " << result.AI_DANCEABILITY << std::endl;
        
        result.AI_VALENCE = valenceAnalyzer->calculateValence(audio);
        std::cout << "😊 Valence: " << result.AI_VALENCE << std::endl;
        
        result.AI_MODE = modeDetector->detectMode(audio);
        std::cout << "🎼 Mode: " << result.AI_MODE << std::endl;
        
        result.AI_TIME_SIGNATURE = timeSignatureDetector->detectTimeSignature(audio);
        std::cout << "🎵 Time Signature: " << result.AI_TIME_SIGNATURE << "/4" << std::endl;
        
        result.AI_CHARACTERISTICS = characteristicsExtractor->extractCharacteristics(audio);
        std::cout << "🎨 Characteristics: ";
        for (const auto& char_str : result.AI_CHARACTERISTICS) {
            std::cout << char_str << " ";
        }
        std::cout << std::endl;
        
        // Classification analysis
        result.AI_SUBGENRES = genreClassifier->classifySubgenres(audio, result);
        std::cout << "🎭 Subgenres: ";
        for (const auto& genre : result.AI_SUBGENRES) {
            std::cout << genre << " ";
        }
        std::cout << std::endl;
        
        result.AI_ERA = genreClassifier->classifyEra(audio, result);
        std::cout << "📅 Era: " << result.AI_ERA << std::endl;
        
        result.AI_CULTURAL_CONTEXT = genreClassifier->analyzeCulturalContext(audio, result);
        std::cout << "🌍 Cultural Context: " << result.AI_CULTURAL_CONTEXT << std::endl;
        
        result.AI_MOOD = moodAnalyzer->analyzeMood(result);
        std::cout << "😊 Mood: " << result.AI_MOOD << std::endl;
        
        result.AI_OCCASION = moodAnalyzer->analyzeOccasions(result);
        std::cout << "🎉 Occasions: ";
        for (const auto& occasion : result.AI_OCCASION) {
            std::cout << occasion << " ";
        }
        std::cout << std::endl;
        
        // Final confidence calculation
        result.AI_CONFIDENCE = confidenceCalculator->calculateOverallConfidence(audio, result);
        std::cout << "📊 Confidence: " << result.AI_CONFIDENCE << std::endl;
        
        result.AI_ANALYZED = true;
        
        std::cout << "✅ AI analysis completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error during AI analysis: " << e.what() << std::endl;
        result.AI_ANALYZED = false;
        result.AI_CONFIDENCE = 0.0f;
    }
    
    return result;
}

} // namespace MusicAnalysis

// ========================================
// 🧪 C INTERFACE FOR NODE.JS INTEGRATION
// ========================================

extern "C" {
    
    // Create analyzer instance
    MusicAnalysis::AIMetadataAnalyzer* create_ai_analyzer() {
        return new MusicAnalysis::AIMetadataAnalyzer();
    }
    
    // Destroy analyzer instance
    void destroy_ai_analyzer(MusicAnalysis::AIMetadataAnalyzer* analyzer) {
        delete analyzer;
    }
    
    // Analyze audio buffer
    MusicAnalysis::AIAnalysisResult* analyze_audio_buffer(
        MusicAnalysis::AIMetadataAnalyzer* analyzer,
        float* samples,
        int sample_count,
        int sample_rate
    ) {
        try {
            std::vector<float> audioData(samples, samples + sample_count);
            MusicAnalysis::AudioBuffer buffer(audioData, sample_rate, 1);
            
            MusicAnalysis::AIAnalysisResult result = analyzer->analyzeAudio(buffer);
            
            // Allocate result on heap for C interface
            auto* heapResult = new MusicAnalysis::AIAnalysisResult(result);
            return heapResult;
            
        } catch (const std::exception& e) {
            std::cerr << "Error in analyze_audio_buffer: " << e.what() << std::endl;
            return nullptr;
        }
    }
    
    // Get analysis result fields
    float get_ai_acousticness(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_ACOUSTICNESS : 0.0f;
    }
    
    bool get_ai_analyzed(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_ANALYZED : false;
    }
    
    float get_ai_bpm(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_BPM : 0.0f;
    }
    
    const char* get_ai_characteristics_json(MusicAnalysis::AIAnalysisResult* result) {
        if (!result || result->AI_CHARACTERISTICS.empty()) return "[]";
        
        static std::string json;
        json = "[";
        for (size_t i = 0; i < result->AI_CHARACTERISTICS.size(); i++) {
            json += "\"" + result->AI_CHARACTERISTICS[i] + "\"";
            if (i < result->AI_CHARACTERISTICS.size() - 1) json += ",";
        }
        json += "]";
        return json.c_str();
    }
    
    float get_ai_confidence(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_CONFIDENCE : 0.0f;
    }
    
    const char* get_ai_cultural_context(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_CULTURAL_CONTEXT.c_str() : "";
    }
    
    float get_ai_danceability(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_DANCEABILITY : 0.0f;
    }
    
    float get_ai_energy(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_ENERGY : 0.0f;
    }
    
    const char* get_ai_era(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_ERA.c_str() : "";
    }
    
    float get_ai_instrumentalness(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_INSTRUMENTALNESS : 0.0f;
    }
    
    const char* get_ai_key(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_KEY.c_str() : "";
    }
    
    float get_ai_liveness(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_LIVENESS : 0.0f;
    }
    
    float get_ai_loudness(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_LOUDNESS : 0.0f;
    }
    
    const char* get_ai_mode(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_MODE.c_str() : "";
    }
    
    const char* get_ai_mood(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_MOOD.c_str() : "";
    }
    
    const char* get_ai_occasion_json(MusicAnalysis::AIAnalysisResult* result) {
        if (!result || result->AI_OCCASION.empty()) return "[]";
        
        static std::string json;
        json = "[";
        for (size_t i = 0; i < result->AI_OCCASION.size(); i++) {
            json += "\"" + result->AI_OCCASION[i] + "\"";
            if (i < result->AI_OCCASION.size() - 1) json += ",";
        }
        json += "]";
        return json.c_str();
    }
    
    float get_ai_speechiness(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_SPEECHINESS : 0.0f;
    }
    
    const char* get_ai_subgenres_json(MusicAnalysis::AIAnalysisResult* result) {
        if (!result || result->AI_SUBGENRES.empty()) return "[]";
        
        static std::string json;
        json = "[";
        for (size_t i = 0; i < result->AI_SUBGENRES.size(); i++) {
            json += "\"" + result->AI_SUBGENRES[i] + "\"";
            if (i < result->AI_SUBGENRES.size() - 1) json += ",";
        }
        json += "]";
        return json.c_str();
    }
    
    int get_ai_time_signature(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_TIME_SIGNATURE : 4;
    }
    
    float get_ai_valence(MusicAnalysis::AIAnalysisResult* result) {
        return result ? result->AI_VALENCE : 0.0f;
    }
    
    // Cleanup result
    void destroy_ai_result(MusicAnalysis::AIAnalysisResult* result) {
        delete result;
    }
}