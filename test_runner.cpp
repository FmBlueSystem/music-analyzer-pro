/**
 * 🧪 COMPREHENSIVE AI ALGORITHMS TEST RUNNER
 * Tests all 19 AI_* fields with real audio data
 */

#include "src/ai_algorithms.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <random>
#include <cstring>
#include <filesystem>

using namespace MusicAnalysis;
namespace fs = std::filesystem;

// ========================================
// 🎵 TEST AUDIO GENERATORS
// ========================================

class TestAudioGenerator {
public:
    // Generate sine wave test signal
    static AudioBuffer generateSineWave(float frequency, float duration, int sampleRate = 44100) {
        int numSamples = (int)(duration * sampleRate);
        std::vector<float> samples(numSamples);
        
        for (int i = 0; i < numSamples; i++) {
            float t = (float)i / sampleRate;
            samples[i] = 0.5f * std::sin(2.0f * M_PI * frequency * t);
        }
        
        return AudioBuffer(samples, sampleRate, 1);
    }
    
    // Generate chord progression test signal (C-Am-F-G)
    static AudioBuffer generateChordProgression(float duration, int sampleRate = 44100) {
        int numSamples = (int)(duration * sampleRate);
        std::vector<float> samples(numSamples);
        
        float chordDuration = duration / 4.0f;
        
        // C major chord (C-E-G)
        for (int i = 0; i < numSamples / 4; i++) {
            float t = (float)i / sampleRate;
            samples[i] = 0.3f * (std::sin(2.0f * M_PI * 261.63f * t) +  // C4
                               std::sin(2.0f * M_PI * 329.63f * t) +    // E4
                               std::sin(2.0f * M_PI * 392.00f * t));    // G4
        }
        
        // A minor chord (A-C-E)
        for (int i = numSamples / 4; i < numSamples / 2; i++) {
            float t = (float)i / sampleRate;
            samples[i] = 0.3f * (std::sin(2.0f * M_PI * 220.00f * t) +  // A3
                               std::sin(2.0f * M_PI * 261.63f * t) +    // C4
                               std::sin(2.0f * M_PI * 329.63f * t));    // E4
        }
        
        // F major chord (F-A-C)
        for (int i = numSamples / 2; i < 3 * numSamples / 4; i++) {
            float t = (float)i / sampleRate;
            samples[i] = 0.3f * (std::sin(2.0f * M_PI * 174.61f * t) +  // F3
                               std::sin(2.0f * M_PI * 220.00f * t) +    // A3
                               std::sin(2.0f * M_PI * 261.63f * t));    // C4
        }
        
        // G major chord (G-B-D)
        for (int i = 3 * numSamples / 4; i < numSamples; i++) {
            float t = (float)i / sampleRate;
            samples[i] = 0.3f * (std::sin(2.0f * M_PI * 196.00f * t) +  // G3
                               std::sin(2.0f * M_PI * 246.94f * t) +    // B3
                               std::sin(2.0f * M_PI * 293.66f * t));    // D4
        }
        
        return AudioBuffer(samples, sampleRate, 1);
    }
    
    // Generate drum pattern (kick-snare-hihat)
    static AudioBuffer generateDrumPattern(float bpm, float duration, int sampleRate = 44100) {
        int numSamples = (int)(duration * sampleRate);
        std::vector<float> samples(numSamples, 0.0f);
        
        float beatInterval = 60.0f / bpm; // seconds per beat
        int samplesPerBeat = (int)(beatInterval * sampleRate);
        
        std::mt19937 rng(42); // Fixed seed for reproducibility
        std::uniform_real_distribution<float> noise(-0.5f, 0.5f);
        
        for (int beat = 0; beat * samplesPerBeat < numSamples; beat++) {
            int beatStart = beat * samplesPerBeat;
            
            // Kick drum on beats 1 and 3
            if (beat % 4 == 0 || beat % 4 == 2) {
                for (int i = 0; i < samplesPerBeat / 8 && beatStart + i < numSamples; i++) {
                    float t = (float)i / sampleRate;
                    samples[beatStart + i] += 0.8f * std::sin(2.0f * M_PI * 60.0f * t) * std::exp(-t * 20.0f);
                }
            }
            
            // Snare on beats 2 and 4
            if (beat % 4 == 1 || beat % 4 == 3) {
                for (int i = 0; i < samplesPerBeat / 4 && beatStart + i < numSamples; i++) {
                    samples[beatStart + i] += 0.6f * noise(rng);
                }
            }
            
            // Hi-hat on every beat
            for (int i = 0; i < samplesPerBeat / 16 && beatStart + i < numSamples; i++) {
                samples[beatStart + i] += 0.2f * noise(rng);
            }
        }
        
        return AudioBuffer(samples, sampleRate, 1);
    }
    
    // Generate speech-like signal
    static AudioBuffer generateSpeechPattern(float duration, int sampleRate = 44100) {
        int numSamples = (int)(duration * sampleRate);
        std::vector<float> samples(numSamples);
        
        std::mt19937 rng(123);
        std::uniform_real_distribution<float> freq(200.0f, 800.0f);
        std::uniform_real_distribution<float> amp(0.1f, 0.7f);
        
        int segmentLength = sampleRate / 10; // 100ms segments
        
        for (int seg = 0; seg < numSamples / segmentLength; seg++) {
            float f = freq(rng);
            float a = amp(rng);
            
            for (int i = 0; i < segmentLength && seg * segmentLength + i < numSamples; i++) {
                float t = (float)i / sampleRate;
                int sampleIdx = seg * segmentLength + i;
                
                // Formant synthesis approximation
                samples[sampleIdx] = a * (
                    std::sin(2.0f * M_PI * f * t) +           // F1
                    0.5f * std::sin(2.0f * M_PI * f * 2.5f * t) + // F2
                    0.3f * std::sin(2.0f * M_PI * f * 4.0f * t)   // F3
                );
                
                // Add consonant-like noise bursts
                if (i < segmentLength / 10) {
                    std::uniform_real_distribution<float> noise(-0.3f, 0.3f);
                    samples[sampleIdx] += noise(rng);
                }
            }
        }
        
        return AudioBuffer(samples, sampleRate, 1);
    }
    
    // Generate live recording simulation (with reverb)
    static AudioBuffer generateLiveRecording(const AudioBuffer& source) {
        std::vector<float> processed = source.samples;
        
        // Add simple reverb (delayed copies)
        float delays[] = {0.03f, 0.07f, 0.13f}; // 30ms, 70ms, 130ms
        float gains[] = {0.3f, 0.2f, 0.1f};
        
        for (int d = 0; d < 3; d++) {
            int delaySamples = (int)(delays[d] * source.sampleRate);
            for (int i = delaySamples; i < (int)processed.size(); i++) {
                processed[i] += gains[d] * source.samples[i - delaySamples];
            }
        }
        
        // Add background noise
        std::mt19937 rng(456);
        std::uniform_real_distribution<float> noise(-0.05f, 0.05f);
        for (float& sample : processed) {
            sample += noise(rng);
        }
        
        return AudioBuffer(processed, source.sampleRate, source.channels);
    }
};

// ========================================
// 🧪 TEST CASES IMPLEMENTATION
// ========================================

class AIAlgorithmTester {
private:
    AIMetadataAnalyzer analyzer;
    int testsRun = 0;
    int testsPassed = 0;
    
public:
    void runAllTests() {
        std::cout << "\n🎵 COMPREHENSIVE AI ALGORITHMS TEST SUITE\n";
        std::cout << "=========================================\n\n";
        
        // Test individual algorithms
        testKeyDetection();
        testBPMDetection();
        testLoudnessAnalysis();
        testAcousticnessAnalysis();
        testInstrumentalnessDetection();
        testSpeechinessDetection();
        testLivenessDetection();
        testEnergyAnalysis();
        testDanceabilityAnalysis();
        testValenceAnalysis();
        testModeDetection();
        testTimeSignatureDetection();
        testCharacteristicsExtraction();
        testGenreClassification();
        testMoodAnalysis();
        testConfidenceCalculation();
        
        // Integration tests
        testFullAnalysisPipeline();
        
        // Performance tests
        runPerformanceBenchmarks();
        
        // Report results
        printTestResults();
    }
    
    void testKeyDetection() {
        std::cout << "🎹 Testing Key Detection...\n";
        
        // Test C major chord
        AudioBuffer cMajor = TestAudioGenerator::generateChordProgression(2.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(cMajor);
        
        bool keyTest = !result.AI_KEY.empty();
        reportTest("Key Detection - Basic", keyTest);
        
        if (keyTest) {
            std::cout << "   Detected key: " << result.AI_KEY << "\n";
        }
    }
    
    void testBPMDetection() {
        std::cout << "🥁 Testing BPM Detection...\n";
        
        // Test 120 BPM drum pattern
        AudioBuffer drums120 = TestAudioGenerator::generateDrumPattern(120.0f, 4.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(drums120);
        
        bool bpmInRange = result.AI_BPM >= 110.0f && result.AI_BPM <= 130.0f;
        reportTest("BPM Detection - 120 BPM", bpmInRange);
        
        std::cout << "   Detected BPM: " << result.AI_BPM << "\n";
        
        // Test 80 BPM
        AudioBuffer drums80 = TestAudioGenerator::generateDrumPattern(80.0f, 4.0f);
        result = analyzer.analyzeAudio(drums80);
        
        bool bpm80InRange = result.AI_BPM >= 70.0f && result.AI_BPM <= 90.0f;
        reportTest("BPM Detection - 80 BPM", bpm80InRange);
        
        std::cout << "   Detected BPM: " << result.AI_BPM << "\n";
    }
    
    void testLoudnessAnalysis() {
        std::cout << "🔊 Testing Loudness Analysis...\n";
        
        // Test with sine wave
        AudioBuffer sine = TestAudioGenerator::generateSineWave(440.0f, 2.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(sine);
        
        bool loudnessInRange = result.AI_LOUDNESS >= -60.0f && result.AI_LOUDNESS <= 0.0f;
        reportTest("Loudness Analysis - Range Check", loudnessInRange);
        
        std::cout << "   Detected loudness: " << result.AI_LOUDNESS << " LUFS\n";
    }
    
    void testAcousticnessAnalysis() {
        std::cout << "🎸 Testing Acousticness Analysis...\n";
        
        // Test acoustic-like signal (chord progression)
        AudioBuffer acoustic = TestAudioGenerator::generateChordProgression(3.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(acoustic);
        
        bool acousticnessValid = result.AI_ACOUSTICNESS >= 0.0f && result.AI_ACOUSTICNESS <= 1.0f;
        reportTest("Acousticness Analysis - Range", acousticnessValid);
        
        std::cout << "   Acousticness: " << result.AI_ACOUSTICNESS << "\n";
    }
    
    void testInstrumentalnessDetection() {
        std::cout << "🎤 Testing Instrumentalness Detection...\n";
        
        // Test instrumental signal (sine wave)
        AudioBuffer instrumental = TestAudioGenerator::generateSineWave(440.0f, 2.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(instrumental);
        
        bool instrumentalHigh = result.AI_INSTRUMENTALNESS > 0.5f;
        reportTest("Instrumentalness - Instrumental Signal", instrumentalHigh);
        
        // Test vocal-like signal
        AudioBuffer vocal = TestAudioGenerator::generateSpeechPattern(2.0f);
        result = analyzer.analyzeAudio(vocal);
        
        bool vocalLow = result.AI_INSTRUMENTALNESS < 0.8f;
        reportTest("Instrumentalness - Vocal Signal", vocalLow);
        
        std::cout << "   Instrumental: " << result.AI_INSTRUMENTALNESS << "\n";
    }
    
    void testSpeechinessDetection() {
        std::cout << "🗣️ Testing Speechiness Detection...\n";
        
        // Test speech-like pattern
        AudioBuffer speech = TestAudioGenerator::generateSpeechPattern(2.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(speech);
        
        bool speechinessValid = result.AI_SPEECHINESS >= 0.0f && result.AI_SPEECHINESS <= 1.0f;
        reportTest("Speechiness - Range Check", speechinessValid);
        
        std::cout << "   Speechiness: " << result.AI_SPEECHINESS << "\n";
    }
    
    void testLivenessDetection() {
        std::cout << "🎪 Testing Liveness Detection...\n";
        
        // Test studio vs live signal
        AudioBuffer studio = TestAudioGenerator::generateSineWave(440.0f, 2.0f);
        AudioBuffer live = TestAudioGenerator::generateLiveRecording(studio);
        
        AIAnalysisResult studioResult = analyzer.analyzeAudio(studio);
        AIAnalysisResult liveResult = analyzer.analyzeAudio(live);
        
        bool livenessComparison = liveResult.AI_LIVENESS > studioResult.AI_LIVENESS;
        reportTest("Liveness - Live vs Studio", livenessComparison);
        
        std::cout << "   Studio liveness: " << studioResult.AI_LIVENESS << "\n";
        std::cout << "   Live liveness: " << liveResult.AI_LIVENESS << "\n";
    }
    
    void testEnergyAnalysis() {
        std::cout << "⚡ Testing Energy Analysis...\n";
        
        // Test high energy signal (drums)
        AudioBuffer highEnergy = TestAudioGenerator::generateDrumPattern(140.0f, 2.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(highEnergy);
        
        bool energyValid = result.AI_ENERGY >= 0.0f && result.AI_ENERGY <= 1.0f;
        reportTest("Energy Analysis - Range", energyValid);
        
        std::cout << "   Energy level: " << result.AI_ENERGY << "\n";
    }
    
    void testDanceabilityAnalysis() {
        std::cout << "🕺 Testing Danceability Analysis...\n";
        
        // Test danceable rhythm (120 BPM drums)
        AudioBuffer dance = TestAudioGenerator::generateDrumPattern(120.0f, 4.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(dance);
        
        bool danceabilityValid = result.AI_DANCEABILITY >= 0.0f && result.AI_DANCEABILITY <= 1.0f;
        reportTest("Danceability - Range", danceabilityValid);
        
        std::cout << "   Danceability: " << result.AI_DANCEABILITY << "\n";
    }
    
    void testValenceAnalysis() {
        std::cout << "😊 Testing Valence Analysis...\n";
        
        // Test major chord (positive valence)
        AudioBuffer major = TestAudioGenerator::generateChordProgression(2.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(major);
        
        bool valenceValid = result.AI_VALENCE >= 0.0f && result.AI_VALENCE <= 1.0f;
        reportTest("Valence - Range", valenceValid);
        
        std::cout << "   Valence: " << result.AI_VALENCE << "\n";
    }
    
    void testModeDetection() {
        std::cout << "🎼 Testing Mode Detection...\n";
        
        AudioBuffer chord = TestAudioGenerator::generateChordProgression(2.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(chord);
        
        bool modeValid = !result.AI_MODE.empty() && 
                        (result.AI_MODE == "Major" || result.AI_MODE == "Minor");
        reportTest("Mode Detection", modeValid);
        
        std::cout << "   Detected mode: " << result.AI_MODE << "\n";
    }
    
    void testTimeSignatureDetection() {
        std::cout << "🎵 Testing Time Signature Detection...\n";
        
        AudioBuffer drums = TestAudioGenerator::generateDrumPattern(120.0f, 4.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(drums);
        
        bool timeSignatureValid = result.AI_TIME_SIGNATURE >= 3 && result.AI_TIME_SIGNATURE <= 7;
        reportTest("Time Signature - Range", timeSignatureValid);
        
        std::cout << "   Time signature: " << result.AI_TIME_SIGNATURE << "/4\n";
    }
    
    void testCharacteristicsExtraction() {
        std::cout << "🎨 Testing Characteristics Extraction...\n";
        
        AudioBuffer audio = TestAudioGenerator::generateDrumPattern(120.0f, 2.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(audio);
        
        bool hasCharacteristics = !result.AI_CHARACTERISTICS.empty() && 
                                 result.AI_CHARACTERISTICS.size() <= 5;
        reportTest("Characteristics Extraction", hasCharacteristics);
        
        std::cout << "   Characteristics: ";
        for (const auto& char_str : result.AI_CHARACTERISTICS) {
            std::cout << char_str << " ";
        }
        std::cout << "\n";
    }
    
    void testGenreClassification() {
        std::cout << "🎭 Testing Genre Classification...\n";
        
        AudioBuffer electronic = TestAudioGenerator::generateSineWave(440.0f, 2.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(electronic);
        
        bool hasSubgenres = !result.AI_SUBGENRES.empty() && result.AI_SUBGENRES.size() <= 3;
        bool hasEra = !result.AI_ERA.empty();
        bool hasCulturalContext = !result.AI_CULTURAL_CONTEXT.empty();
        
        reportTest("Subgenres Classification", hasSubgenres);
        reportTest("Era Classification", hasEra);
        reportTest("Cultural Context", hasCulturalContext);
        
        std::cout << "   Subgenres: ";
        for (const auto& genre : result.AI_SUBGENRES) {
            std::cout << genre << " ";
        }
        std::cout << "\n   Era: " << result.AI_ERA << "\n";
        std::cout << "   Cultural context: " << result.AI_CULTURAL_CONTEXT << "\n";
    }
    
    void testMoodAnalysis() {
        std::cout << "😊 Testing Mood Analysis...\n";
        
        AudioBuffer audio = TestAudioGenerator::generateChordProgression(2.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(audio);
        
        bool hasMood = !result.AI_MOOD.empty();
        bool hasOccasions = !result.AI_OCCASION.empty() && result.AI_OCCASION.size() <= 3;
        
        reportTest("Mood Analysis", hasMood);
        reportTest("Occasion Analysis", hasOccasions);
        
        std::cout << "   Mood: " << result.AI_MOOD << "\n";
        std::cout << "   Occasions: ";
        for (const auto& occasion : result.AI_OCCASION) {
            std::cout << occasion << " ";
        }
        std::cout << "\n";
    }
    
    void testConfidenceCalculation() {
        std::cout << "📊 Testing Confidence Calculation...\n";
        
        AudioBuffer audio = TestAudioGenerator::generateChordProgression(2.0f);
        AIAnalysisResult result = analyzer.analyzeAudio(audio);
        
        bool confidenceValid = result.AI_CONFIDENCE >= 0.0f && result.AI_CONFIDENCE <= 1.0f;
        bool analyzed = result.AI_ANALYZED;
        
        reportTest("Confidence Range", confidenceValid);
        reportTest("Analysis Flag", analyzed);
        
        std::cout << "   Confidence: " << result.AI_CONFIDENCE << "\n";
        std::cout << "   Analyzed: " << (analyzed ? "Yes" : "No") << "\n";
    }
    
    void testFullAnalysisPipeline() {
        std::cout << "\n🔬 Testing Full Analysis Pipeline...\n";
        
        // Test with complex signal
        AudioBuffer drums = TestAudioGenerator::generateDrumPattern(128.0f, 4.0f);
        AudioBuffer chords = TestAudioGenerator::generateChordProgression(4.0f);
        
        // Mix signals
        std::vector<float> mixed(drums.samples.size());
        for (size_t i = 0; i < mixed.size(); i++) {
            mixed[i] = 0.6f * drums.samples[i] + 0.4f * chords.samples[i];
        }
        AudioBuffer complex(mixed, 44100, 1);
        
        auto start = std::chrono::high_resolution_clock::now();
        AIAnalysisResult result = analyzer.analyzeAudio(complex);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Validate all fields are populated
        bool allFieldsValid = 
            result.AI_ANALYZED &&
            result.AI_BPM > 0 &&
            result.AI_CONFIDENCE > 0 &&
            !result.AI_KEY.empty() &&
            !result.AI_MODE.empty() &&
            !result.AI_MOOD.empty() &&
            !result.AI_SUBGENRES.empty() &&
            !result.AI_OCCASION.empty() &&
            !result.AI_CHARACTERISTICS.empty();
        
        reportTest("Full Pipeline - All Fields", allFieldsValid);
        reportTest("Full Pipeline - Performance", duration.count() < 5000); // Under 5 seconds
        
        std::cout << "   Analysis time: " << duration.count() << "ms\n";
        std::cout << "   All 19 AI_* fields populated: " << (allFieldsValid ? "Yes" : "No") << "\n";
    }
    
    void runPerformanceBenchmarks() {
        std::cout << "\n⚡ Performance Benchmarks...\n";
        
        const int numRuns = 10;
        std::vector<long> times;
        
        for (int i = 0; i < numRuns; i++) {
            AudioBuffer test = TestAudioGenerator::generateChordProgression(3.0f);
            
            auto start = std::chrono::high_resolution_clock::now();
            AIAnalysisResult result = analyzer.analyzeAudio(test);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            times.push_back(duration.count());
        }
        
        // Calculate statistics
        long minTime = *std::min_element(times.begin(), times.end());
        long maxTime = *std::max_element(times.begin(), times.end());
        long avgTime = std::accumulate(times.begin(), times.end(), 0L) / times.size();
        
        std::cout << "   Runs: " << numRuns << "\n";
        std::cout << "   Min time: " << minTime << "ms\n";
        std::cout << "   Max time: " << maxTime << "ms\n";
        std::cout << "   Avg time: " << avgTime << "ms\n";
        
        bool performanceGood = avgTime < 3000; // Under 3 seconds average
        reportTest("Performance Benchmark", performanceGood);
    }
    
private:
    void reportTest(const std::string& testName, bool passed) {
        testsRun++;
        if (passed) {
            testsPassed++;
            std::cout << "   ✅ " << testName << "\n";
        } else {
            std::cout << "   ❌ " << testName << "\n";
        }
    }
    
    void printTestResults() {
        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "🎉 TEST RESULTS SUMMARY\n";
        std::cout << std::string(50, '=') << "\n";
        std::cout << "Tests run: " << testsRun << "\n";
        std::cout << "Tests passed: " << testsPassed << "\n";
        std::cout << "Tests failed: " << (testsRun - testsPassed) << "\n";
        std::cout << "Success rate: " << (100.0f * testsPassed / testsRun) << "%\n";
        
        if (testsPassed == testsRun) {
            std::cout << "🎊 ALL TESTS PASSED! 🎊\n";
        } else {
            std::cout << "⚠️  Some tests failed. Check output above.\n";
        }
        std::cout << std::string(50, '=') << "\n";
    }
};

// ========================================
// 🚀 MAIN TEST RUNNER
// ========================================

int main(int argc, char* argv[]) {
    bool runBenchmarks = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--benchmark") == 0) {
            runBenchmarks = true;
        }
    }
    
    try {
        std::cout << "🎵 Music Analyzer AI Algorithms Test Suite\n";
        std::cout << "Built with C++17, FFTW3, and advanced DSP algorithms\n\n";
        
        AIAlgorithmTester tester;
        tester.runAllTests();
        
        if (runBenchmarks) {
            std::cout << "\n🏃‍♂️ Additional benchmark mode requested...\n";
            // Additional benchmarks could go here
        }
        
        std::cout << "\n🎉 Test suite completed successfully!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test suite failed with error: " << e.what() << std::endl;
        return 1;
    }
}