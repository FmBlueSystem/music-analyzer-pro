#ifndef AI_ALGORITHMS_H
#define AI_ALGORITHMS_H

#include <vector>
#include <string>
#include <complex>
#include <map>
#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>

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
    int sampleRate;  // Added for complete implementations
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

// ========================================
// 🎯 HAMMS - Harmonic And Melodic Music Similarity
// ========================================

struct HAMMSVector {
    float harmonicity = 0.0f;      // Harmonic clarity (0-1)
    float melodicity = 0.0f;       // Melodic prominence (0-1)
    float rhythmicity = 0.0f;      // Rhythmic complexity (0-1)
    float timbrality = 0.0f;       // Timbral richness (0-1)
    float dynamics = 0.0f;         // Dynamic range (0-1)
    float tonality = 0.0f;         // Tonal stability (0-1)
    float temporality = 0.0f;      // Tempo consistency (0-1)
    
    // Calculate similarity between two HAMMS vectors
    float calculateSimilarity(const HAMMSVector& other) const {
        float diff = 0.0f;
        diff += std::pow(harmonicity - other.harmonicity, 2);
        diff += std::pow(melodicity - other.melodicity, 2);
        diff += std::pow(rhythmicity - other.rhythmicity, 2);
        diff += std::pow(timbrality - other.timbrality, 2);
        diff += std::pow(dynamics - other.dynamics, 2);
        diff += std::pow(tonality - other.tonality, 2);
        diff += std::pow(temporality - other.temporality, 2);
        
        // Return similarity score (1 = identical, 0 = completely different)
        return 1.0f - std::sqrt(diff / 7.0f);
    }
    
    // Convert to JSON string for storage
    std::string toJSON() const {
        std::ostringstream json;
        json << "{";
        json << "\"harmonicity\":" << harmonicity << ",";
        json << "\"melodicity\":" << melodicity << ",";
        json << "\"rhythmicity\":" << rhythmicity << ",";
        json << "\"timbrality\":" << timbrality << ",";
        json << "\"dynamics\":" << dynamics << ",";
        json << "\"tonality\":" << tonality << ",";
        json << "\"temporality\":" << temporality;
        json << "}";
        return json.str();
    }
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
    
    // HAMMS vector for music similarity
    HAMMSVector HAMMS_VECTOR;
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
    // Attack/Decay analysis structures
    struct AttackProfile {
        float duration;    // Attack duration in seconds
        float slope;       // Attack slope (amplitude/time)
        float sharpness;   // Attack sharpness metric
    };
    
    struct DecayProfile {
        float duration;    // Decay duration in seconds
        float rate;        // Exponential decay rate
        std::string type;  // "sustained", "natural", "percussive"
    };
    
    float analyzeHarmonicContent(const SpectralFeatures& features);
    float detectInstruments(const AudioBuffer& audio);
    float calculateSyntheticElements(const SpectralFeatures& features);
    
    bool isAcousticInstrument(const SpectralFeatures& features);
    float calculateAttackDecayCharacteristics(const AudioBuffer& audio);
    
    // New helper methods for complete implementation
    float findFundamentalFrequency(const SpectralFeatures& features);
    bool isPeak(const std::vector<float>& magnitude, int index);
    std::vector<float> smoothEnvelope(const std::vector<float>& envelope, int smoothingWindow);
    std::vector<int> detectOnsetPoints(const std::vector<float>& envelope);
    AttackProfile analyzeAttack(const std::vector<float>& envelope, int onsetIdx);
    DecayProfile analyzeDecay(const std::vector<float>& envelope, int peakIdx);
    float scoreAcousticCharacteristics(const AttackProfile& attack, const DecayProfile& decay);
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
    
    // YIN pitch detection methods
    std::vector<float> calculateYinDifferenceFunction(const std::vector<float>& window);
    std::vector<float> calculateCMNDF(const std::vector<float>& diff);
    int findPitchPeriod(const std::vector<float>& cmndf, float sampleRate, float minFreq, float maxFreq);
    float parabolicInterpolation(const std::vector<float>& diff, int tau);
    float analyzeSpeechIntonation(const std::vector<float>& pitchContour, const std::vector<float>& confidence);
    float analyzeProsody(const std::vector<float>& semitones);
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
    
    // RT60 estimation methods
    std::vector<int> detectImpulses(const AudioBuffer& audio);
    std::vector<float> calculateEnergyCurve(const std::vector<float>& signal, int windowSize, int hopSize);
    std::vector<float> schroederBackwardIntegration(const std::vector<float>& energy);
    float fitRT60(const std::vector<float>& schroederCurve, float timeStep);
    float estimateRT60FromDecay(const AudioBuffer& audio);
    float calculateNoiseFloor(const AudioBuffer& audio);
    float estimateReverbTime(const AudioBuffer& audio);
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
    // Melodic segment structure
    struct MelodicSegment {
        std::vector<float> pitches;
        std::vector<float> confidences;
        float startTime = 0.0f;
        float duration = 0.0f;
        float confidence = 0.0f;
    };
    
    float analyzeMajorHarmony(const ChromaVector& chroma);
    float analyzeMelodicPositivity(const AudioBuffer& audio);
    float analyzeTempoFactor(float bpm);
    float analyzeTimbralBrightness(const SpectralFeatures& features);
    
    float calculateConsonanceDissonance(const ChromaVector& chroma);
    float analyzeMelodicContour(const AudioBuffer& audio);
    
    // Melodic analysis helpers
    std::vector<MelodicSegment> extractMelodicSegments(const AudioBuffer& audio, int windowSize, int hopSize);
    float detectPitchAutocorrelation(const std::vector<float>& window, float sampleRate);
    float calculatePitchConfidence(const std::vector<float>& window, float pitch, float sampleRate);
    float analyzeMelodicDirection(const MelodicSegment& segment);
    float analyzeIntervalPositivity(const MelodicSegment& segment);
    float analyzeMelodicStability(const MelodicSegment& segment);
    float analyzePitchRange(const MelodicSegment& segment);
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
    BeatVector detectBeats(const AudioBuffer& audio);  // Made public for GenreClassifier
    
private:
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
    // Cultural features structure
    struct CulturalFeatures {
        std::string rhythmicPatterns;
        std::string scaleType;
        std::string instrumentationType;
        std::string productionEra;
        float acousticness = 0.0f;
        float energy = 0.0f;
        float valence = 0.0f;
        float danceability = 0.0f;
        float melodicComplexity = 0.0f;
        float harmonicComplexity = 0.0f;
    };
    
    std::string analyzeProductionTechniques(const SpectralFeatures& features);
    std::string analyzeInstrumentationPatterns(const AudioBuffer& audio);
    bool hasVintageCharacteristics(const SpectralFeatures& features);
    
    // Cultural context analysis methods
    CulturalFeatures extractCulturalFeatures(const AudioBuffer& audio, const AIAnalysisResult& features);
    std::string analyzeRhythmicPatterns(const AudioBuffer& audio);
    std::string analyzeScaleType(const AudioBuffer& audio, const AIAnalysisResult& features);
    std::string analyzeInstrumentationType(const AudioBuffer& audio, const AIAnalysisResult& features);
    float analyzeMelodicComplexity(const ChromaVector& chroma);
    float analyzeHarmonicComplexity(const ChromaVector& chroma, const AIAnalysisResult& features);
    std::string analyzeRegionalCharacteristics(const CulturalFeatures& cultural);
    std::string analyzeTemporalContext(const CulturalFeatures& cultural, const std::string& era);
    std::string analyzeProductionCulture(const CulturalFeatures& cultural);
    std::string synthesizeCulturalContext(const std::string& regional, const std::string& temporal,
                                        const std::string& production, const CulturalFeatures& cultural);
};

class MoodAnalyzer {
public:
    std::string analyzeMood(const AIAnalysisResult& features);
    std::vector<std::string> analyzeOccasions(const AIAnalysisResult& features);
    
private:
    std::string mapEnergyValenceToMood(float energy, float valence);
    std::vector<std::string> mapBPMEnergyToOccasions(float bpm, float energy);
};

// Forward declarations
class HAMMSAnalyzer;

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
    std::unique_ptr<HAMMSAnalyzer> hammsAnalyzer;
    
    void initializeAnalyzers();
    AIAnalysisResult combineResults(const AudioBuffer& audio);
};

class HAMMSAnalyzer {
public:
    HAMMSVector calculateHAMMS(const AudioBuffer& audio);
    
private:
    // Harmonic analysis
    float analyzeHarmonicity(const AudioBuffer& audio);
    float calculateHarmonicToNoiseRatio(const SpectralFeatures& features);
    float detectHarmonicSeries(const std::vector<float>& spectrum);
    
    // Melodic analysis  
    float analyzeMelodicity(const AudioBuffer& audio);
    std::vector<float> extractMelodicContour(const AudioBuffer& audio);
    float calculateMelodicComplexity(const std::vector<float>& contour);
    
    // Rhythmic analysis
    float analyzeRhythmicity(const AudioBuffer& audio);
    float calculateRhythmicRegularity(const OnsetVector& onsets);
    float analyzeSyncopation(const BeatVector& beats);
    
    // Timbral analysis
    float analyzeTimbrality(const AudioBuffer& audio);
    float calculateSpectralComplexity(const SpectralFeatures& features);
    float analyzeTimbralVariation(const AudioBuffer& audio);
    
    // Dynamic analysis
    float analyzeDynamics(const AudioBuffer& audio);
    float calculateDynamicRange(const AudioBuffer& audio);
    float analyzeDynamicVariation(const std::vector<float>& envelope);
    
    // Tonal analysis
    float analyzeTonality(const AudioBuffer& audio);
    float calculateTonalClarity(const ChromaVector& chroma);
    float analyzeKeyStability(const AudioBuffer& audio);
    
    // Temporal analysis
    float analyzeTemporality(const AudioBuffer& audio);
    float calculateTempoStability(const AudioBuffer& audio);
    float analyzeRhythmicConsistency(const BeatVector& beats);
};

} // namespace MusicAnalysis

#endif // AI_ALGORITHMS_H