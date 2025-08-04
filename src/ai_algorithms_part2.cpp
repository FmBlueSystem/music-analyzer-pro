// Continuation of ai_algorithms.cpp - Part 2

#include "ai_algorithms.h"
#include <numeric>

namespace MusicAnalysis {

// ========================================
// 🕺 AI_DANCEABILITY - Rhythm Analysis
// ========================================

float DanceabilityAnalyzer::calculateDanceability(const AudioBuffer& audio) {
    BeatVector beats = detectBeats(audio);
    BPMDetector bpmDetector;
    float bpm = bpmDetector.detectBPM(audio);
    
    float beatStrength = analyzeBeatStrength(beats);
    float tempoSuitability = analyzeTempoSuitability(bpm);
    float rhythmRegularity = analyzeRhythmRegularity(beats);
    
    // Formula from documentation
    return beatStrength * 0.4f + tempoSuitability * 0.3f + rhythmRegularity * 0.3f;
}

BeatVector DanceabilityAnalyzer::detectBeats(const AudioBuffer& audio) {
    // Use onset detection as basis for beat detection
    BPMDetector bpmDetector;
    OnsetVector onsets = bpmDetector.detectOnsets(audio);
    
    BeatVector beats;
    
    // Convert strong onsets to beats
    if (!onsets.onsetTimes.empty()) {
        float avgStrength = std::accumulate(onsets.onsetStrengths.begin(), onsets.onsetStrengths.end(), 0.0f) / onsets.onsetStrengths.size();
        
        for (size_t i = 0; i < onsets.onsetTimes.size(); i++) {
            if (onsets.onsetStrengths[i] > avgStrength * 1.2f) { // Strong beats only
                beats.beatTimes.push_back(onsets.onsetTimes[i]);
                beats.beatStrengths.push_back(onsets.onsetStrengths[i]);
            }
        }
    }
    
    return beats;
}

float DanceabilityAnalyzer::analyzeBeatStrength(const BeatVector& beats) {
    if (beats.beatStrengths.empty()) return 0.0f;
    
    float avgStrength = std::accumulate(beats.beatStrengths.begin(), beats.beatStrengths.end(), 0.0f) / beats.beatStrengths.size();
    float maxStrength = *std::max_element(beats.beatStrengths.begin(), beats.beatStrengths.end());
    
    // Strong, consistent beats are more danceable
    float consistency = (maxStrength > 0) ? avgStrength / maxStrength : 0.0f;
    float strength = std::min(1.0f, avgStrength * 10.0f); // Scale appropriately
    
    return (consistency * 0.6f + strength * 0.4f);
}

float DanceabilityAnalyzer::analyzeTempoSuitability(float bpm) {
    // Optimal dancing tempos from documentation
    if (bpm >= 90.0f && bpm <= 130.0f) return 1.0f;      // Optimal for most dancing
    if (bpm >= 130.0f && bpm <= 160.0f) return 0.9f;     // High energy dancing
    if (bpm >= 70.0f && bpm <= 90.0f) return 0.6f;       // Slow dancing
    if (bpm >= 160.0f && bpm <= 180.0f) return 0.7f;     // Very fast dancing
    if (bpm >= 60.0f && bpm <= 70.0f) return 0.3f;       // Too slow
    if (bpm >= 180.0f && bpm <= 200.0f) return 0.4f;     // Too fast
    return 0.1f; // Outside danceable range
}

float DanceabilityAnalyzer::analyzeRhythmRegularity(const BeatVector& beats) {
    if (beats.beatTimes.size() < 3) return 0.0f;
    
    // Calculate inter-beat intervals
    std::vector<float> intervals;
    for (size_t i = 1; i < beats.beatTimes.size(); i++) {
        intervals.push_back(beats.beatTimes[i] - beats.beatTimes[i-1]);
    }
    
    // Calculate regularity (low variance = high regularity)
    float mean = std::accumulate(intervals.begin(), intervals.end(), 0.0f) / intervals.size();
    float variance = 0.0f;
    for (float interval : intervals) {
        variance += (interval - mean) * (interval - mean);
    }
    variance /= intervals.size();
    
    float stdDev = std::sqrt(variance);
    float coefficient_of_variation = (mean > 0) ? stdDev / mean : 1.0f;
    
    // Lower CV = more regular = more danceable
    return std::max(0.0f, 1.0f - coefficient_of_variation * 2.0f);
}

float DanceabilityAnalyzer::analyzeSyncopation(const BeatVector& beats) {
    // Simplified syncopation analysis
    if (beats.beatTimes.size() < 4) return 0.0f;
    
    // Look for off-beat emphasis
    float syncopationScore = 0.0f;
    
    for (size_t i = 1; i < beats.beatTimes.size() - 1; i++) {
        float prevInterval = beats.beatTimes[i] - beats.beatTimes[i-1];
        float nextInterval = beats.beatTimes[i+1] - beats.beatTimes[i];
        
        // Syncopation often involves uneven timing
        float ratio = std::max(prevInterval, nextInterval) / std::min(prevInterval, nextInterval);
        if (ratio > 1.2f && ratio < 2.0f) {
            syncopationScore += beats.beatStrengths[i];
        }
    }
    
    float avgStrength = std::accumulate(beats.beatStrengths.begin(), beats.beatStrengths.end(), 0.0f) / beats.beatStrengths.size();
    return (avgStrength > 0) ? std::min(1.0f, syncopationScore / (avgStrength * beats.beatStrengths.size())) : 0.0f;
}

bool DanceabilityAnalyzer::isOptimalDanceTempo(float bpm) {
    return (bpm >= 90.0f && bpm <= 130.0f) || (bpm >= 130.0f && bpm <= 160.0f);
}

// ========================================
// 😊 AI_VALENCE - Musical Positivity
// ========================================

float ValenceAnalyzer::calculateValence(const AudioBuffer& audio) {
    ChromaVector chroma = AudioProcessor::calculateChroma(audio);
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    BPMDetector bpmDetector;
    float bpm = bpmDetector.detectBPM(audio);
    
    float majorHarmony = analyzeMajorHarmony(chroma);
    float melodicPositivity = analyzeMelodicPositivity(audio);
    float tempoFactor = analyzeTempoFactor(bpm);
    float timbralBrightness = analyzeTimbralBrightness(features);
    
    // Formula from documentation
    return majorHarmony * 0.3f + melodicPositivity * 0.2f + tempoFactor * 0.2f + timbralBrightness * 0.3f;
}

float ValenceAnalyzer::analyzeMajorHarmony(const ChromaVector& chroma) {
    // Compare against major vs minor chord templates
    std::vector<float> majorTriad = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // C major
    std::vector<float> minorTriad = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // C minor
    
    float majorScore = 0.0f, minorScore = 0.0f;
    
    // Test all 12 transpositions
    for (int root = 0; root < 12; root++) {
        float majorCorr = 0.0f, minorCorr = 0.0f;
        
        for (int i = 0; i < 12; i++) {
            int chromaIdx = (i + root) % 12;
            majorCorr += chroma.chroma[chromaIdx] * majorTriad[i];
            minorCorr += chroma.chroma[chromaIdx] * minorTriad[i];
        }
        
        majorScore = std::max(majorScore, majorCorr);
        minorScore = std::max(minorScore, minorCorr);
    }
    
    float total = majorScore + minorScore;
    return (total > 0) ? majorScore / total : 0.5f;
}

float ValenceAnalyzer::analyzeMelodicPositivity(const AudioBuffer& audio) {
    // Analyze melodic contour for upward vs downward motion
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    // Simplified: higher spectral centroid often correlates with upward motion
    float normalizedCentroid = std::min(1.0f, features.spectralCentroid / 3000.0f);
    
    // Consonance/dissonance analysis
    float consonance = calculateConsonanceDissonance(AudioProcessor::calculateChroma(audio));
    
    return (normalizedCentroid * 0.4f + consonance * 0.6f);
}

float ValenceAnalyzer::calculateConsonanceDissonance(const ChromaVector& chroma) {
    // Simplified consonance calculation based on perfect fifths and major thirds
    float consonanceScore = 0.0f;
    
    for (int root = 0; root < 12; root++) {
        // Perfect fifth
        int fifthIdx = (root + 7) % 12;
        consonanceScore += chroma.chroma[root] * chroma.chroma[fifthIdx] * 0.8f;
        
        // Major third
        int thirdIdx = (root + 4) % 12;
        consonanceScore += chroma.chroma[root] * chroma.chroma[thirdIdx] * 0.6f;
        
        // Minor third (less consonant but still pleasant)
        int minorThirdIdx = (root + 3) % 12;
        consonanceScore += chroma.chroma[root] * chroma.chroma[minorThirdIdx] * 0.3f;
    }
    
    return std::min(1.0f, consonanceScore * 5.0f);
}

float ValenceAnalyzer::analyzeTempoFactor(float bpm) {
    // Faster tempos generally correlate with higher valence
    if (bpm >= 120.0f && bpm <= 140.0f) return 0.9f;      // Optimal positive tempo
    if (bpm >= 100.0f && bpm <= 160.0f) return 0.8f;      // Generally positive
    if (bpm >= 80.0f && bpm <= 100.0f) return 0.6f;       // Moderate
    if (bpm >= 60.0f && bpm <= 80.0f) return 0.3f;        // Lower valence
    if (bpm < 60.0f) return 0.1f;                          // Very low valence
    if (bpm > 160.0f) return 0.7f;                         // High energy but may be aggressive
    return 0.5f;
}

float ValenceAnalyzer::analyzeTimbralBrightness(const SpectralFeatures& features) {
    // Brighter sounds generally have higher valence
    float brightness = std::min(1.0f, features.spectralCentroid / 4000.0f);
    
    // High-frequency energy ratio
    float totalEnergy = 0.0f, highFreqEnergy = 0.0f;
    for (size_t i = 0; i < features.magnitude.size(); i++) {
        totalEnergy += features.magnitude[i];
        if (features.frequencies[i] > 2000.0f) {
            highFreqEnergy += features.magnitude[i];
        }
    }
    
    float highFreqRatio = (totalEnergy > 0) ? highFreqEnergy / totalEnergy : 0.0f;
    
    return (brightness * 0.7f + highFreqRatio * 0.3f);
}

float ValenceAnalyzer::analyzeMelodicContour(const AudioBuffer& audio) {
    // Simplified melodic contour analysis
    ChromaVector chroma = AudioProcessor::calculateChroma(audio);
    
    // Find dominant pitch classes
    std::vector<int> dominantPitches;
    float threshold = 0.1f;
    
    for (int i = 0; i < 12; i++) {
        if (chroma.chroma[i] > threshold) {
            dominantPitches.push_back(i);
        }
    }
    
    if (dominantPitches.size() < 2) return 0.5f;
    
    // Analyze intervals between dominant pitches
    float positiveIntervals = 0.0f, totalIntervals = 0.0f;
    
    for (size_t i = 1; i < dominantPitches.size(); i++) {
        int interval = dominantPitches[i] - dominantPitches[i-1];
        if (interval < 0) interval += 12; // Handle octave wraparound
        
        totalIntervals++;
        
        // Positive intervals: major 2nd, major 3rd, perfect 4th, perfect 5th, major 6th, major 7th
        if (interval == 2 || interval == 4 || interval == 5 || interval == 7 || interval == 9 || interval == 11) {
            positiveIntervals++;
        }
    }
    
    return (totalIntervals > 0) ? positiveIntervals / totalIntervals : 0.5f;
}

// ========================================
// 🎼 AI_MODE - Major/Minor Detection
// ========================================

std::string ModeDetector::detectMode(const AudioBuffer& audio) {
    ChromaVector chroma = AudioProcessor::calculateChroma(audio);
    
    float majorStrength = analyzeMajorThirdStrength(chroma);
    float minorStrength = analyzeMinorThirdStrength(chroma);
    
    return classifyMode(majorStrength, minorStrength);
}

float ModeDetector::analyzeMajorThirdStrength(const ChromaVector& chroma) {
    float majorThirdStrength = 0.0f;
    
    // Check all possible major thirds
    for (int root = 0; root < 12; root++) {
        int majorThirdIdx = (root + 4) % 12;
        majorThirdStrength += chroma.chroma[root] * chroma.chroma[majorThirdIdx];
    }
    
    return majorThirdStrength;
}

float ModeDetector::analyzeMinorThirdStrength(const ChromaVector& chroma) {
    float minorThirdStrength = 0.0f;
    
    // Check all possible minor thirds
    for (int root = 0; root < 12; root++) {
        int minorThirdIdx = (root + 3) % 12;
        minorThirdStrength += chroma.chroma[root] * chroma.chroma[minorThirdIdx];
    }
    
    return minorThirdStrength;
}

std::string ModeDetector::classifyMode(float majorStrength, float minorStrength) {
    // Algorithm from documentation
    if (majorStrength > minorStrength * 1.2f) {
        return "Major";
    } else {
        return "Minor";
    }
}

// ========================================
// 🎵 AI_TIME_SIGNATURE - Meter Detection
// ========================================

int TimeSignatureDetector::detectTimeSignature(const AudioBuffer& audio) {
    BeatVector beats = detectBeats(audio);
    std::vector<float> accentPattern = analyzeAccentPattern(beats);
    return analyzeMeter(accentPattern);
}

BeatVector TimeSignatureDetector::detectBeats(const AudioBuffer& audio) {
    // Reuse beat detection from DanceabilityAnalyzer
    DanceabilityAnalyzer danceAnalyzer;
    return danceAnalyzer.detectBeats(audio);
}

std::vector<float> TimeSignatureDetector::analyzeAccentPattern(const BeatVector& beats) {
    if (beats.beatTimes.size() < 8) return std::vector<float>();
    
    // Calculate inter-beat intervals
    std::vector<float> intervals;
    for (size_t i = 1; i < beats.beatTimes.size(); i++) {
        intervals.push_back(beats.beatTimes[i] - beats.beatTimes[i-1]);
    }
    
    // Find the most common interval (basic beat)
    std::map<int, int> intervalCounts;
    for (float interval : intervals) {
        int quantizedInterval = (int)std::round(interval * 100); // Quantize to 10ms
        intervalCounts[quantizedInterval]++;
    }
    
    int basicBeatInterval = 0;
    int maxCount = 0;
    for (const auto& pair : intervalCounts) {
        if (pair.second > maxCount) {
            maxCount = pair.second;
            basicBeatInterval = pair.first;
        }
    }
    
    if (basicBeatInterval == 0) return std::vector<float>();
    
    // Analyze accent patterns based on beat strengths
    std::vector<float> accentPattern;
    float basicBeat = basicBeatInterval / 100.0f;
    
    for (size_t i = 0; i < beats.beatStrengths.size() && i < 16; i++) {
        accentPattern.push_back(beats.beatStrengths[i]);
    }
    
    return accentPattern;
}

int TimeSignatureDetector::analyzeMeter(const std::vector<float>& accentPattern) {
    if (accentPattern.size() < 4) return 4; // Default to 4/4
    
    // Test different meter patterns
    if (isThreeQuarterTime(accentPattern)) return 3;
    if (isSixEightTime(accentPattern)) return 6;
    if (isComplexMeter(accentPattern)) {
        // Analyze for 5/4 or 7/4
        if (accentPattern.size() >= 7) {
            // Look for 7-beat patterns
            float strength7 = 0.0f;
            for (size_t i = 0; i < accentPattern.size(); i += 7) {
                if (i < accentPattern.size()) strength7 += accentPattern[i];
            }
            
            float strength5 = 0.0f;
            for (size_t i = 0; i < accentPattern.size(); i += 5) {
                if (i < accentPattern.size()) strength5 += accentPattern[i];
            }
            
            if (strength7 > strength5) return 7;
            if (strength5 > 0) return 5;
        }
    }
    
    return 4; // Default to 4/4
}

bool TimeSignatureDetector::isThreeQuarterTime(const std::vector<float>& pattern) {
    if (pattern.size() < 6) return false;
    
    // Look for strong-weak-weak pattern
    float accent1 = 0.0f, accent2 = 0.0f, accent3 = 0.0f;
    int count = 0;
    
    for (size_t i = 0; i < pattern.size(); i += 3) {
        if (i + 2 < pattern.size()) {
            accent1 += pattern[i];
            accent2 += pattern[i + 1];
            accent3 += pattern[i + 2];
            count++;
        }
    }
    
    if (count == 0) return false;
    
    accent1 /= count;
    accent2 /= count;
    accent3 /= count;
    
    return (accent1 > accent2 * 1.2f && accent1 > accent3 * 1.2f);
}

bool TimeSignatureDetector::isSixEightTime(const std::vector<float>& pattern) {
    if (pattern.size() < 6) return false;
    
    // Look for strong-weak-weak-medium-weak-weak pattern
    float accent1 = 0.0f, accent4 = 0.0f, others = 0.0f;
    int count = 0;
    
    for (size_t i = 0; i < pattern.size(); i += 6) {
        if (i + 5 < pattern.size()) {
            accent1 += pattern[i];
            accent4 += pattern[i + 3];
            others += (pattern[i + 1] + pattern[i + 2] + pattern[i + 4] + pattern[i + 5]) / 4.0f;
            count++;
        }
    }
    
    if (count == 0) return false;
    
    accent1 /= count;
    accent4 /= count;
    others /= count;
    
    return (accent1 > others * 1.3f && accent4 > others * 1.1f && accent1 > accent4 * 1.1f);
}

bool TimeSignatureDetector::isComplexMeter(const std::vector<float>& pattern) {
    if (pattern.size() < 5) return false;
    
    // Look for irregular accent patterns that don't fit 3/4 or 4/4
    float variance = 0.0f;
    float mean = std::accumulate(pattern.begin(), pattern.end(), 0.0f) / pattern.size();
    
    for (float accent : pattern) {
        variance += (accent - mean) * (accent - mean);
    }
    variance /= pattern.size();
    
    float stdDev = std::sqrt(variance);
    float cv = (mean > 0) ? stdDev / mean : 0.0f;
    
    // High coefficient of variation suggests irregular meter
    return cv > 0.5f;
}

// ========================================
// 🎨 AI_CHARACTERISTICS - Timbral Features
// ========================================

std::vector<std::string> CharacteristicsExtractor::extractCharacteristics(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    std::vector<std::string> timbralFeatures = analyzeTimbralFeatures(features);
    std::vector<std::string> rhythmicPatterns = analyzeRhythmicPatterns(audio);
    std::vector<std::string> effects = analyzeEffects(audio);
    
    // Combine all characteristics
    std::vector<std::string> allCharacteristics;
    allCharacteristics.insert(allCharacteristics.end(), timbralFeatures.begin(), timbralFeatures.end());
    allCharacteristics.insert(allCharacteristics.end(), rhythmicPatterns.begin(), rhythmicPatterns.end());
    allCharacteristics.insert(allCharacteristics.end(), effects.begin(), effects.end());
    
    // Limit to 3-5 most significant characteristics
    if (allCharacteristics.size() > 5) {
        allCharacteristics.resize(5);
    }
    
    return allCharacteristics;
}

std::vector<std::string> CharacteristicsExtractor::analyzeTimbralFeatures(const SpectralFeatures& features) {
    std::vector<std::string> timbralFeatures;
    
    // Brightness analysis
    if (features.spectralCentroid > 4000.0f) {
        timbralFeatures.push_back("Bright");
    } else if (features.spectralCentroid < 1000.0f) {
        timbralFeatures.push_back("Dark");
    } else {
        timbralFeatures.push_back("Balanced");
    }
    
    // Percussiveness analysis
    if (features.zeroCrossingRate > 0.1f) {
        timbralFeatures.push_back("Percussive");
    } else if (features.zeroCrossingRate < 0.02f) {
        timbralFeatures.push_back("Smooth");
    }
    
    // Spectral rolloff analysis
    if (features.spectralRolloff > 8000.0f) {
        timbralFeatures.push_back("Full-spectrum");
    } else if (features.spectralRolloff < 3000.0f) {
        timbralFeatures.push_back("Muffled");
    }
    
    // Harmonic content
    if (hasDistortion(features)) {
        timbralFeatures.push_back("Distorted");
    }
    
    return timbralFeatures;
}

std::vector<std::string> CharacteristicsExtractor::analyzeRhythmicPatterns(const AudioBuffer& audio) {
    std::vector<std::string> rhythmicFeatures;
    
    BPMDetector bpmDetector;
    float bpm = bpmDetector.detectBPM(audio);
    
    if (bpm > 140.0f) {
        rhythmicFeatures.push_back("Driving rhythm");
    } else if (bpm < 80.0f) {
        rhythmicFeatures.push_back("Laid-back rhythm");
    } else {
        rhythmicFeatures.push_back("Moderate rhythm");
    }
    
    // Analyze rhythm complexity
    OnsetVector onsets = bpmDetector.detectOnsets(audio);
    if (!onsets.onsetTimes.empty()) {
        float duration = onsets.onsetTimes.back() - onsets.onsetTimes.front();
        float onsetDensity = onsets.onsetTimes.size() / duration;
        
        if (onsetDensity > 5.0f) {
            rhythmicFeatures.push_back("Complex rhythm");
        } else if (onsetDensity < 1.0f) {
            rhythmicFeatures.push_back("Simple rhythm");
        }
    }
    
    return rhythmicFeatures;
}

std::vector<std::string> CharacteristicsExtractor::analyzeEffects(const AudioBuffer& audio) {
    std::vector<std::string> effects;
    
    if (hasReverb(audio)) {
        effects.push_back("Reverb");
    }
    
    if (hasCompression(audio)) {
        effects.push_back("Compressed");
    }
    
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    if (hasDistortion(features)) {
        effects.push_back("Distortion");
    }
    
    return effects;
}

bool CharacteristicsExtractor::hasDistortion(const SpectralFeatures& features) {
    // Look for harmonic distortion indicators
    float totalEnergy = 0.0f;
    float highFreqEnergy = 0.0f;
    
    for (size_t i = 0; i < features.magnitude.size(); i++) {
        totalEnergy += features.magnitude[i];
        if (features.frequencies[i] > 5000.0f) {
            highFreqEnergy += features.magnitude[i];
        }
    }
    
    float highFreqRatio = (totalEnergy > 0) ? highFreqEnergy / totalEnergy : 0.0f;
    
    // High-frequency content + high zero crossing rate suggests distortion
    return (highFreqRatio > 0.3f && features.zeroCrossingRate > 0.08f);
}

bool CharacteristicsExtractor::hasReverb(const AudioBuffer& audio) {
    // Simplified reverb detection based on decay characteristics
    LivenessDetector livenessDetector;
    float liveness = livenessDetector.detectLiveness(audio);
    
    return liveness > 0.3f; // Reverb contributes to liveness score
}

bool CharacteristicsExtractor::hasCompression(const AudioBuffer& audio) {
    // Analyze dynamic range
    int windowSize = (int)(0.1f * audio.sampleRate); // 100ms windows
    std::vector<float> rmsValues;
    
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += windowSize) {
        float rms = AudioProcessor::calculateRMS(
            std::vector<float>(audio.samples.begin() + i, audio.samples.begin() + i + windowSize)
        );
        rmsValues.push_back(rms);
    }
    
    if (rmsValues.empty()) return false;
    
    float maxRMS = *std::max_element(rmsValues.begin(), rmsValues.end());
    float minRMS = *std::min_element(rmsValues.begin(), rmsValues.end());
    
    if (maxRMS == 0) return false;
    
    float dynamicRange = 20.0f * std::log10(maxRMS / (minRMS + 1e-10f));
    
    // Low dynamic range suggests compression
    return dynamicRange < 15.0f; // Less than 15dB suggests compression
}

std::string CharacteristicsExtractor::mapToSemanticTerm(float feature, const std::string& category) {
    // Map numerical features to semantic terms
    if (category == "brightness") {
        if (feature > 0.8f) return "Very bright";
        if (feature > 0.6f) return "Bright";
        if (feature > 0.4f) return "Balanced";
        if (feature > 0.2f) return "Dark";
        return "Very dark";
    } else if (category == "energy") {
        if (feature > 0.8f) return "High energy";
        if (feature > 0.6f) return "Energetic";
        if (feature > 0.4f) return "Moderate energy";
        if (feature > 0.2f) return "Low energy";
        return "Very low energy";
    }
    
    return "Unknown";
}

} // namespace MusicAnalysis