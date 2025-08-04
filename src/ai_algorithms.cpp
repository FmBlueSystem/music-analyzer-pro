#include "ai_algorithms.h"
#include <fftw3.h>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <map>
#include <iostream>

namespace MusicAnalysis {

// ========================================
// 🔊 CORE AUDIO PROCESSING IMPLEMENTATION
// ========================================

AudioBuffer AudioProcessor::preprocessAudio(const std::vector<float>& rawAudio, int sampleRate) {
    // Normalize audio
    auto normalized = normalize(rawAudio);
    
    // Apply high-pass filter to remove DC component
    std::vector<float> filtered(normalized.size());
    float alpha = 0.95f;
    filtered[0] = normalized[0];
    for (size_t i = 1; i < normalized.size(); i++) {
        filtered[i] = alpha * (filtered[i-1] + normalized[i] - normalized[i-1]);
    }
    
    return AudioBuffer(filtered, sampleRate, 1);
}

std::vector<std::complex<float>> AudioProcessor::calculateFFT(const std::vector<float>& signal) {
    int N = signal.size();
    
    // Plan FFTW
    float* in = (float*)fftwf_malloc(sizeof(float) * N);
    fftwf_complex* out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * (N/2 + 1));
    
    // Copy input data
    std::copy(signal.begin(), signal.end(), in);
    
    fftwf_plan plan = fftwf_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);
    fftwf_execute(plan);
    
    // Convert to std::complex
    std::vector<std::complex<float>> result(N/2 + 1);
    for (int i = 0; i < N/2 + 1; i++) {
        result[i] = std::complex<float>(out[i][0], out[i][1]);
    }
    
    // Cleanup
    fftwf_destroy_plan(plan);
    fftwf_free(in);
    fftwf_free(out);
    
    return result;
}

SpectralFeatures AudioProcessor::calculateSpectralFeatures(const AudioBuffer& audio) {
    auto fft = calculateFFT(audio.samples);
    SpectralFeatures features;
    
    // Calculate magnitude spectrum
    features.magnitude.reserve(fft.size());
    for (const auto& complex_val : fft) {
        features.magnitude.push_back(std::abs(complex_val));
    }
    
    // Calculate frequencies
    features.frequencies.resize(fft.size());
    for (size_t i = 0; i < fft.size(); i++) {
        features.frequencies[i] = (float)i * audio.sampleRate / (2.0f * (fft.size() - 1));
    }
    
    // Spectral Centroid
    float numerator = 0.0f, denominator = 0.0f;
    for (size_t i = 0; i < features.magnitude.size(); i++) {
        numerator += features.frequencies[i] * features.magnitude[i];
        denominator += features.magnitude[i];
    }
    features.spectralCentroid = denominator > 0 ? numerator / denominator : 0.0f;
    
    // Spectral Rolloff (85% of energy)
    float totalEnergy = 0.0f;
    for (float mag : features.magnitude) {
        totalEnergy += mag * mag;
    }
    
    float cumulativeEnergy = 0.0f;
    float threshold = 0.85f * totalEnergy;
    features.spectralRolloff = 0.0f;
    
    for (size_t i = 0; i < features.magnitude.size(); i++) {
        cumulativeEnergy += features.magnitude[i] * features.magnitude[i];
        if (cumulativeEnergy >= threshold) {
            features.spectralRolloff = features.frequencies[i];
            break;
        }
    }
    
    // Zero Crossing Rate
    int zeroCrossings = 0;
    for (size_t i = 1; i < audio.samples.size(); i++) {
        if ((audio.samples[i] >= 0) != (audio.samples[i-1] >= 0)) {
            zeroCrossings++;
        }
    }
    features.zeroCrossingRate = (float)zeroCrossings / audio.samples.size();
    
    return features;
}

ChromaVector AudioProcessor::calculateChroma(const AudioBuffer& audio) {
    auto fft = calculateFFT(audio.samples);
    ChromaVector chroma;
    
    // Calculate chroma from FFT
    for (size_t i = 0; i < fft.size(); i++) {
        float frequency = (float)i * audio.sampleRate / (2.0f * (fft.size() - 1));
        if (frequency < 80.0f) continue; // Skip very low frequencies
        
        // Convert frequency to MIDI note
        float midiNote = 12.0f * std::log2(frequency / 440.0f) + 69.0f;
        int chromaticClass = (int)std::round(midiNote) % 12;
        
        if (chromaticClass >= 0 && chromaticClass < 12) {
            chroma.chroma[chromaticClass] += std::abs(fft[i]);
        }
    }
    
    // Normalize chroma vector
    float sum = std::accumulate(chroma.chroma.begin(), chroma.chroma.end(), 0.0f);
    if (sum > 0) {
        for (float& val : chroma.chroma) {
            val /= sum;
        }
    }
    
    return chroma;
}

float AudioProcessor::calculateRMS(const std::vector<float>& signal) {
    float sum = 0.0f;
    for (float sample : signal) {
        sum += sample * sample;
    }
    return std::sqrt(sum / signal.size());
}

std::vector<float> AudioProcessor::normalize(const std::vector<float>& signal) {
    float maxVal = *std::max_element(signal.begin(), signal.end());
    float minVal = *std::min_element(signal.begin(), signal.end());
    float range = std::max(std::abs(maxVal), std::abs(minVal));
    
    std::vector<float> normalized(signal.size());
    if (range > 0) {
        for (size_t i = 0; i < signal.size(); i++) {
            normalized[i] = signal[i] / range;
        }
    }
    return normalized;
}

// ========================================
// 🎹 AI_KEY - Krumhansl-Schmuckler Algorithm
// ========================================

const std::vector<float> KeyDetector::MAJOR_PROFILE = {
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
};

const std::vector<float> KeyDetector::MINOR_PROFILE = {
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
};

const std::vector<std::string> KeyDetector::KEY_NAMES = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

std::string KeyDetector::detectKey(const AudioBuffer& audio) {
    ChromaVector chroma = AudioProcessor::calculateChroma(audio);
    return matchKeyTemplate(chroma);
}

std::string KeyDetector::matchKeyTemplate(const ChromaVector& chroma) {
    float bestCorrelation = -1.0f;
    std::string bestKey = "C major";
    
    // Test all 24 keys (12 major + 12 minor)
    for (int root = 0; root < 12; root++) {
        // Test major key
        float majorCorr = 0.0f;
        for (int i = 0; i < 12; i++) {
            int chromaticIndex = (i + root) % 12;
            majorCorr += chroma.chroma[chromaticIndex] * MAJOR_PROFILE[i];
        }
        
        if (majorCorr > bestCorrelation) {
            bestCorrelation = majorCorr;
            bestKey = KEY_NAMES[root] + " major";
        }
        
        // Test minor key
        float minorCorr = 0.0f;
        for (int i = 0; i < 12; i++) {
            int chromaticIndex = (i + root) % 12;
            minorCorr += chroma.chroma[chromaticIndex] * MINOR_PROFILE[i];
        }
        
        if (minorCorr > bestCorrelation) {
            bestCorrelation = minorCorr;
            bestKey = KEY_NAMES[root] + " minor";
        }
    }
    
    return bestKey;
}

// ========================================
// 🥁 AI_BPM - Advanced Onset Detection
// ========================================

float BPMDetector::detectBPM(const AudioBuffer& audio) {
    OnsetVector onsets = detectOnsets(audio);
    std::vector<float> intervals = calculateInterOnsetIntervals(onsets);
    float bpm = autocorrelationTempo(intervals);
    return validateGenreBPM(bpm);
}

OnsetVector BPMDetector::detectOnsets(const AudioBuffer& audio) {
    std::vector<float> spectralFlux = calculateSpectralFlux(audio);
    std::vector<float> thresholds = adaptiveThresholding(spectralFlux);
    
    OnsetVector onsets;
    float timePerFrame = 512.0f / audio.sampleRate; // Assuming 512 sample hop size
    
    for (size_t i = 1; i < spectralFlux.size() - 1; i++) {
        if (spectralFlux[i] > thresholds[i] && 
            spectralFlux[i] > spectralFlux[i-1] && 
            spectralFlux[i] > spectralFlux[i+1]) {
            onsets.onsetTimes.push_back(i * timePerFrame);
            onsets.onsetStrengths.push_back(spectralFlux[i]);
        }
    }
    
    return onsets;
}

std::vector<float> BPMDetector::calculateSpectralFlux(const AudioBuffer& audio) {
    int frameSize = 1024;
    int hopSize = 512;
    std::vector<float> flux;
    
    std::vector<float> prevMagnitude;
    
    for (int i = 0; i <= (int)audio.samples.size() - frameSize; i += hopSize) {
        std::vector<float> frame(audio.samples.begin() + i, audio.samples.begin() + i + frameSize);
        auto fft = AudioProcessor::calculateFFT(frame);
        
        std::vector<float> magnitude;
        for (const auto& complex_val : fft) {
            magnitude.push_back(std::abs(complex_val));
        }
        
        if (!prevMagnitude.empty()) {
            float fluxValue = 0.0f;
            for (size_t j = 0; j < std::min(magnitude.size(), prevMagnitude.size()); j++) {
                float diff = magnitude[j] - prevMagnitude[j];
                if (diff > 0) fluxValue += diff;
            }
            flux.push_back(fluxValue);
        }
        
        prevMagnitude = magnitude;
    }
    
    return flux;
}

std::vector<float> BPMDetector::adaptiveThresholding(const std::vector<float>& flux) {
    std::vector<float> thresholds(flux.size());
    int windowSize = 5;
    
    for (size_t i = 0; i < flux.size(); i++) {
        int start = std::max(0, (int)i - windowSize);
        int end = std::min((int)flux.size(), (int)i + windowSize + 1);
        
        float mean = 0.0f;
        for (int j = start; j < end; j++) {
            mean += flux[j];
        }
        mean /= (end - start);
        
        float variance = 0.0f;
        for (int j = start; j < end; j++) {
            variance += (flux[j] - mean) * (flux[j] - mean);
        }
        variance /= (end - start);
        
        thresholds[i] = mean + 0.5f * std::sqrt(variance);
    }
    
    return thresholds;
}

std::vector<float> BPMDetector::calculateInterOnsetIntervals(const OnsetVector& onsets) {
    std::vector<float> intervals;
    for (size_t i = 1; i < onsets.onsetTimes.size(); i++) {
        intervals.push_back(onsets.onsetTimes[i] - onsets.onsetTimes[i-1]);
    }
    return intervals;
}

float BPMDetector::autocorrelationTempo(const std::vector<float>& intervals) {
    if (intervals.empty()) return 120.0f; // Default BPM
    
    // Convert intervals to BPM candidates
    std::map<int, float> bpmCandidates;
    
    for (float interval : intervals) {
        if (interval > 0.2f && interval < 2.0f) { // Valid interval range
            int bpm = (int)std::round(60.0f / interval);
            if (bpm >= 60 && bpm <= 200) {
                bpmCandidates[bpm] += 1.0f;
            }
        }
    }
    
    // Find most common BPM
    int bestBPM = 120;
    float bestScore = 0.0f;
    
    for (const auto& candidate : bpmCandidates) {
        if (candidate.second > bestScore) {
            bestScore = candidate.second;
            bestBPM = candidate.first;
        }
    }
    
    return (float)bestBPM;
}

float BPMDetector::validateGenreBPM(float estimatedBPM) {
    // Genre-aware validation ranges
    if (estimatedBPM < 60) return estimatedBPM * 2; // Double time
    if (estimatedBPM > 200) return estimatedBPM / 2; // Half time
    return estimatedBPM;
}

// ========================================
// 🔊 AI_LOUDNESS - EBU R128 Standard
// ========================================

const std::vector<float> LoudnessAnalyzer::K_WEIGHTING_B = {1.53512485958697f, -2.69169618940638f, 1.19839281085285f};
const std::vector<float> LoudnessAnalyzer::K_WEIGHTING_A = {1.0f, -1.69065929318241f, 0.73248077421585f};

float LoudnessAnalyzer::calculateLUFS(const AudioBuffer& audio) {
    AudioBuffer weightedAudio = applyKWeighting(audio);
    return calculateIntegratedLoudness(weightedAudio);
}

AudioBuffer LoudnessAnalyzer::applyKWeighting(const AudioBuffer& audio) {
    std::vector<float> filtered = audio.samples;
    
    // Apply pre-filter (high-pass around 38 Hz)
    float alpha = 0.98f;
    for (size_t i = 1; i < filtered.size(); i++) {
        filtered[i] = alpha * (filtered[i-1] + audio.samples[i] - audio.samples[i-1]);
    }
    
    // Apply high-frequency shelving filter
    // Simplified K-weighting approximation
    std::vector<float> output(filtered.size(), 0.0f);
    
    for (size_t i = 2; i < filtered.size(); i++) {
        output[i] = K_WEIGHTING_B[0] * filtered[i] + 
                   K_WEIGHTING_B[1] * filtered[i-1] + 
                   K_WEIGHTING_B[2] * filtered[i-2] -
                   K_WEIGHTING_A[1] * output[i-1] - 
                   K_WEIGHTING_A[2] * output[i-2];
    }
    
    return AudioBuffer(output, audio.sampleRate, audio.channels);
}

float LoudnessAnalyzer::calculateIntegratedLoudness(const AudioBuffer& weightedAudio) {
    // Calculate mean square in 400ms blocks
    int blockSize = (int)(0.4f * weightedAudio.sampleRate); // 400ms
    std::vector<float> blockLoudness;
    
    for (int i = 0; i <= (int)weightedAudio.samples.size() - blockSize; i += blockSize) {
        float meanSquare = 0.0f;
        for (int j = i; j < i + blockSize; j++) {
            meanSquare += weightedAudio.samples[j] * weightedAudio.samples[j];
        }
        meanSquare /= blockSize;
        
        if (meanSquare > 0) {
            float loudness = -0.691f + 10.0f * std::log10(meanSquare);
            blockLoudness.push_back(loudness);
        }
    }
    
    if (blockLoudness.empty()) return -70.0f; // Very quiet
    
    // Gating: remove blocks below threshold
    std::sort(blockLoudness.begin(), blockLoudness.end());
    float relativeThreshold = blockLoudness[blockLoudness.size() * 0.9f] - 10.0f; // 90th percentile - 10dB
    
    float integratedLoudness = 0.0f;
    int validBlocks = 0;
    
    for (float loudness : blockLoudness) {
        if (loudness >= relativeThreshold) {
            integratedLoudness += std::pow(10.0f, loudness / 10.0f);
            validBlocks++;
        }
    }
    
    if (validBlocks == 0) return -70.0f;
    
    integratedLoudness /= validBlocks;
    return -0.691f + 10.0f * std::log10(integratedLoudness);
}

float LoudnessAnalyzer::convertToDBFS(float lufs) {
    // Convert LUFS to dBFS (approximate)
    return lufs + 23.0f; // Rough conversion
}

// ========================================
// 🎸 AI_ACOUSTICNESS - Harmonic Analysis
// ========================================

float AcousticnessAnalyzer::calculateAcousticness(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    float harmonicContent = analyzeHarmonicContent(features);
    float instrumentScore = detectInstruments(audio);
    float syntheticElements = calculateSyntheticElements(features);
    
    // Scoring algorithm from documentation
    if (harmonicContent > 0.7f && syntheticElements < 0.3f) {
        return std::max(0.7f, harmonicContent);
    } else if (syntheticElements > 0.8f) {
        return std::min(0.2f, 1.0f - syntheticElements);
    }
    
    return (harmonicContent * 0.5f + instrumentScore * 0.3f + (1.0f - syntheticElements) * 0.2f);
}

float AcousticnessAnalyzer::analyzeHarmonicContent(const SpectralFeatures& features) {
    // Analyze harmonic-to-noise ratio
    float harmonicEnergy = 0.0f;
    float totalEnergy = 0.0f;
    
    for (size_t i = 1; i < features.magnitude.size(); i++) {
        totalEnergy += features.magnitude[i] * features.magnitude[i];
        
        // Look for harmonic peaks (simplified)
        if (i > 2 && i < features.magnitude.size() - 2) {
            if (features.magnitude[i] > features.magnitude[i-1] && 
                features.magnitude[i] > features.magnitude[i+1] &&
                features.magnitude[i] > features.magnitude[i-2] && 
                features.magnitude[i] > features.magnitude[i+2]) {
                harmonicEnergy += features.magnitude[i] * features.magnitude[i];
            }
        }
    }
    
    return totalEnergy > 0 ? harmonicEnergy / totalEnergy : 0.0f;
}

float AcousticnessAnalyzer::detectInstruments(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    float acousticScore = 0.0f;
    
    // Check for acoustic instrument characteristics
    if (features.spectralCentroid > 1000.0f && features.spectralCentroid < 4000.0f) {
        acousticScore += 0.3f; // Good range for acoustic instruments
    }
    
    // Check attack/decay characteristics
    float attackDecay = calculateAttackDecayCharacteristics(audio);
    acousticScore += attackDecay * 0.4f;
    
    // Check for natural harmonics
    if (features.spectralRolloff < 8000.0f) {
        acousticScore += 0.3f; // Natural instruments typically have lower rolloff
    }
    
    return std::min(1.0f, acousticScore);
}

float AcousticnessAnalyzer::calculateSyntheticElements(const SpectralFeatures& features) {
    float syntheticScore = 0.0f;
    
    // Very high frequency content suggests synthesis
    if (features.spectralCentroid > 8000.0f) {
        syntheticScore += 0.3f;
    }
    
    // Very sharp spectral rolloff suggests digital filtering
    if (features.spectralRolloff > 12000.0f) {
        syntheticScore += 0.4f;
    }
    
    // Low zero crossing rate with high energy suggests quantization
    if (features.zeroCrossingRate < 0.01f && features.spectralCentroid > 2000.0f) {
        syntheticScore += 0.3f;
    }
    
    return std::min(1.0f, syntheticScore);
}

float AcousticnessAnalyzer::calculateAttackDecayCharacteristics(const AudioBuffer& audio) {
    // Simplified attack/decay analysis
    int windowSize = (int)(0.05f * audio.sampleRate); // 50ms windows
    std::vector<float> envelopes;
    
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += windowSize) {
        float rms = 0.0f;
        for (int j = i; j < i + windowSize; j++) {
            rms += audio.samples[j] * audio.samples[j];
        }
        envelopes.push_back(std::sqrt(rms / windowSize));
    }
    
    // Look for natural attack/decay patterns
    float naturalScore = 0.0f;
    for (size_t i = 1; i < envelopes.size(); i++) {
        float ratio = envelopes[i] / (envelopes[i-1] + 1e-10f);
        if (ratio > 0.8f && ratio < 1.2f) {
            naturalScore += 1.0f; // Gradual changes are more natural
        }
    }
    
    return envelopes.size() > 0 ? naturalScore / envelopes.size() : 0.0f;
}

// ========================================
// 🎤 AI_INSTRUMENTALNESS - Vocal Detection
// ========================================

float InstrumentalnessDetector::detectInstrumentalness(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    float vocalProbability = detectVocalContent(audio);
    return 1.0f - vocalProbability;
}

float InstrumentalnessDetector::detectVocalContent(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    std::vector<float> formants = extractFormantFrequencies(features);
    
    float vocalScore = 0.0f;
    
    // Check for typical vocal formant frequencies
    bool hasF1 = false, hasF2 = false;
    for (float formant : formants) {
        if (formant >= 200.0f && formant <= 1000.0f) hasF1 = true; // F1 range
        if (formant >= 800.0f && formant <= 2500.0f) hasF2 = true; // F2 range
    }
    
    if (hasF1 && hasF2) vocalScore += 0.6f;
    
    // Check spectral centroid in vocal range
    if (features.spectralCentroid >= 500.0f && features.spectralCentroid <= 2000.0f) {
        vocalScore += 0.2f;
    }
    
    // Check for sustained tones
    ChromaVector chroma = AudioProcessor::calculateChroma(audio);
    float maxChroma = *std::max_element(chroma.chroma.begin(), chroma.chroma.end());
    if (maxChroma > 0.3f) {
        vocalScore += 0.2f; // Strong pitch suggests vocals
    }
    
    return std::min(1.0f, vocalScore);
}

std::vector<float> InstrumentalnessDetector::extractFormantFrequencies(const SpectralFeatures& features) {
    std::vector<float> formants;
    
    // Simple peak picking for formant detection
    for (size_t i = 2; i < features.magnitude.size() - 2; i++) {
        if (features.frequencies[i] > 100.0f && features.frequencies[i] < 3000.0f) {
            if (features.magnitude[i] > features.magnitude[i-1] && 
                features.magnitude[i] > features.magnitude[i+1] &&
                features.magnitude[i] > features.magnitude[i-2] && 
                features.magnitude[i] > features.magnitude[i+2]) {
                formants.push_back(features.frequencies[i]);
            }
        }
    }
    
    return formants;
}

// ========================================
// 🗣️ AI_SPEECHINESS - Speech Pattern Recognition
// ========================================

float SpeechinessDetector::detectSpeechiness(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    float speechPatterns = analyzeSpeechPatterns(features);
    float rhythmicSpeech = analyzeRhythmicSpeech(audio);
    float consonants = detectConsonants(audio);
    
    // Formula from documentation
    return speechPatterns * 0.4f + consonants * 0.3f + rhythmicSpeech * 0.3f;
}

float SpeechinessDetector::analyzeSpeechPatterns(const SpectralFeatures& features) {
    float speechScore = 0.0f;
    
    // High zero crossing rate suggests consonants
    if (features.zeroCrossingRate > 0.1f) {
        speechScore += 0.4f;
    }
    
    // Specific spectral characteristics of speech
    if (features.spectralCentroid > 1000.0f && features.spectralCentroid < 3000.0f) {
        speechScore += 0.3f;
    }
    
    // Speech has characteristic spectral rolloff
    if (features.spectralRolloff > 3000.0f && features.spectralRolloff < 8000.0f) {
        speechScore += 0.3f;
    }
    
    return std::min(1.0f, speechScore);
}

float SpeechinessDetector::analyzeRhythmicSpeech(const AudioBuffer& audio) {
    // Analyze amplitude modulation patterns typical of speech
    int windowSize = (int)(0.02f * audio.sampleRate); // 20ms windows
    std::vector<float> amplitudes;
    
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += windowSize/2) {
        float rms = AudioProcessor::calculateRMS(
            std::vector<float>(audio.samples.begin() + i, audio.samples.begin() + i + windowSize)
        );
        amplitudes.push_back(rms);
    }
    
    // Calculate amplitude modulation rate
    int modulationCount = 0;
    for (size_t i = 1; i < amplitudes.size(); i++) {
        if ((amplitudes[i] > amplitudes[i-1]) != (amplitudes[i-1] > amplitudes[i-2])) {
            modulationCount++;
        }
    }
    
    float modulationRate = (float)modulationCount / amplitudes.size();
    
    // Speech typically has 3-8 Hz modulation
    return (modulationRate > 0.1f && modulationRate < 0.5f) ? modulationRate * 2.0f : 0.0f;
}

float SpeechinessDetector::detectConsonants(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    // Consonants have high-frequency content and rapid changes
    float consonantScore = 0.0f;
    
    if (features.zeroCrossingRate > 0.15f) {
        consonantScore += 0.5f;
    }
    
    if (features.spectralCentroid > 2000.0f) {
        consonantScore += 0.3f;
    }
    
    // Look for fricative characteristics
    float highFreqEnergy = 0.0f, totalEnergy = 0.0f;
    for (size_t i = 0; i < features.magnitude.size(); i++) {
        totalEnergy += features.magnitude[i];
        if (features.frequencies[i] > 4000.0f) {
            highFreqEnergy += features.magnitude[i];
        }
    }
    
    if (totalEnergy > 0 && (highFreqEnergy / totalEnergy) > 0.2f) {
        consonantScore += 0.2f;
    }
    
    return std::min(1.0f, consonantScore);
}

float SpeechinessDetector::analyzeIntonationContours(const AudioBuffer& audio) {
    // Simplified pitch tracking for intonation analysis
    ChromaVector chroma = AudioProcessor::calculateChroma(audio);
    
    std::vector<float> pitchStrengths;
    for (float chromaVal : chroma.chroma) {
        pitchStrengths.push_back(chromaVal);
    }
    
    // Calculate pitch variation (speech has characteristic intonation)
    float mean = std::accumulate(pitchStrengths.begin(), pitchStrengths.end(), 0.0f) / pitchStrengths.size();
    float variance = 0.0f;
    for (float strength : pitchStrengths) {
        variance += (strength - mean) * (strength - mean);
    }
    variance /= pitchStrengths.size();
    
    // Moderate pitch variation suggests speech
    float stdDev = std::sqrt(variance);
    return (stdDev > 0.05f && stdDev < 0.3f) ? stdDev * 2.0f : 0.0f;
}

// ========================================
// 🎪 AI_LIVENESS - Acoustic Environment
// ========================================

float LivenessDetector::detectLiveness(const AudioBuffer& audio) {
    float reverbScore = analyzeReverb(audio);
    float noiseScore = analyzeBackgroundNoise(audio);
    float spatialScore = analyzeSpatialCharacteristics(audio);
    float crowdScore = detectCrowdNoise(audio);
    
    // Formula from documentation
    return (reverbScore + spatialScore) * 0.4f + noiseScore * 0.4f + crowdScore * 0.2f;
}

float LivenessDetector::analyzeReverb(const AudioBuffer& audio) {
    // Calculate reverb time estimate
    float reverbTime = calculateReverbTime(audio);
    
    // Live venues typically have longer reverb times
    if (reverbTime > 0.5f) return 0.8f;      // Concert hall
    if (reverbTime > 0.2f) return 0.6f;      // Club
    if (reverbTime > 0.1f) return 0.3f;      // Room
    return 0.1f;                             // Studio (dry)
}

float LivenessDetector::calculateReverbTime(const AudioBuffer& audio) {
    // Simplified RT60 estimation
    std::vector<float> envelope;
    int windowSize = (int)(0.02f * audio.sampleRate); // 20ms
    
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += windowSize) {
        float rms = AudioProcessor::calculateRMS(
            std::vector<float>(audio.samples.begin() + i, audio.samples.begin() + i + windowSize)
        );
        envelope.push_back(rms);
    }
    
    // Find decay characteristics
    float maxEnergy = *std::max_element(envelope.begin(), envelope.end());
    float decayThreshold = maxEnergy * 0.001f; // -60dB
    
    int decayStartIdx = 0, decayEndIdx = envelope.size() - 1;
    
    // Find where decay starts
    for (size_t i = 0; i < envelope.size(); i++) {
        if (envelope[i] >= maxEnergy * 0.5f) {
            decayStartIdx = i;
            break;
        }
    }
    
    // Find where it reaches -60dB
    for (int i = decayStartIdx; i < (int)envelope.size(); i++) {
        if (envelope[i] <= decayThreshold) {
            decayEndIdx = i;
            break;
        }
    }
    
    float decayTimeSeconds = (decayEndIdx - decayStartIdx) * (windowSize / (float)audio.sampleRate);
    return decayTimeSeconds;
}

float LivenessDetector::analyzeBackgroundNoise(const AudioBuffer& audio) {
    // Find quiet sections and analyze their noise characteristics
    int windowSize = (int)(0.1f * audio.sampleRate); // 100ms
    std::vector<float> noiseEstimates;
    
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += windowSize) {
        float rms = AudioProcessor::calculateRMS(
            std::vector<float>(audio.samples.begin() + i, audio.samples.begin() + i + windowSize)
        );
        if (rms < 0.1f) { // Quiet section
            noiseEstimates.push_back(rms);
        }
    }
    
    if (noiseEstimates.empty()) return 0.0f;
    
    float avgNoise = std::accumulate(noiseEstimates.begin(), noiseEstimates.end(), 0.0f) / noiseEstimates.size();
    
    // Higher background noise suggests live environment
    if (avgNoise > 0.05f) return 0.8f;
    if (avgNoise > 0.02f) return 0.5f;
    if (avgNoise > 0.01f) return 0.2f;
    return 0.1f;
}

float LivenessDetector::analyzeSpatialCharacteristics(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    float spatialScore = 0.0f;
    
    // Live recordings often have broader frequency response
    if (features.spectralRolloff > 8000.0f) {
        spatialScore += 0.3f;
    }
    
    // Check for room reflections (simplified)
    if (features.spectralCentroid > 2000.0f && features.spectralCentroid < 5000.0f) {
        spatialScore += 0.4f;
    }
    
    // Dynamic range (live recordings often have higher dynamic range)
    float maxSample = *std::max_element(audio.samples.begin(), audio.samples.end());
    float minSample = *std::min_element(audio.samples.begin(), audio.samples.end());
    float dynamicRange = maxSample - minSample;
    
    if (dynamicRange > 1.5f) spatialScore += 0.3f;
    
    return std::min(1.0f, spatialScore);
}

float LivenessDetector::detectCrowdNoise(const AudioBuffer& audio) {
    // Look for characteristics of crowd noise
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    float crowdScore = 0.0f;
    
    // Crowd noise has specific spectral characteristics
    if (features.spectralCentroid > 500.0f && features.spectralCentroid < 2000.0f) {
        crowdScore += 0.3f;
    }
    
    // High zero crossing rate from multiple voices
    if (features.zeroCrossingRate > 0.05f) {
        crowdScore += 0.4f;
    }
    
    // Broadband energy suggests crowd
    float bandwidthRatio = features.spectralRolloff / features.spectralCentroid;
    if (bandwidthRatio > 3.0f) {
        crowdScore += 0.3f;
    }
    
    return std::min(1.0f, crowdScore);
}

// ========================================
// ⚡ AI_ENERGY - Perceptual Intensity
// ========================================

float EnergyAnalyzer::calculateEnergy(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    float loudnessEnergy = calculateLoudnessEnergy(audio);
    float spectralEnergy = calculateSpectralEnergy(features);
    float rhythmicEnergy = calculateRhythmicEnergy(audio);
    
    // Formula from documentation
    return loudnessEnergy * 0.3f + spectralEnergy * 0.3f + rhythmicEnergy * 0.4f;
}

float EnergyAnalyzer::calculateLoudnessEnergy(const AudioBuffer& audio) {
    float rms = AudioProcessor::calculateRMS(audio.samples);
    
    // Map RMS to energy scale (0-1)
    if (rms > 0.5f) return 1.0f;
    if (rms > 0.3f) return 0.8f;
    if (rms > 0.1f) return 0.6f;
    if (rms > 0.05f) return 0.4f;
    if (rms > 0.01f) return 0.2f;
    return 0.1f;
}

float EnergyAnalyzer::calculateSpectralEnergy(const SpectralFeatures& features) {
    float spectralEnergy = 0.0f;
    
    // High-frequency content contributes to perceived energy
    float highFreqEnergy = 0.0f, totalEnergy = 0.0f;
    for (size_t i = 0; i < features.magnitude.size(); i++) {
        totalEnergy += features.magnitude[i];
        if (features.frequencies[i] > 2000.0f) {
            highFreqEnergy += features.magnitude[i];
        }
    }
    
    if (totalEnergy > 0) {
        spectralEnergy += (highFreqEnergy / totalEnergy) * 0.5f;
    }
    
    // Spectral centroid contributes to brightness/energy
    float normalizedCentroid = std::min(1.0f, features.spectralCentroid / 4000.0f);
    spectralEnergy += normalizedCentroid * 0.3f;
    
    // Spectral bandwidth
    float normalizedBandwidth = std::min(1.0f, features.spectralRolloff / 10000.0f);
    spectralEnergy += normalizedBandwidth * 0.2f;
    
    return std::min(1.0f, spectralEnergy);
}

float EnergyAnalyzer::calculateRhythmicEnergy(const AudioBuffer& audio) {
    BPMDetector bpmDetector;
    OnsetVector onsets = bpmDetector.detectOnsets(audio);
    
    float onsetDensity = calculateOnsetDensity(onsets);
    float dynamicRange = analyzeDynamicRange(audio);
    
    return std::min(1.0f, onsetDensity * 0.6f + dynamicRange * 0.4f);
}

float EnergyAnalyzer::calculateOnsetDensity(const OnsetVector& onsets) {
    if (onsets.onsetTimes.empty()) return 0.0f;
    
    float duration = onsets.onsetTimes.back() - onsets.onsetTimes.front();
    if (duration <= 0) return 0.0f;
    
    float density = onsets.onsetTimes.size() / duration; // onsets per second
    
    // Map to 0-1 scale
    if (density > 10.0f) return 1.0f;      // Very high energy
    if (density > 5.0f) return 0.8f;       // High energy
    if (density > 2.0f) return 0.6f;       // Medium energy
    if (density > 1.0f) return 0.4f;       // Low-medium energy
    if (density > 0.5f) return 0.2f;       // Low energy
    return 0.1f;                           // Very low energy
}

float EnergyAnalyzer::analyzeDynamicRange(const AudioBuffer& audio) {
    // Calculate dynamic range as contributing factor to energy
    int windowSize = (int)(0.1f * audio.sampleRate); // 100ms windows
    std::vector<float> rmsValues;
    
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += windowSize) {
        float rms = AudioProcessor::calculateRMS(
            std::vector<float>(audio.samples.begin() + i, audio.samples.begin() + i + windowSize)
        );
        rmsValues.push_back(rms);
    }
    
    if (rmsValues.empty()) return 0.0f;
    
    float maxRMS = *std::max_element(rmsValues.begin(), rmsValues.end());
    float minRMS = *std::min_element(rmsValues.begin(), rmsValues.end());
    
    if (maxRMS == 0) return 0.0f;
    
    float dynamicRange = 20.0f * std::log10(maxRMS / (minRMS + 1e-10f));
    
    // Higher dynamic range can contribute to energy perception
    return std::min(1.0f, dynamicRange / 40.0f); // Normalize to ~40dB max
}

// ========================================
// 🕺 AI_DANCEABILITY - Rhythm Analysis
// ========================================

float DanceabilityAnalyzer::calculateDanceability(const AudioBuffer& audio) {
    BPMDetector bpmDetector;
    float bpm = bpmDetector.detectBPM(audio);
    BeatVector beats = detectBeats(audio);
    
    float beatStrength = analyzeBeatStrength(beats);
    float tempoSuitability = analyzeTempoSuitability(bpm);
    float rhythmRegularity = analyzeRhythmRegularity(beats);
    
    // Formula from documentation
    return beatStrength * 0.4f + tempoSuitability * 0.3f + rhythmRegularity * 0.3f;
}

BeatVector DanceabilityAnalyzer::detectBeats(const AudioBuffer& audio) {
    BPMDetector bpmDetector;
    OnsetVector onsets = bpmDetector.detectOnsets(audio);
    
    BeatVector beats;
    if (onsets.onsetTimes.empty()) return beats;
    
    // Convert strong onsets to beats
    float avgStrength = 0.0f;
    for (float strength : onsets.onsetStrengths) {
        avgStrength += strength;
    }
    avgStrength /= onsets.onsetStrengths.size();
    
    for (size_t i = 0; i < onsets.onsetTimes.size(); i++) {
        if (onsets.onsetStrengths[i] > avgStrength * 1.2f) {
            beats.beatTimes.push_back(onsets.onsetTimes[i]);
            beats.beatStrengths.push_back(onsets.onsetStrengths[i]);
        }
    }
    
    return beats;
}

float DanceabilityAnalyzer::analyzeBeatStrength(const BeatVector& beats) {
    if (beats.beatStrengths.empty()) return 0.0f;
    
    float avgStrength = 0.0f;
    for (float strength : beats.beatStrengths) {
        avgStrength += strength;
    }
    avgStrength /= beats.beatStrengths.size();
    
    // Map to 0-1 scale
    return std::min(1.0f, avgStrength / 0.5f);
}

float DanceabilityAnalyzer::analyzeTempoSuitability(float bpm) {
    // Optimal dance tempo weighting from documentation
    if (bpm >= 90.0f && bpm <= 130.0f) return 1.0f;    // High weight (optimal)
    if (bpm >= 130.0f && bpm <= 160.0f) return 0.9f;   // High weight (club dancing)
    if (bpm >= 60.0f && bpm < 90.0f) return 0.6f;      // Medium weight
    if (bpm < 60.0f || bpm > 160.0f) return 0.3f;      // Lower weight
    return 0.5f;
}

float DanceabilityAnalyzer::analyzeRhythmRegularity(const BeatVector& beats) {
    if (beats.beatTimes.size() < 3) return 0.0f;
    
    // Calculate inter-beat intervals
    std::vector<float> intervals;
    for (size_t i = 1; i < beats.beatTimes.size(); i++) {
        intervals.push_back(beats.beatTimes[i] - beats.beatTimes[i-1]);
    }
    
    // Calculate coefficient of variation
    float mean = 0.0f;
    for (float interval : intervals) {
        mean += interval;
    }
    mean /= intervals.size();
    
    float variance = 0.0f;
    for (float interval : intervals) {
        variance += (interval - mean) * (interval - mean);
    }
    variance /= intervals.size();
    
    float cv = std::sqrt(variance) / mean;
    
    // Lower coefficient of variation = more regular = more danceable
    return std::max(0.0f, 1.0f - cv * 2.0f);
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
    // Calculate major vs minor chord tendencies
    float majorScore = 0.0f;
    float minorScore = 0.0f;
    
    // Major triad patterns (root, major third, fifth)
    for (int root = 0; root < 12; root++) {
        int majorThird = (root + 4) % 12;
        int fifth = (root + 7) % 12;
        majorScore += chroma.chroma[root] * chroma.chroma[majorThird] * chroma.chroma[fifth];
    }
    
    // Minor triad patterns (root, minor third, fifth)
    for (int root = 0; root < 12; root++) {
        int minorThird = (root + 3) % 12;
        int fifth = (root + 7) % 12;
        minorScore += chroma.chroma[root] * chroma.chroma[minorThird] * chroma.chroma[fifth];
    }
    
    float totalScore = majorScore + minorScore;
    return totalScore > 0 ? majorScore / totalScore : 0.5f;
}

float ValenceAnalyzer::analyzeMelodicPositivity(const AudioBuffer& audio) {
    // Simplified melodic contour analysis
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    // Rising melodic tendencies are generally more positive
    float positivityScore = 0.0f;
    
    // Higher spectral centroid suggests brighter, more positive content
    if (features.spectralCentroid > 2000.0f) {
        positivityScore += 0.4f;
    } else if (features.spectralCentroid > 1000.0f) {
        positivityScore += 0.2f;
    }
    
    // Consonant harmonic structure
    ChromaVector chroma = AudioProcessor::calculateChroma(audio);
    float consonance = calculateConsonanceDissonance(chroma);
    positivityScore += consonance * 0.6f;
    
    return std::min(1.0f, positivityScore);
}

float ValenceAnalyzer::analyzeTempoFactor(float bpm) {
    // Tempo implications for mood from documentation
    if (bpm > 120.0f) return 0.7f;        // Upbeat = more positive
    if (bpm > 90.0f) return 0.5f;         // Moderate
    if (bpm > 60.0f) return 0.3f;         // Slower = less positive
    return 0.2f;                          // Very slow
}

float ValenceAnalyzer::analyzeTimbralBrightness(const SpectralFeatures& features) {
    // Bright timbres are generally more positive
    float brightness = features.spectralCentroid / 4000.0f; // Normalize to ~0-1
    return std::min(1.0f, brightness);
}

float ValenceAnalyzer::calculateConsonanceDissonance(const ChromaVector& chroma) {
    // Simple consonance calculation based on perfect intervals
    float consonance = 0.0f;
    
    for (int i = 0; i < 12; i++) {
        // Perfect fifth (7 semitones)
        consonance += chroma.chroma[i] * chroma.chroma[(i + 7) % 12] * 0.9f;
        // Perfect fourth (5 semitones)
        consonance += chroma.chroma[i] * chroma.chroma[(i + 5) % 12] * 0.7f;
        // Major third (4 semitones)
        consonance += chroma.chroma[i] * chroma.chroma[(i + 4) % 12] * 0.6f;
        // Octave (0 semitones)
        consonance += chroma.chroma[i] * chroma.chroma[i] * 0.5f;
    }
    
    return std::min(1.0f, consonance);
}

float ValenceAnalyzer::analyzeMelodicContour(const AudioBuffer& audio) {
    // Placeholder for more sophisticated melodic analysis
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    return features.spectralCentroid / 4000.0f; // Simplified
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
    
    // Sum all major third relationships
    for (int root = 0; root < 12; root++) {
        int majorThird = (root + 4) % 12;
        majorThirdStrength += chroma.chroma[root] * chroma.chroma[majorThird];
    }
    
    return majorThirdStrength;
}

float ModeDetector::analyzeMinorThirdStrength(const ChromaVector& chroma) {
    float minorThirdStrength = 0.0f;
    
    // Sum all minor third relationships
    for (int root = 0; root < 12; root++) {
        int minorThird = (root + 3) % 12;
        minorThirdStrength += chroma.chroma[root] * chroma.chroma[minorThird];
    }
    
    return minorThirdStrength;
}

std::string ModeDetector::classifyMode(float majorStrength, float minorStrength) {
    // Decision algorithm from documentation
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
    DanceabilityAnalyzer danceAnalyzer;
    return danceAnalyzer.detectBeats(audio);
}

std::vector<float> TimeSignatureDetector::analyzeAccentPattern(const BeatVector& beats) {
    if (beats.beatTimes.size() < 8) {
        return std::vector<float>(4, 0.25f); // Default to 4/4
    }
    
    // Reserved for future complex meter analysis
    // float totalInterval = 0.0f;
    // for (size_t i = 1; i < beats.beatTimes.size(); i++) {
    //     totalInterval += beats.beatTimes[i] - beats.beatTimes[i-1];
    // }
    // float avgInterval = totalInterval / (beats.beatTimes.size() - 1);
    
    // Analyze accent patterns by grouping beats
    std::vector<float> accentPattern;
    
    // Look for patterns in beat strengths
    for (size_t i = 0; i < std::min(beats.beatStrengths.size(), size_t(16)); i++) {
        accentPattern.push_back(beats.beatStrengths[i]);
    }
    
    return accentPattern;
}

int TimeSignatureDetector::analyzeMeter(const std::vector<float>& accentPattern) {
    if (isThreeQuarterTime(accentPattern)) return 3;
    if (isSixEightTime(accentPattern)) return 6;
    if (isComplexMeter(accentPattern)) return 7; // or 5
    return 4; // Default to 4/4
}

bool TimeSignatureDetector::isThreeQuarterTime(const std::vector<float>& pattern) {
    if (pattern.size() < 6) return false;
    
    // Look for strong-weak-weak pattern
    for (size_t i = 0; i + 2 < pattern.size(); i += 3) {
        if (pattern[i] <= pattern[i+1] || pattern[i] <= pattern[i+2]) {
            return false; // Pattern doesn't hold
        }
    }
    return true;
}

bool TimeSignatureDetector::isSixEightTime(const std::vector<float>& pattern) {
    if (pattern.size() < 6) return false;
    
    // Look for compound meter: strong-weak-weak-medium-weak-weak
    for (size_t i = 0; i + 5 < pattern.size(); i += 6) {
        if (pattern[i] <= pattern[i+1] || pattern[i] <= pattern[i+2] ||
            pattern[i+3] <= pattern[i+4] || pattern[i+3] <= pattern[i+5]) {
            return false;
        }
    }
    return true;
}

bool TimeSignatureDetector::isComplexMeter(const std::vector<float>& pattern) {
    // Simple heuristic: irregular strong beat patterns
    if (pattern.size() < 5) return false;
    
    int strongBeats = 0;
    for (size_t i = 0; i < pattern.size(); i++) {
        if (pattern[i] > 0.7f) strongBeats++;
    }
    
    // Complex meters tend to have irregular accent patterns
    float strongBeatRatio = (float)strongBeats / pattern.size();
    return strongBeatRatio < 0.3f || strongBeatRatio > 0.6f;
}

// ========================================
// 🎨 AI_CHARACTERISTICS - Timbral Features
// ========================================

std::vector<std::string> CharacteristicsExtractor::extractCharacteristics(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    std::vector<std::string> characteristics;
    std::vector<std::string> timbral = analyzeTimbralFeatures(features);
    std::vector<std::string> rhythmic = analyzeRhythmicPatterns(audio);
    std::vector<std::string> effects = analyzeEffects(audio);
    
    // Combine all characteristics
    characteristics.insert(characteristics.end(), timbral.begin(), timbral.end());
    characteristics.insert(characteristics.end(), rhythmic.begin(), rhythmic.end());
    characteristics.insert(characteristics.end(), effects.begin(), effects.end());
    
    // Limit to 3-5 as per documentation
    if (characteristics.size() > 5) {
        characteristics.resize(5);
    }
    
    return characteristics;
}

std::vector<std::string> CharacteristicsExtractor::analyzeTimbralFeatures(const SpectralFeatures& features) {
    std::vector<std::string> timbral;
    
    // Brightness analysis
    if (features.spectralCentroid > 3000.0f) {
        timbral.push_back("Bright timbre");
    } else if (features.spectralCentroid < 1000.0f) {
        timbral.push_back("Dark timbre");
    }
    
    // Spectral rolloff analysis
    if (features.spectralRolloff > 8000.0f) {
        timbral.push_back("Extended harmonics");
    }
    
    // Zero crossing rate analysis
    if (features.zeroCrossingRate > 0.1f) {
        timbral.push_back("Percussive elements");
    }
    
    // Distortion detection
    if (hasDistortion(features)) {
        timbral.push_back("Distorted");
    }
    
    return timbral;
}

std::vector<std::string> CharacteristicsExtractor::analyzeRhythmicPatterns(const AudioBuffer& audio) {
    std::vector<std::string> rhythmic;
    
    BPMDetector bpmDetector;
    float bpm = bpmDetector.detectBPM(audio);
    OnsetVector onsets = bpmDetector.detectOnsets(audio);
    
    // Onset density analysis
    if (!onsets.onsetTimes.empty()) {
        float duration = onsets.onsetTimes.back() - onsets.onsetTimes.front();
        float density = onsets.onsetTimes.size() / duration;
        
        if (density > 5.0f) {
            rhythmic.push_back("Complex rhythm");
        } else if (density < 1.0f) {
            rhythmic.push_back("Simple rhythm");
        }
    }
    
    // Tempo analysis
    if (bpm > 140.0f) {
        rhythmic.push_back("Fast tempo");
    } else if (bpm < 80.0f) {
        rhythmic.push_back("Slow tempo");
    }
    
    return rhythmic;
}

std::vector<std::string> CharacteristicsExtractor::analyzeEffects(const AudioBuffer& audio) {
    std::vector<std::string> effects;
    
    if (hasReverb(audio)) {
        effects.push_back("Reverb");
    }
    
    if (hasCompression(audio)) {
        effects.push_back("Compressed");
    }
    
    return effects;
}

bool CharacteristicsExtractor::hasDistortion(const SpectralFeatures& features) {
    // Simple distortion detection based on spectral characteristics
    return features.zeroCrossingRate > 0.15f && features.spectralCentroid > 2000.0f;
}

bool CharacteristicsExtractor::hasReverb(const AudioBuffer& audio) {
    LivenessDetector livenessDetector;
    float reverbTime = livenessDetector.calculateReverbTime(audio);
    return reverbTime > 0.2f;
}

bool CharacteristicsExtractor::hasCompression(const AudioBuffer& audio) {
    EnergyAnalyzer energyAnalyzer;
    float dynamicRange = energyAnalyzer.analyzeDynamicRange(audio);
    return dynamicRange < 0.3f; // Low dynamic range suggests compression
}

std::string CharacteristicsExtractor::mapToSemanticTerm(float feature, const std::string& category) {
    // Placeholder for more sophisticated semantic mapping
    if (category == "brightness") {
        if (feature > 0.7f) return "Bright";
        if (feature < 0.3f) return "Dark";
    }
    return "Neutral";
}

// ========================================
// 📊 AI_CONFIDENCE - Quality Assessment
// ========================================

float ConfidenceCalculator::calculateOverallConfidence(const AudioBuffer& audio, const AIAnalysisResult& results) {
    float audioQuality = assessAudioQuality(audio);
    float analysisConsistency = validateConsistency(results);
    float featureCertainty = calculateFeatureCertainty(results);
    
    // Formula from documentation
    return audioQuality * 0.3f + analysisConsistency * 0.4f + featureCertainty * 0.3f;
}

float ConfidenceCalculator::assessAudioQuality(const AudioBuffer& audio) {
    float snr = calculateSNR(audio);
    float artifacts = detectCompressionArtifacts(audio);
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    bool freqResponse = isFrequencyResponseComplete(features);
    
    float qualityScore = 0.0f;
    
    // SNR contribution
    if (snr > 40.0f) qualityScore += 0.4f;      // High quality
    else if (snr > 20.0f) qualityScore += 0.3f; // Good quality
    else if (snr > 10.0f) qualityScore += 0.2f; // Fair quality
    else qualityScore += 0.1f;                   // Poor quality
    
    // Artifacts contribution
    qualityScore += (1.0f - artifacts) * 0.3f;
    
    // Frequency response contribution
    if (freqResponse) qualityScore += 0.3f;
    
    return std::min(1.0f, qualityScore);
}

float ConfidenceCalculator::validateConsistency(const AIAnalysisResult& results) {
    float consistencyScore = 1.0f;
    
    // Check for logical inconsistencies
    if (results.AI_ENERGY > 0.8f && results.AI_BPM < 80.0f) {
        consistencyScore -= 0.2f; // High energy but slow tempo is inconsistent
    }
    
    if (results.AI_DANCEABILITY > 0.8f && results.AI_BPM < 60.0f) {
        consistencyScore -= 0.3f; // High danceability but very slow tempo
    }
    
    if (results.AI_ACOUSTICNESS > 0.8f && results.AI_ENERGY > 0.9f) {
        consistencyScore -= 0.1f; // Acoustic music is rarely extremely energetic
    }
    
    if (results.AI_VALENCE > 0.8f && results.AI_MODE == "Minor") {
        consistencyScore -= 0.1f; // High valence with minor mode is less common
    }
    
    return std::max(0.0f, consistencyScore);
}

float ConfidenceCalculator::calculateFeatureCertainty(const AIAnalysisResult& results) {
    float certaintyScore = 0.0f;
    int validFeatures = 0;
    
    // Check if features are in expected ranges
    if (results.AI_BPM >= 60.0f && results.AI_BPM <= 200.0f) {
        certaintyScore += 1.0f;
        validFeatures++;
    }
    
    if (results.AI_ENERGY >= 0.0f && results.AI_ENERGY <= 1.0f) {
        certaintyScore += 1.0f;
        validFeatures++;
    }
    
    if (results.AI_VALENCE >= 0.0f && results.AI_VALENCE <= 1.0f) {
        certaintyScore += 1.0f;
        validFeatures++;
    }
    
    if (results.AI_DANCEABILITY >= 0.0f && results.AI_DANCEABILITY <= 1.0f) {
        certaintyScore += 1.0f;
        validFeatures++;
    }
    
    if (results.AI_TIME_SIGNATURE >= 3 && results.AI_TIME_SIGNATURE <= 7) {
        certaintyScore += 1.0f;
        validFeatures++;
    }
    
    return validFeatures > 0 ? certaintyScore / validFeatures : 0.0f;
}

float ConfidenceCalculator::calculateSNR(const AudioBuffer& audio) {
    // Simple SNR calculation
    float signal = AudioProcessor::calculateRMS(audio.samples);
    
    // Estimate noise from quiet sections
    int windowSize = (int)(0.1f * audio.sampleRate);
    float minRMS = signal;
    
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += windowSize) {
        std::vector<float> window(audio.samples.begin() + i, audio.samples.begin() + i + windowSize);
        float windowRMS = AudioProcessor::calculateRMS(window);
        if (windowRMS < minRMS) minRMS = windowRMS;
    }
    
    if (minRMS > 0) {
        return 20.0f * std::log10(signal / minRMS);
    }
    return 60.0f; // Assume high SNR if no noise detected
}

float ConfidenceCalculator::detectCompressionArtifacts(const AudioBuffer& audio) {
    EnergyAnalyzer energyAnalyzer;
    float dynamicRange = energyAnalyzer.analyzeDynamicRange(audio);
    
    // Lower dynamic range suggests more compression artifacts
    if (dynamicRange < 0.1f) return 0.8f; // High artifacts
    if (dynamicRange < 0.3f) return 0.5f; // Medium artifacts
    if (dynamicRange < 0.6f) return 0.2f; // Low artifacts
    return 0.0f; // Minimal artifacts
}

bool ConfidenceCalculator::isFrequencyResponseComplete(const SpectralFeatures& features) {
    // Check if frequency response covers expected range
    return features.spectralRolloff > 8000.0f; // At least up to 8kHz
}

// ========================================
// 🎭 GENRE & MOOD CLASSIFICATION
// ========================================

std::vector<std::string> GenreClassifier::classifySubgenres(const AudioBuffer& /* audio */, const AIAnalysisResult& features) {
    std::vector<std::string> subgenres;
    
    // Electronic music classification
    if (features.AI_ACOUSTICNESS < 0.3f && features.AI_ENERGY > 0.6f) {
        if (features.AI_BPM > 120.0f && features.AI_BPM < 140.0f) {
            subgenres.push_back("House");
            if (features.AI_ENERGY > 0.8f) {
                subgenres.push_back("Progressive House");
            }
        } else if (features.AI_BPM > 140.0f) {
            subgenres.push_back("Techno");
        }
    }
    
    // Rock classification
    if (features.AI_ENERGY > 0.7f && features.AI_ACOUSTICNESS > 0.2f && features.AI_ACOUSTICNESS < 0.7f) {
        subgenres.push_back("Rock");
        if (features.AI_VALENCE < 0.4f) {
            subgenres.push_back("Alternative Rock");
        }
    }
    
    // Folk/Acoustic classification
    if (features.AI_ACOUSTICNESS > 0.8f && features.AI_ENERGY < 0.5f) {
        subgenres.push_back("Folk");
        if (features.AI_INSTRUMENTALNESS < 0.2f) {
            subgenres.push_back("Singer-Songwriter");
        }
    }
    
    // Limit to 2-3 subgenres
    if (subgenres.size() > 3) {
        subgenres.resize(3);
    }
    
    return subgenres;
}

std::string GenreClassifier::classifyEra(const AudioBuffer& audio, const AIAnalysisResult& features) {
    SpectralFeatures spectral = AudioProcessor::calculateSpectralFeatures(audio);
    
    // Era classification based on production techniques
    if (hasVintageCharacteristics(spectral)) {
        if (features.AI_ACOUSTICNESS > 0.7f) return "1960s";
        if (features.AI_ENERGY > 0.6f && features.AI_ACOUSTICNESS < 0.5f) return "1970s";
    }
    
    // 1980s characteristics
    if (features.AI_ACOUSTICNESS < 0.4f && features.AI_ENERGY > 0.5f) {
        return "1980s";
    }
    
    // 1990s characteristics
    if (features.AI_ENERGY > 0.7f && features.AI_VALENCE < 0.5f) {
        return "1990s";
    }
    
    // Modern production
    if (features.AI_LOUDNESS > -10.0f) {
        if (features.AI_BPM > 100.0f && features.AI_ACOUSTICNESS < 0.3f) {
            return "2010s";
        }
        return "2000s";
    }
    
    return "Contemporary";
}

std::string GenreClassifier::analyzeCulturalContext(const AudioBuffer& /* audio */, const AIAnalysisResult& features) {
    // Simplified cultural context analysis
    if (features.AI_ACOUSTICNESS > 0.8f && features.AI_MODE == "Minor") {
        return "Folk tradition influence";
    }
    
    if (features.AI_BPM > 120.0f && features.AI_ENERGY > 0.8f) {
        return "Electronic dance culture";
    }
    
    if (features.AI_ENERGY > 0.7f && features.AI_VALENCE < 0.4f) {
        return "Alternative/underground movement";
    }
    
    if (features.AI_ACOUSTICNESS > 0.6f && features.AI_VALENCE > 0.6f) {
        return "Popular music tradition";
    }
    
    return "Contemporary Western style";
}

std::string GenreClassifier::analyzeProductionTechniques(const SpectralFeatures& features) {
    if (features.spectralRolloff < 8000.0f) {
        return "Analog warmth";
    }
    
    if (features.spectralCentroid > 3000.0f) {
        return "Digital brightness";
    }
    
    return "Balanced production";
}

std::string GenreClassifier::analyzeInstrumentationPatterns(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    if (features.spectralCentroid > 2000.0f && features.zeroCrossingRate > 0.1f) {
        return "Electric instruments";
    }
    
    if (features.spectralCentroid < 1500.0f && features.zeroCrossingRate < 0.05f) {
        return "Acoustic instruments";
    }
    
    return "Mixed instrumentation";
}

bool GenreClassifier::hasVintageCharacteristics(const SpectralFeatures& features) {
    return features.spectralRolloff < 8000.0f && features.spectralCentroid < 2000.0f;
}

std::string MoodAnalyzer::analyzeMood(const AIAnalysisResult& features) {
    return mapEnergyValenceToMood(features.AI_ENERGY, features.AI_VALENCE);
}

std::vector<std::string> MoodAnalyzer::analyzeOccasions(const AIAnalysisResult& features) {
    return mapBPMEnergyToOccasions(features.AI_BPM, features.AI_ENERGY);
}

std::string MoodAnalyzer::mapEnergyValenceToMood(float energy, float valence) {
    // Mood mapping matrix from documentation
    if (energy > 0.6f && valence > 0.6f) {
        return "Energetic, Joyful, Uplifting";
    } else if (energy > 0.6f && valence < 0.4f) {
        return "Aggressive, Intense, Powerful";
    } else if (energy < 0.4f && valence > 0.6f) {
        return "Peaceful, Content, Relaxed";
    } else if (energy < 0.4f && valence < 0.4f) {
        return "Sad, Melancholic, Contemplative";
    } else if (energy > 0.5f) {
        return "Dynamic, Engaging";
    } else if (valence > 0.5f) {
        return "Pleasant, Positive";
    } else {
        return "Neutral, Balanced";
    }
}

std::vector<std::string> MoodAnalyzer::mapBPMEnergyToOccasions(float bpm, float energy) {
    std::vector<std::string> occasions;
    
    // Algorithmic mapping from documentation
    if (bpm > 120.0f && energy > 0.7f) {
        occasions.push_back("Party");
        occasions.push_back("Workout");
        if (bpm > 140.0f) occasions.push_back("Dancing");
    } else if (bpm < 90.0f && energy < 0.4f) {
        occasions.push_back("Study");
        occasions.push_back("Relaxation");
        occasions.push_back("Meditation");
    } else if (bpm > 100.0f && bpm < 140.0f && energy > 0.5f) {
        occasions.push_back("Driving");
        occasions.push_back("Background");
    } else if (energy < 0.3f) {
        occasions.push_back("Sleep");
        occasions.push_back("Reading");
    } else {
        occasions.push_back("Background");
        occasions.push_back("Casual Listening");
    }
    
    // Limit to 2-3 occasions
    if (occasions.size() > 3) {
        occasions.resize(3);
    }
    
    return occasions;
}

// ========================================
// 🚀 MASTER ANALYZER
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
    
    // Core audio analysis
    result.AI_KEY = keyDetector->detectKey(audio);
    result.AI_BPM = bpmDetector->detectBPM(audio);
    result.AI_LOUDNESS = loudnessAnalyzer->calculateLUFS(audio);
    result.AI_ACOUSTICNESS = acousticnessAnalyzer->calculateAcousticness(audio);
    result.AI_INSTRUMENTALNESS = instrumentalnessDetector->detectInstrumentalness(audio);
    result.AI_SPEECHINESS = speechinessDetector->detectSpeechiness(audio);
    result.AI_LIVENESS = livenessDetector->detectLiveness(audio);
    result.AI_ENERGY = energyAnalyzer->calculateEnergy(audio);
    result.AI_DANCEABILITY = danceabilityAnalyzer->calculateDanceability(audio);
    result.AI_VALENCE = valenceAnalyzer->calculateValence(audio);
    result.AI_MODE = modeDetector->detectMode(audio);
    result.AI_TIME_SIGNATURE = timeSignatureDetector->detectTimeSignature(audio);
    result.AI_CHARACTERISTICS = characteristicsExtractor->extractCharacteristics(audio);
    
    // Classification analysis
    result.AI_SUBGENRES = genreClassifier->classifySubgenres(audio, result);
    result.AI_ERA = genreClassifier->classifyEra(audio, result);
    result.AI_CULTURAL_CONTEXT = genreClassifier->analyzeCulturalContext(audio, result);
    result.AI_MOOD = moodAnalyzer->analyzeMood(result);
    result.AI_OCCASION = moodAnalyzer->analyzeOccasions(result);
    
    // Final confidence calculation
    result.AI_CONFIDENCE = confidenceCalculator->calculateOverallConfidence(audio, result);
    result.AI_ANALYZED = true;
    
    return result;
}

} // namespace MusicAnalysis