#ifndef AI_ALGORITHMS_H
#define AI_ALGORITHMS_H

#include <vector>
#include <string>
#include <complex>
#include <map>
#include <algorithm>
#include <cmath>
#include <memory>

namespace MusicAnalysis {

// ========================================
// 🎵 CORE DATA STRUCTURES
// ========================================

struct AudioBuffer {
    std::vector<float> samples;
    int sampleRate;
    int channels;
    int length;
    
    AudioBuffer(const std::vector<float>& data, int sr, int ch) 
        : samples(data), sampleRate(sr), channels(ch), length(data.size()) {}
};

struct SpectralFeatures {
    std::vector<float> magnitude;
    std::vector<float> phase;
    std::vector<float> frequencies;
    float spectralCentroid;
    float spectralRolloff;
    float zeroCrossingRate;
};

struct ChromaVector {
    std::vector<float> chroma; // 12 semitones: C, C#, D, D#, E, F, F#, G, G#, A, A#, B
    ChromaVector() : chroma(12, 0.0f) {}
};

struct OnsetVector {
    std::vector<float> onsetTimes;
    std::vector<float> onsetStrengths;
};

struct BeatVector {
    std::vector<float> beatTimes;
    std::vector<float> beatStrengths;
};

struct AIAnalysisResult {
    // Core AI_* fields
    float AI_ACOUSTICNESS = 0.0f;
    bool AI_ANALYZED = false;
    float AI_BPM = 0.0f;
    std::vector<std::string> AI_CHARACTERISTICS;
    float AI_CONFIDENCE = 0.0f;
    std::string AI_CULTURAL_CONTEXT;
    float AI_DANCEABILITY = 0.0f;
    float AI_ENERGY = 0.0f;
    std::string AI_ERA;
    float AI_INSTRUMENTALNESS = 0.0f;
    std::string AI_KEY;
    float AI_LIVENESS = 0.0f;
    float AI_LOUDNESS = 0.0f;
    std::string AI_MODE;
    std::string AI_MOOD;
    std::vector<std::string> AI_OCCASION;
    float AI_SPEECHINESS = 0.0f;
    std::vector<std::string> AI_SUBGENRES;
    int AI_TIME_SIGNATURE = 4;
    float AI_VALENCE = 0.0f;
};

// ========================================
// 🔊 CORE AUDIO PROCESSING
// ========================================

class AudioProcessor {
public:
    static AudioBuffer preprocessAudio(const std::vector<float>& rawAudio, int sampleRate);
    static std::vector<std::complex<float>> calculateFFT(const std::vector<float>& signal);
    static SpectralFeatures calculateSpectralFeatures(const AudioBuffer& audio);
    static ChromaVector calculateChroma(const AudioBuffer& audio);
    static float calculateRMS(const std::vector<float>& signal);
    
private:
    static std::vector<float> applyWindow(const std::vector<float>& signal, int windowType = 0);
    static std::vector<float> normalize(const std::vector<float>& signal);
};

// ========================================
// 🎹 AI_KEY - Krumhansl-Schmuckler Algorithm
// ========================================

class KeyDetector {
public:
    std::string detectKey(const AudioBuffer& audio);
    
private:
    ChromaVector extractChroma(const AudioBuffer& audio);
    std::string matchKeyTemplate(const ChromaVector& chroma);
    
    // Krumhansl-Schmuckler key profiles
    static const std::vector<float> MAJOR_PROFILE;
    static const std::vector<float> MINOR_PROFILE;
    static const std::vector<std::string> KEY_NAMES;
};

// ========================================
// 🥁 AI_BPM - Advanced Onset Detection
// ========================================

class BPMDetector {
public:
    float detectBPM(const AudioBuffer& audio);
    OnsetVector detectOnsets(const AudioBuffer& audio);
    
private:
    std::vector<float> calculateInterOnsetIntervals(const OnsetVector& onsets);
    float autocorrelationTempo(const std::vector<float>& intervals);
    float validateGenreBPM(float estimatedBPM);
    
    std::vector<float> calculateSpectralFlux(const AudioBuffer& audio);
    std::vector<float> adaptiveThresholding(const std::vector<float>& flux);
};

// ========================================
// 🔊 AI_LOUDNESS - EBU R128 Standard
// ========================================

class LoudnessAnalyzer {
public:
    float calculateLUFS(const AudioBuffer& audio);
    
private:
    AudioBuffer applyKWeighting(const AudioBuffer& audio);
    float calculateIntegratedLoudness(const AudioBuffer& weightedAudio);
    float convertToDBFS(float lufs);
    
    // K-weighting filter coefficients
    static const std::vector<float> K_WEIGHTING_B;
    static const std::vector<float> K_WEIGHTING_A;
};

// ========================================
// 🎸 AI_ACOUSTICNESS - Harmonic Analysis
// ========================================

class AcousticnessAnalyzer {
public:
    float calculateAcousticness(const AudioBuffer& audio);
    
private:
    float analyzeHarmonicContent(const SpectralFeatures& features);
    float detectInstruments(const AudioBuffer& audio);
    float calculateSyntheticElements(const SpectralFeatures& features);
    
    bool isAcousticInstrument(const SpectralFeatures& features);
    float calculateAttackDecayCharacteristics(const AudioBuffer& audio);
};

// ========================================
// 🎤 AI_INSTRUMENTALNESS - Vocal Detection
// ========================================

class InstrumentalnessDetector {
public:
    float detectInstrumentalness(const AudioBuffer& audio);
    
private:
    float analyzeFormants(const SpectralFeatures& features);
    float detectVocalContent(const AudioBuffer& audio);
    std::vector<float> extractFormantFrequencies(const SpectralFeatures& features);
    
    bool isVocalFrequencyRange(float frequency);
    float calculateVocalProbability(const std::vector<float>& formants);
};

// ========================================
// 🗣️ AI_SPEECHINESS - Speech Pattern Recognition
// ========================================

class SpeechinessDetector {
public:
    float detectSpeechiness(const AudioBuffer& audio);
    
private:
    float analyzeSpeechPatterns(const SpectralFeatures& features);
    float analyzeRhythmicSpeech(const AudioBuffer& audio);
    float detectConsonants(const AudioBuffer& audio);
    float analyzeIntonationContours(const AudioBuffer& audio);
};

// ========================================
// 🎪 AI_LIVENESS - Acoustic Environment
// ========================================

class LivenessDetector {
public:
    float detectLiveness(const AudioBuffer& audio);
    
private:
    float analyzeReverb(const AudioBuffer& audio);
    float analyzeBackgroundNoise(const AudioBuffer& audio);
    float analyzeSpatialCharacteristics(const AudioBuffer& audio);
    float detectCrowdNoise(const AudioBuffer& audio);
    
public:
    float calculateReverbTime(const AudioBuffer& audio);
    
private:
    bool hasStudioCharacteristics(const AudioBuffer& audio);
};

// ========================================
// ⚡ AI_ENERGY - Perceptual Intensity
// ========================================

class EnergyAnalyzer {
public:
    float calculateEnergy(const AudioBuffer& audio);
    float analyzeDynamicRange(const AudioBuffer& audio);
    
private:
    float calculateLoudnessEnergy(const AudioBuffer& audio);
    float calculateSpectralEnergy(const SpectralFeatures& features);
    float calculateRhythmicEnergy(const AudioBuffer& audio);
    
    float calculateOnsetDensity(const OnsetVector& onsets);
};

// ========================================
// 🕺 AI_DANCEABILITY - Rhythm Analysis
// ========================================

class DanceabilityAnalyzer {
public:
    float calculateDanceability(const AudioBuffer& audio);
    BeatVector detectBeats(const AudioBuffer& audio);
    
private:
    float analyzeBeatStrength(const BeatVector& beats);
    float analyzeTempoSuitability(float bpm);
    float analyzeRhythmRegularity(const BeatVector& beats);
    float analyzeSyncopation(const BeatVector& beats);
    bool isOptimalDanceTempo(float bpm);
};

// ========================================
// 😊 AI_VALENCE - Musical Positivity
// ========================================

class ValenceAnalyzer {
public:
    float calculateValence(const AudioBuffer& audio);
    
private:
    float analyzeMajorHarmony(const ChromaVector& chroma);
    float analyzeMelodicPositivity(const AudioBuffer& audio);
    float analyzeTempoFactor(float bpm);
    float analyzeTimbralBrightness(const SpectralFeatures& features);
    
    float calculateConsonanceDissonance(const ChromaVector& chroma);
    float analyzeMelodicContour(const AudioBuffer& audio);
};

// ========================================
// 🎼 AI_MODE - Major/Minor Detection
// ========================================

class ModeDetector {
public:
    std::string detectMode(const AudioBuffer& audio);
    
private:
    float analyzeMajorThirdStrength(const ChromaVector& chroma);
    float analyzeMinorThirdStrength(const ChromaVector& chroma);
    std::string classifyMode(float majorStrength, float minorStrength);
};

// ========================================
// 🎵 AI_TIME_SIGNATURE - Meter Detection
// ========================================

class TimeSignatureDetector {
public:
    int detectTimeSignature(const AudioBuffer& audio);
    
private:
    BeatVector detectBeats(const AudioBuffer& audio);
    std::vector<float> analyzeAccentPattern(const BeatVector& beats);
    int analyzeMeter(const std::vector<float>& accentPattern);
    
    bool isThreeQuarterTime(const std::vector<float>& pattern);
    bool isSixEightTime(const std::vector<float>& pattern);
    bool isComplexMeter(const std::vector<float>& pattern);
};

// ========================================
// 🎨 AI_CHARACTERISTICS - Timbral Features
// ========================================

class CharacteristicsExtractor {
public:
    std::vector<std::string> extractCharacteristics(const AudioBuffer& audio);
    bool hasCompression(const AudioBuffer& audio);
    
private:
    std::vector<std::string> analyzeTimbralFeatures(const SpectralFeatures& features);
    std::vector<std::string> analyzeRhythmicPatterns(const AudioBuffer& audio);
    std::vector<std::string> analyzeEffects(const AudioBuffer& audio);
    
    bool hasDistortion(const SpectralFeatures& features);
    bool hasReverb(const AudioBuffer& audio);
    std::string mapToSemanticTerm(float feature, const std::string& category);
};

// ========================================
// 📊 AI_CONFIDENCE - Quality Assessment
// ========================================

class ConfidenceCalculator {
public:
    float calculateOverallConfidence(const AudioBuffer& audio, const AIAnalysisResult& results);
    
private:
    float assessAudioQuality(const AudioBuffer& audio);
    float validateConsistency(const AIAnalysisResult& results);
    float calculateFeatureCertainty(const AIAnalysisResult& results);
    
    float calculateSNR(const AudioBuffer& audio);
    float detectCompressionArtifacts(const AudioBuffer& audio);
    bool isFrequencyResponseComplete(const SpectralFeatures& features);
};

// ========================================
// 🎭 CLASSIFICATION ALGORITHMS
// ========================================

class GenreClassifier {
public:
    std::vector<std::string> classifySubgenres(const AudioBuffer& audio, const AIAnalysisResult& features);
    std::string classifyEra(const AudioBuffer& audio, const AIAnalysisResult& features);
    std::string analyzeCulturalContext(const AudioBuffer& audio, const AIAnalysisResult& features);
    
private:
    std::string analyzeProductionTechniques(const SpectralFeatures& features);
    std::string analyzeInstrumentationPatterns(const AudioBuffer& audio);
    bool hasVintageCharacteristics(const SpectralFeatures& features);
};

class MoodAnalyzer {
public:
    std::string analyzeMood(const AIAnalysisResult& features);
    std::vector<std::string> analyzeOccasions(const AIAnalysisResult& features);
    
private:
    std::string mapEnergyValenceToMood(float energy, float valence);
    std::vector<std::string> mapBPMEnergyToOccasions(float bpm, float energy);
};

// ========================================
// 🚀 MASTER ANALYZER
// ========================================

class AIMetadataAnalyzer {
public:
    AIAnalysisResult analyzeAudio(const AudioBuffer& audio);
    
private:
    // Individual analyzers
    std::unique_ptr<KeyDetector> keyDetector;
    std::unique_ptr<BPMDetector> bpmDetector;
    std::unique_ptr<LoudnessAnalyzer> loudnessAnalyzer;
    std::unique_ptr<AcousticnessAnalyzer> acousticnessAnalyzer;
    std::unique_ptr<InstrumentalnessDetector> instrumentalnessDetector;
    std::unique_ptr<SpeechinessDetector> speechinessDetector;
    std::unique_ptr<LivenessDetector> livenessDetector;
    std::unique_ptr<EnergyAnalyzer> energyAnalyzer;
    std::unique_ptr<DanceabilityAnalyzer> danceabilityAnalyzer;
    std::unique_ptr<ValenceAnalyzer> valenceAnalyzer;
    std::unique_ptr<ModeDetector> modeDetector;
    std::unique_ptr<TimeSignatureDetector> timeSignatureDetector;
    std::unique_ptr<CharacteristicsExtractor> characteristicsExtractor;
    std::unique_ptr<ConfidenceCalculator> confidenceCalculator;
    std::unique_ptr<GenreClassifier> genreClassifier;
    std::unique_ptr<MoodAnalyzer> moodAnalyzer;
    
    void initializeAnalyzers();
    AIAnalysisResult combineResults(const AudioBuffer& audio);
};

} // namespace MusicAnalysis

#endif // AI_ALGORITHMS_H