// Base implementations for missing functions
#include "ai_algorithms.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace MusicAnalysis {

// ========================================
// AudioProcessor - Missing Implementation
// ========================================

std::vector<float> AudioProcessor::applyWindow(const std::vector<float>& signal, int windowType) {
    std::vector<float> windowed(signal.size());
    
    for (size_t i = 0; i < signal.size(); ++i) {
        float windowValue = 1.0f;
        
        switch (windowType) {
            case 0: // Hamming window
                windowValue = 0.54f - 0.46f * std::cos(2.0f * M_PI * i / (signal.size() - 1));
                break;
            case 1: // Hann window
                windowValue = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (signal.size() - 1)));
                break;
            case 2: // Blackman window
                windowValue = 0.42f - 0.5f * std::cos(2.0f * M_PI * i / (signal.size() - 1)) 
                            + 0.08f * std::cos(4.0f * M_PI * i / (signal.size() - 1));
                break;
            default: // Rectangular (no window)
                windowValue = 1.0f;
        }
        
        windowed[i] = signal[i] * windowValue;
    }
    
    return windowed;
}

// ========================================
// KeyDetector - Missing Implementation
// ========================================

ChromaVector KeyDetector::extractChroma(const AudioBuffer& audio) {
    // Use the AudioProcessor implementation
    AudioProcessor processor;
    return processor.calculateChroma(audio);
}

// ========================================
// InstrumentalnessDetector - Missing Implementations
// ========================================

float InstrumentalnessDetector::analyzeFormants(const SpectralFeatures& features) {
    // Analyze formant frequencies to detect vocal characteristics
    std::vector<float> formants = extractFormantFrequencies(features);
    
    if (formants.empty()) return 0.0f;
    
    // Check if formants match typical vocal ranges
    float vocalScore = 0.0f;
    
    // F1: 300-1000 Hz (vowel sounds)
    if (formants.size() > 0 && formants[0] >= 300 && formants[0] <= 1000) {
        vocalScore += 0.33f;
    }
    
    // F2: 1000-3000 Hz (vowel differentiation)
    if (formants.size() > 1 && formants[1] >= 1000 && formants[1] <= 3000) {
        vocalScore += 0.33f;
    }
    
    // F3: 2500-4000 Hz (speaker characteristics)
    if (formants.size() > 2 && formants[2] >= 2500 && formants[2] <= 4000) {
        vocalScore += 0.34f;
    }
    
    return vocalScore;
}

bool InstrumentalnessDetector::isVocalFrequencyRange(float frequency) {
    // Human vocal range typically 80-1100 Hz
    // Extended range for harmonics: 80-8000 Hz
    return frequency >= 80.0f && frequency <= 8000.0f;
}

float InstrumentalnessDetector::calculateVocalProbability(const std::vector<float>& formants) {
    if (formants.empty()) return 0.0f;
    
    // Count how many formants fall within vocal ranges
    int vocalFormants = 0;
    
    for (float formant : formants) {
        if (isVocalFrequencyRange(formant)) {
            vocalFormants++;
        }
    }
    
    // Calculate probability based on ratio of vocal formants
    float probability = static_cast<float>(vocalFormants) / formants.size();
    
    // Apply confidence based on number of formants detected
    if (formants.size() < 3) {
        probability *= 0.7f; // Lower confidence with fewer formants
    }
    
    return probability;
}

// ========================================
// LivenessDetector - Missing Implementation
// ========================================

bool LivenessDetector::hasStudioCharacteristics(const AudioBuffer& audio) {
    // Analyze characteristics typical of studio recordings
    
    // 1. Check for consistent noise floor (studio recordings have controlled noise)
    float noiseFloor = calculateNoiseFloor(audio);
    bool consistentNoise = noiseFloor < -60.0f && noiseFloor > -90.0f;
    
    // 2. Check for studio reverb characteristics
    float rt60 = estimateReverbTime(audio);
    bool studioReverb = rt60 > 0.1f && rt60 < 0.5f; // Typical studio reverb
    
    // 3. Check for compression/limiting (common in studio recordings)
    CharacteristicsExtractor extractor;
    bool hasCompression = extractor.hasCompression(audio);
    
    // 4. Check spectral consistency (studio recordings have controlled frequency response)
    AudioProcessor processor;
    SpectralFeatures features = processor.calculateSpectralFeatures(audio);
    
    // Calculate spectral flatness
    float spectralFlatness = 0.0f;
    if (!features.magnitude.empty()) {
        float geometricMean = 1.0f;
        float arithmeticMean = 0.0f;
        
        for (float mag : features.magnitude) {
            if (mag > 0) {
                geometricMean *= std::pow(mag, 1.0f / features.magnitude.size());
                arithmeticMean += mag;
            }
        }
        arithmeticMean /= features.magnitude.size();
        
        if (arithmeticMean > 0) {
            spectralFlatness = geometricMean / arithmeticMean;
        }
    }
    
    bool controlledSpectrum = spectralFlatness > 0.5f;
    
    // Studio recording if at least 3 out of 4 characteristics are present
    int studioScore = (consistentNoise ? 1 : 0) + (studioReverb ? 1 : 0) + 
                     (hasCompression ? 1 : 0) + (controlledSpectrum ? 1 : 0);
    
    return studioScore >= 3;
}

// ========================================
// 🎵 TEMPO DETECTOR - Complete Implementation
// ========================================

class TempoDetector {
public:
    float detectTempo(const AudioBuffer& audio) {
        OnsetVector onsets = detectOnsets(audio);
        
        if (onsets.onsetTimes.size() < 3) {
            return 120.0f; // Default tempo
        }
        
        // Calculate inter-onset intervals
        std::vector<float> intervals = calculateInterOnsetIntervals(onsets);
        
        // Use autocorrelation to find tempo
        float bpm = autocorrelationTempo(intervals);
        
        // Validate against common genre ranges
        return validateGenreBPM(bpm);
    }
    
    OnsetVector detectOnsets(const AudioBuffer& audio) {
        OnsetVector onsets;
        
        // Calculate spectral flux for onset detection
        std::vector<float> spectralFlux = calculateSpectralFlux(audio);
        
        // Apply adaptive thresholding
        std::vector<float> threshold = adaptiveThresholding(spectralFlux);
        
        // Find peaks above threshold
        for (size_t i = 1; i < spectralFlux.size() - 1; ++i) {
            if (spectralFlux[i] > threshold[i] && 
                spectralFlux[i] > spectralFlux[i-1] && 
                spectralFlux[i] > spectralFlux[i+1]) {
                
                float time = static_cast<float>(i * 512) / audio.sampleRate; // Assuming hop size of 512
                onsets.onsetTimes.push_back(time);
                onsets.onsetStrengths.push_back(spectralFlux[i]);
            }
        }
        
        return onsets;
    }
    
private:
    std::vector<float> calculateInterOnsetIntervals(const OnsetVector& onsets) {
        std::vector<float> intervals;
        
        for (size_t i = 1; i < onsets.onsetTimes.size(); ++i) {
            intervals.push_back(onsets.onsetTimes[i] - onsets.onsetTimes[i-1]);
        }
        
        return intervals;
    }
    
    float autocorrelationTempo(const std::vector<float>& intervals) {
        if (intervals.empty()) return 120.0f;
        
        // Build histogram of intervals
        std::map<int, float> histogram;
        
        for (float interval : intervals) {
            // Convert to BPM range (40-200 BPM)
            for (int bpm = 40; bpm <= 200; ++bpm) {
                float expectedInterval = 60.0f / bpm;
                
                // Check if this interval matches this BPM (or multiples)
                for (int multiple = 1; multiple <= 4; ++multiple) {
                    float targetInterval = expectedInterval * multiple;
                    float diff = std::abs(interval - targetInterval);
                    
                    if (diff < 0.05f) { // 50ms tolerance
                        histogram[bpm] += 1.0f / multiple; // Weight by multiple
                    }
                }
            }
        }
        
        // Find BPM with highest score
        int bestBPM = 120;
        float bestScore = 0.0f;
        
        for (const auto& [bpm, score] : histogram) {
            if (score > bestScore) {
                bestScore = score;
                bestBPM = bpm;
            }
        }
        
        return static_cast<float>(bestBPM);
    }
    
    float validateGenreBPM(float estimatedBPM) {
        // Ensure BPM is within reasonable range
        if (estimatedBPM < 40.0f) return estimatedBPM * 2.0f;
        if (estimatedBPM > 200.0f) return estimatedBPM / 2.0f;
        return estimatedBPM;
    }
    
    std::vector<float> calculateSpectralFlux(const AudioBuffer& audio) {
        std::vector<float> flux;
        const int windowSize = 2048;
        const int hopSize = 512;
        
        AudioProcessor processor;
        std::vector<float> prevMagnitude(windowSize / 2 + 1, 0.0f);
        
        for (int i = 0; i <= audio.length - windowSize; i += hopSize) {
            // Extract window
            AudioBuffer window(
                std::vector<float>(audio.samples.begin() + i, 
                                 audio.samples.begin() + i + windowSize),
                audio.sampleRate, audio.channels
            );
            
            // Calculate spectrum
            SpectralFeatures features = processor.calculateSpectralFeatures(window);
            
            // Calculate flux as sum of positive differences
            float frameFlux = 0.0f;
            for (size_t j = 0; j < features.magnitude.size() && j < prevMagnitude.size(); ++j) {
                float diff = features.magnitude[j] - prevMagnitude[j];
                if (diff > 0) {
                    frameFlux += diff;
                }
            }
            
            flux.push_back(frameFlux);
            prevMagnitude = features.magnitude;
        }
        
        return flux;
    }
    
    std::vector<float> adaptiveThresholding(const std::vector<float>& flux) {
        std::vector<float> threshold(flux.size());
        const int windowSize = 10; // Local window for threshold calculation
        
        for (size_t i = 0; i < flux.size(); ++i) {
            // Calculate local statistics
            float localMean = 0.0f;
            float localStd = 0.0f;
            int count = 0;
            
            for (int j = -windowSize; j <= windowSize; ++j) {
                int idx = i + j;
                if (idx >= 0 && idx < static_cast<int>(flux.size())) {
                    localMean += flux[idx];
                    count++;
                }
            }
            
            if (count > 0) {
                localMean /= count;
                
                // Calculate standard deviation
                for (int j = -windowSize; j <= windowSize; ++j) {
                    int idx = i + j;
                    if (idx >= 0 && idx < static_cast<int>(flux.size())) {
                        localStd += std::pow(flux[idx] - localMean, 2);
                    }
                }
                localStd = std::sqrt(localStd / count);
                
                // Set threshold as mean + 1.5 * std
                threshold[i] = localMean + 1.5f * localStd;
            }
        }
        
        return threshold;
    }
};

// ========================================
// TimeSignatureDetector - Implementation
// ========================================

// TimeSignatureDetector::detectBeats moved to ai_algorithms_part2.cpp
#if 0
BeatVector TimeSignatureDetector::detectBeats(const AudioBuffer& audio) {
    BeatVector beats;
    
    // Use tempo detector to get onsets
    TempoDetector tempoDetector;
    OnsetVector onsets = tempoDetector.detectOnsets(audio);
    
    if (onsets.onsetTimes.empty()) {
        return beats;
    }
    
    // Extract strong beats based on onset strength
    float meanStrength = std::accumulate(onsets.onsetStrengths.begin(), 
                                       onsets.onsetStrengths.end(), 0.0f) / onsets.onsetStrengths.size();
    
    for (size_t i = 0; i < onsets.onsetTimes.size(); ++i) {
        if (onsets.onsetStrengths[i] > meanStrength * 1.2f) { // Strong beats
            beats.beatTimes.push_back(onsets.onsetTimes[i]);
            beats.beatStrengths.push_back(onsets.onsetStrengths[i]);
        }
    }
    
    return beats;
}
#endif

// ========================================
// LivenessDetector - Missing Methods
// ========================================

float LivenessDetector::calculateNoiseFloor(const AudioBuffer& audio) {
    // Calculate noise floor in dB
    const int windowSize = 2048;
    const int hopSize = 1024;
    
    std::vector<float> energies;
    
    for (int i = 0; i <= audio.length - windowSize; i += hopSize) {
        float windowEnergy = 0.0f;
        for (int j = 0; j < windowSize; ++j) {
            windowEnergy += audio.samples[i + j] * audio.samples[i + j];
        }
        energies.push_back(windowEnergy / windowSize);
    }
    
    // Sort energies and take 10th percentile as noise floor
    std::sort(energies.begin(), energies.end());
    int percentileIndex = energies.size() * 0.1f;
    float noiseFloorEnergy = energies[percentileIndex];
    
    // Convert to dB
    return 20.0f * std::log10(std::sqrt(noiseFloorEnergy) + 1e-10f);
}

float LivenessDetector::estimateReverbTime(const AudioBuffer& audio) {
    // Wrapper for calculateReverbTime
    return calculateReverbTime(audio);
}

} // namespace MusicAnalysis