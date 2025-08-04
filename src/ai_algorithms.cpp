#include "ai_algorithms.h"
#include <fftw3.h>
#include <cmath>
#include <complex>
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
    
    // ITU-R BS.1770 K-weighting filter implementation
    // Stage 1: Pre-filter (high-pass)
    // Butterworth filter fc = 38 Hz
    double f0 = 38.0;
    double Q = 0.5;
    double K = tan(M_PI * f0 / audio.sampleRate);
    double norm = 1.0 / (1.0 + K / Q + K * K);
    double a0 = 1.0 * norm;
    double a1 = -2.0 * norm;
    double a2 = 1.0 * norm;
    double b1 = 2.0 * (K * K - 1.0) * norm;
    double b2 = (1.0 - K / Q + K * K) * norm;
    
    std::vector<float> stage1(filtered.size());
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    
    for (size_t i = 0; i < filtered.size(); i++) {
        double x0 = filtered[i];
        double y0 = a0 * x0 + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
        
        stage1[i] = y0;
        
        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = y0;
    }
    
    // Stage 2: High-frequency shelf
    // Butterworth high shelf fc = 1681 Hz, G = +3.999843 dB
    double f1 = 1681.0;
    double G = pow(10.0, 3.999843 / 20.0);
    double K1 = tan(M_PI * f1 / audio.sampleRate);
    double V0 = pow(10.0, G / 20.0);
    double root2 = sqrt(2.0);
    
    double a0s, a1s, a2s, b0s, b1s, b2s;
    
    if (G >= 0) {
        double norm1 = 1.0 / (1.0 + root2 * K1 + K1 * K1);
        a0s = (V0 + root2 * sqrt(V0) * K1 + K1 * K1) * norm1;
        a1s = 2.0 * (K1 * K1 - V0) * norm1;
        a2s = (V0 - root2 * sqrt(V0) * K1 + K1 * K1) * norm1;
        b0s = 1.0;
        b1s = 2.0 * (K1 * K1 - 1.0) * norm1;
        b2s = (1.0 - root2 * K1 + K1 * K1) * norm1;
    } else {
        double norm1 = 1.0 / (V0 + root2 * sqrt(V0) * K1 + K1 * K1);
        a0s = (1.0 + root2 * K1 + K1 * K1) * norm1;
        a1s = 2.0 * (K1 * K1 - 1.0) * norm1;
        a2s = (1.0 - root2 * K1 + K1 * K1) * norm1;
        b0s = 1.0;
        b1s = 2.0 * (K1 * K1 - V0) * norm1;
        b2s = (V0 - root2 * sqrt(V0) * K1 + K1 * K1) * norm1;
    }
    
    std::vector<float> output(stage1.size());
    x1 = 0; x2 = 0; y1 = 0; y2 = 0;
    
    for (size_t i = 0; i < stage1.size(); i++) {
        double x0 = stage1[i];
        double y0 = a0s * x0 + a1s * x1 + a2s * x2 - b1s * y1 - b2s * y2;
        
        output[i] = y0;
        
        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = y0;
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
    // Complete harmonic-to-noise ratio analysis with proper harmonic detection
    float harmonicEnergy = 0.0f;
    float noiseEnergy = 0.0f;
    float totalEnergy = 0.0f;
    
    // Find fundamental frequency using autocorrelation
    float fundamentalFreq = findFundamentalFrequency(features);
    if (fundamentalFreq <= 0) return 0.0f;
    
    // Calculate bin resolution
    float binResolution = (features.sampleRate / 2.0f) / features.magnitude.size();
    int fundamentalBin = static_cast<int>(fundamentalFreq / binResolution);
    
    // Analyze up to 10 harmonics
    const int maxHarmonics = 10;
    std::vector<bool> harmonicBins(features.magnitude.size(), false);
    
    for (int harmonic = 1; harmonic <= maxHarmonics; ++harmonic) {
        int targetBin = fundamentalBin * harmonic;
        if (targetBin >= static_cast<int>(features.magnitude.size())) break;
        
        // Search for peak near expected harmonic position (±3 bins tolerance)
        int peakBin = -1;
        float maxMag = 0.0f;
        
        for (int offset = -3; offset <= 3; ++offset) {
            int bin = targetBin + offset;
            if (bin >= 0 && bin < static_cast<int>(features.magnitude.size())) {
                if (features.magnitude[bin] > maxMag && 
                    isPeak(features.magnitude, bin)) {
                    maxMag = features.magnitude[bin];
                    peakBin = bin;
                }
            }
        }
        
        if (peakBin >= 0) {
            // Mark bins around harmonic peak
            for (int offset = -2; offset <= 2; ++offset) {
                int bin = peakBin + offset;
                if (bin >= 0 && bin < static_cast<int>(features.magnitude.size())) {
                    harmonicBins[bin] = true;
                }
            }
        }
    }
    
    // Calculate harmonic and noise energy
    for (size_t i = 0; i < features.magnitude.size(); ++i) {
        float energy = features.magnitude[i] * features.magnitude[i];
        totalEnergy += energy;
        
        if (harmonicBins[i]) {
            harmonicEnergy += energy;
        } else {
            noiseEnergy += energy;
        }
    }
    
    if (totalEnergy <= 0) return 0.0f;
    
    // Calculate harmonic-to-noise ratio
    float hnr = harmonicEnergy / (noiseEnergy + 1e-10f);
    
    // Convert to normalized score (0-1)
    return std::tanh(hnr * 0.5f); // Smooth mapping using tanh
}

float AcousticnessAnalyzer::findFundamentalFrequency(const SpectralFeatures& features) {
    // Enhanced fundamental frequency detection using autocorrelation on spectrum
    if (features.magnitude.size() < 100) return 0.0f;
    
    // Find spectral peaks
    std::vector<std::pair<int, float>> peaks;
    for (size_t i = 1; i < features.magnitude.size() - 1; ++i) {
        if (isPeak(features.magnitude, i) && features.magnitude[i] > 0.01f) {
            peaks.push_back({i, features.magnitude[i]});
        }
    }
    
    if (peaks.empty()) return 0.0f;
    
    // Sort peaks by magnitude
    std::sort(peaks.begin(), peaks.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Use strongest peaks for fundamental detection
    const int maxPeaks = std::min(20, static_cast<int>(peaks.size()));
    float binResolution = (features.sampleRate / 2.0f) / features.magnitude.size();
    
    // Try different fundamental candidates
    float bestFundamental = 0.0f;
    float bestScore = 0.0f;
    
    for (int i = 0; i < maxPeaks && i < 5; ++i) {
        float candidateFreq = peaks[i].first * binResolution;
        if (candidateFreq < 80.0f || candidateFreq > 2000.0f) continue;
        
        // Score based on how well harmonics align
        float score = 0.0f;
        int harmonicsFound = 0;
        
        for (int h = 2; h <= 8; ++h) {
            float harmonicFreq = candidateFreq * h;
            int harmonicBin = static_cast<int>(harmonicFreq / binResolution);
            
            // Check if harmonic exists in peaks
            for (const auto& peak : peaks) {
                if (std::abs(peak.first - harmonicBin) <= 3) {
                    score += peak.second / (h * h); // Weight by harmonic number
                    harmonicsFound++;
                    break;
                }
            }
        }
        
        if (harmonicsFound >= 2 && score > bestScore) {
            bestScore = score;
            bestFundamental = candidateFreq;
        }
    }
    
    return bestFundamental;
}

bool AcousticnessAnalyzer::isPeak(const std::vector<float>& magnitude, int index) {
    if (index <= 0 || index >= static_cast<int>(magnitude.size()) - 1) return false;
    
    // Check if local maximum (considering 2 neighbors on each side)
    for (int offset = 1; offset <= 2; ++offset) {
        if (index - offset >= 0 && magnitude[index] <= magnitude[index - offset]) return false;
        if (index + offset < static_cast<int>(magnitude.size()) && 
            magnitude[index] <= magnitude[index + offset]) return false;
    }
    
    return true;
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
    // Complete attack/decay analysis with transient detection and envelope modeling
    const int windowSize = static_cast<int>(0.002f * audio.sampleRate); // 2ms windows for fine resolution
    const int hopSize = windowSize / 4; // 75% overlap
    
    // Calculate amplitude envelope
    std::vector<float> envelope;
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += hopSize) {
        float rms = 0.0f;
        for (int j = 0; j < windowSize; j++) {
            float sample = audio.samples[i + j];
            rms += sample * sample;
        }
        envelope.push_back(std::sqrt(rms / windowSize));
    }
    
    if (envelope.size() < 10) return 0.5f;
    
    // Smooth envelope to reduce noise
    std::vector<float> smoothedEnvelope = smoothEnvelope(envelope, 3);
    
    // Detect onset points
    std::vector<int> onsetIndices = detectOnsetPoints(smoothedEnvelope);
    if (onsetIndices.empty()) return 0.5f;
    
    // Analyze attack and decay characteristics for each onset
    float totalScore = 0.0f;
    int validOnsets = 0;
    
    for (int onsetIdx : onsetIndices) {
        // Analyze attack phase
        AttackProfile attack = analyzeAttack(smoothedEnvelope, onsetIdx);
        
        // Find decay start (peak after onset)
        int peakIdx = onsetIdx;
        for (int i = onsetIdx; i < std::min(onsetIdx + 50, static_cast<int>(smoothedEnvelope.size())); ++i) {
            if (smoothedEnvelope[i] > smoothedEnvelope[peakIdx]) {
                peakIdx = i;
            }
        }
        
        // Analyze decay phase
        DecayProfile decay = analyzeDecay(smoothedEnvelope, peakIdx);
        
        // Score based on acoustic instrument characteristics
        float onsetScore = scoreAcousticCharacteristics(attack, decay);
        totalScore += onsetScore;
        validOnsets++;
    }
    
    return validOnsets > 0 ? totalScore / validOnsets : 0.5f;
}

// Helper methods for AcousticnessAnalyzer
std::vector<float> AcousticnessAnalyzer::smoothEnvelope(const std::vector<float>& envelope, int smoothingWindow) {
    std::vector<float> smoothed(envelope.size());
    
    for (size_t i = 0; i < envelope.size(); ++i) {
        float sum = 0.0f;
        int count = 0;
        
        for (int j = -smoothingWindow; j <= smoothingWindow; ++j) {
            int idx = i + j;
            if (idx >= 0 && idx < static_cast<int>(envelope.size())) {
                sum += envelope[idx];
                count++;
            }
        }
        
        smoothed[i] = count > 0 ? sum / count : envelope[i];
    }
    
    return smoothed;
}

std::vector<int> AcousticnessAnalyzer::detectOnsetPoints(const std::vector<float>& envelope) {
    std::vector<int> onsets;
    
    // Calculate first derivative (velocity)
    std::vector<float> velocity(envelope.size() - 1);
    for (size_t i = 0; i < envelope.size() - 1; ++i) {
        velocity[i] = envelope[i + 1] - envelope[i];
    }
    
    // Find local maxima in velocity (acceleration peaks)
    const float threshold = 0.001f; // Minimum velocity threshold
    
    for (size_t i = 1; i < velocity.size() - 1; ++i) {
        if (velocity[i] > threshold &&
            velocity[i] > velocity[i - 1] &&
            velocity[i] > velocity[i + 1]) {
            
            // Ensure minimum distance between onsets (100ms)
            if (onsets.empty() || i - onsets.back() > 20) {
                onsets.push_back(i);
            }
        }
    }
    
    return onsets;
}

AcousticnessAnalyzer::AttackProfile AcousticnessAnalyzer::analyzeAttack(const std::vector<float>& envelope, int onsetIdx) {
    AttackProfile profile;
    
    // Find attack duration (10% to 90% of peak)
    int peakIdx = onsetIdx;
    float peakValue = envelope[onsetIdx];
    
    // Find peak within 100ms of onset
    for (int i = onsetIdx; i < std::min(onsetIdx + 50, static_cast<int>(envelope.size())); ++i) {
        if (envelope[i] > peakValue) {
            peakValue = envelope[i];
            peakIdx = i;
        }
    }
    
    // Find 10% and 90% points
    float threshold10 = peakValue * 0.1f;
    float threshold90 = peakValue * 0.9f;
    
    int idx10 = onsetIdx;
    int idx90 = peakIdx;
    
    for (int i = onsetIdx; i <= peakIdx; ++i) {
        if (envelope[i] >= threshold10 && idx10 == onsetIdx) {
            idx10 = i;
        }
        if (envelope[i] >= threshold90) {
            idx90 = i;
            break;
        }
    }
    
    profile.duration = (idx90 - idx10) * 0.0005f; // Convert to seconds (2ms hop)
    profile.slope = (envelope[idx90] - envelope[idx10]) / (profile.duration + 1e-6f);
    profile.sharpness = 1.0f / (profile.duration + 0.001f); // Inverse of duration
    
    return profile;
}

AcousticnessAnalyzer::DecayProfile AcousticnessAnalyzer::analyzeDecay(const std::vector<float>& envelope, int peakIdx) {
    DecayProfile profile;
    
    if (peakIdx >= static_cast<int>(envelope.size()) - 10) {
        profile.duration = 0.1f;
        profile.rate = 10.0f;
        profile.type = "unknown";
        return profile;
    }
    
    float peakValue = envelope[peakIdx];
    float threshold60 = peakValue * 0.4f; // -60% of peak
    
    // Find decay time
    int decayIdx = peakIdx;
    for (int i = peakIdx + 1; i < static_cast<int>(envelope.size()); ++i) {
        if (envelope[i] <= threshold60) {
            decayIdx = i;
            break;
        }
    }
    
    profile.duration = (decayIdx - peakIdx) * 0.0005f; // Convert to seconds
    
    // Fit exponential decay model
    float sumXY = 0.0f, sumX = 0.0f, sumY = 0.0f, sumX2 = 0.0f;
    int points = 0;
    
    for (int i = peakIdx; i <= decayIdx && i < static_cast<int>(envelope.size()); ++i) {
        if (envelope[i] > 0) {
            float x = (i - peakIdx) * 0.0005f;
            float y = std::log(envelope[i] / peakValue);
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
            points++;
        }
    }
    
    if (points > 2 && sumX2 * points - sumX * sumX != 0) {
        profile.rate = (sumXY * points - sumX * sumY) / (sumX2 * points - sumX * sumX);
        profile.rate = std::abs(profile.rate);
    } else {
        profile.rate = 10.0f; // Default fast decay
    }
    
    // Classify decay type
    if (profile.rate < 5.0f) {
        profile.type = "sustained";
    } else if (profile.rate < 20.0f) {
        profile.type = "natural";
    } else {
        profile.type = "percussive";
    }
    
    return profile;
}

float AcousticnessAnalyzer::scoreAcousticCharacteristics(const AttackProfile& attack, const DecayProfile& decay) {
    float score = 0.0f;
    
    // Acoustic instruments typically have:
    // 1. Moderate to fast attack (5-50ms)
    if (attack.duration >= 0.005f && attack.duration <= 0.05f) {
        score += 0.3f;
    } else if (attack.duration < 0.005f) {
        score += 0.2f; // Very fast (percussive)
    }
    
    // 2. Natural decay characteristics
    if (decay.type == "natural") {
        score += 0.4f;
    } else if (decay.type == "sustained") {
        score += 0.2f; // Could be bowed strings
    }
    
    // 3. Specific attack sharpness
    if (attack.sharpness > 20.0f && attack.sharpness < 200.0f) {
        score += 0.3f;
    }
    
    return std::min(1.0f, score);
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
    // Complete pitch tracking with YIN algorithm for accurate intonation analysis
    const int windowSize = static_cast<int>(0.05f * audio.sampleRate); // 50ms windows
    const int hopSize = windowSize / 4; // 75% overlap
    const float minFreq = 80.0f;  // Minimum speech frequency
    const float maxFreq = 400.0f; // Maximum speech frequency
    
    std::vector<float> pitchContour;
    std::vector<float> pitchConfidence;
    
    // YIN algorithm implementation for pitch detection
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += hopSize) {
        // Extract window
        std::vector<float> window(audio.samples.begin() + i, 
                                 audio.samples.begin() + i + windowSize);
        
        // Apply window function
        for (size_t j = 0; j < window.size(); ++j) {
            window[j] *= 0.5f * (1.0f - std::cos(2.0f * M_PI * j / (window.size() - 1)));
        }
        
        // Calculate autocorrelation-based difference function
        std::vector<float> differenceFunction = calculateYinDifferenceFunction(window);
        
        // Cumulative mean normalized difference
        std::vector<float> cmndf = calculateCMNDF(differenceFunction);
        
        // Find pitch period
        int tau = findPitchPeriod(cmndf, audio.sampleRate, minFreq, maxFreq);
        
        if (tau > 0) {
            // Refine with parabolic interpolation
            float refinedTau = parabolicInterpolation(differenceFunction, tau);
            float pitch = audio.sampleRate / refinedTau;
            float confidence = 1.0f - cmndf[tau];
            
            pitchContour.push_back(pitch);
            pitchConfidence.push_back(confidence);
        } else {
            pitchContour.push_back(0.0f);
            pitchConfidence.push_back(0.0f);
        }
    }
    
    // Analyze pitch contour characteristics for speech
    return analyzeSpeechIntonation(pitchContour, pitchConfidence);
}

std::vector<float> SpeechinessDetector::calculateYinDifferenceFunction(const std::vector<float>& window) {
    int halfSize = window.size() / 2;
    std::vector<float> diff(halfSize, 0.0f);
    
    for (int tau = 1; tau < halfSize; ++tau) {
        for (int j = 0; j < halfSize; ++j) {
            float delta = window[j] - window[j + tau];
            diff[tau] += delta * delta;
        }
    }
    
    return diff;
}

std::vector<float> SpeechinessDetector::calculateCMNDF(const std::vector<float>& diff) {
    std::vector<float> cmndf(diff.size());
    cmndf[0] = 1.0f;
    
    float runningSum = 0.0f;
    for (size_t tau = 1; tau < diff.size(); ++tau) {
        runningSum += diff[tau];
        cmndf[tau] = runningSum > 0 ? diff[tau] * tau / runningSum : 1.0f;
    }
    
    return cmndf;
}

int SpeechinessDetector::findPitchPeriod(const std::vector<float>& cmndf, 
                                        float sampleRate, float minFreq, float maxFreq) {
    const float threshold = 0.3f; // YIN threshold
    
    int minPeriod = static_cast<int>(sampleRate / maxFreq);
    int maxPeriod = static_cast<int>(sampleRate / minFreq);
    
    // Find first minimum below threshold
    for (int tau = minPeriod; tau < std::min(maxPeriod, static_cast<int>(cmndf.size())); ++tau) {
        if (cmndf[tau] < threshold) {
            // Check if it's a local minimum
            if (tau > 0 && tau < static_cast<int>(cmndf.size()) - 1) {
                if (cmndf[tau] < cmndf[tau - 1] && cmndf[tau] < cmndf[tau + 1]) {
                    return tau;
                }
            }
        }
    }
    
    // If no minimum found below threshold, find absolute minimum
    int minTau = minPeriod;
    float minValue = cmndf[minPeriod];
    
    for (int tau = minPeriod + 1; tau < std::min(maxPeriod, static_cast<int>(cmndf.size())); ++tau) {
        if (cmndf[tau] < minValue) {
            minValue = cmndf[tau];
            minTau = tau;
        }
    }
    
    return minValue < 0.5f ? minTau : 0; // Only return if confident enough
}

float SpeechinessDetector::parabolicInterpolation(const std::vector<float>& diff, int tau) {
    if (tau <= 0 || tau >= static_cast<int>(diff.size()) - 1) return tau;
    
    float s0 = diff[tau - 1];
    float s1 = diff[tau];
    float s2 = diff[tau + 1];
    
    float a = s2 - s1;
    float b = s0 - s1;
    
    if (a + b == 0) return tau;
    
    return tau + 0.5f * (b - a) / (a + b);
}

float SpeechinessDetector::analyzeSpeechIntonation(const std::vector<float>& pitchContour,
                                                  const std::vector<float>& confidence) {
    if (pitchContour.empty()) return 0.0f;
    
    // Filter out unvoiced segments
    std::vector<float> voicedPitches;
    for (size_t i = 0; i < pitchContour.size(); ++i) {
        if (confidence[i] > 0.5f && pitchContour[i] > 0) {
            voicedPitches.push_back(pitchContour[i]);
        }
    }
    
    if (voicedPitches.size() < 10) return 0.0f;
    
    // Calculate intonation statistics
    float meanPitch = std::accumulate(voicedPitches.begin(), voicedPitches.end(), 0.0f) / voicedPitches.size();
    
    // Convert to semitones for perceptual scale
    std::vector<float> semitones;
    for (float pitch : voicedPitches) {
        semitones.push_back(12.0f * std::log2(pitch / meanPitch));
    }
    
    // Calculate pitch range and variation
    float minSemitone = *std::min_element(semitones.begin(), semitones.end());
    float maxSemitone = *std::max_element(semitones.begin(), semitones.end());
    float pitchRange = maxSemitone - minSemitone;
    
    // Calculate pitch change rate
    float totalChange = 0.0f;
    for (size_t i = 1; i < semitones.size(); ++i) {
        totalChange += std::abs(semitones[i] - semitones[i-1]);
    }
    float changeRate = totalChange / (semitones.size() - 1);
    
    // Speech characteristics:
    // - Moderate pitch range (4-12 semitones typical)
    // - Regular pitch changes (not monotone, not wildly varying)
    float score = 0.0f;
    
    if (pitchRange >= 4.0f && pitchRange <= 12.0f) {
        score += 0.4f;
    } else if (pitchRange > 2.0f && pitchRange < 20.0f) {
        score += 0.2f;
    }
    
    if (changeRate >= 0.5f && changeRate <= 3.0f) {
        score += 0.4f;
    } else if (changeRate > 0.2f && changeRate < 5.0f) {
        score += 0.2f;
    }
    
    // Check for prosodic patterns (rising/falling at phrase boundaries)
    float prosodyScore = analyzeProsody(semitones);
    score += prosodyScore * 0.2f;
    
    return std::min(1.0f, score);
}

float SpeechinessDetector::analyzeProsody(const std::vector<float>& semitones) {
    if (semitones.size() < 20) return 0.5f;
    
    // Detect phrase boundaries by looking for pauses or pitch resets
    std::vector<int> phraseBoundaries;
    phraseBoundaries.push_back(0);
    
    // Simple phrase detection: large pitch jumps or returns to baseline
    float baseline = std::accumulate(semitones.begin(), semitones.end(), 0.0f) / semitones.size();
    
    for (size_t i = 10; i < semitones.size() - 10; ++i) {
        // Check for return to baseline
        if (std::abs(semitones[i] - baseline) < 1.0f && 
            std::abs(semitones[i-1] - baseline) > 2.0f) {
            phraseBoundaries.push_back(i);
        }
    }
    phraseBoundaries.push_back(semitones.size() - 1);
    
    // Analyze phrase endings
    float typicalEndingScore = 0.0f;
    int validPhrases = 0;
    
    for (size_t i = 1; i < phraseBoundaries.size(); ++i) {
        int start = phraseBoundaries[i-1];
        int end = phraseBoundaries[i];
        
        if (end - start > 5) {
            // Check for typical speech prosody patterns
            float startPitch = semitones[start];
            float endPitch = semitones[end];
            
            // Falling intonation at phrase end (declarative)
            if (endPitch < startPitch - 2.0f) {
                typicalEndingScore += 1.0f;
            }
            // Rising intonation (interrogative)
            else if (endPitch > startPitch + 2.0f) {
                typicalEndingScore += 0.8f;
            }
            
            validPhrases++;
        }
    }
    
    return validPhrases > 0 ? typicalEndingScore / validPhrases : 0.5f;
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
    // Complete RT60 estimation using Schroeder backward integration method
    const int windowSize = static_cast<int>(0.005f * audio.sampleRate); // 5ms for fine resolution
    const int hopSize = windowSize / 2; // 50% overlap
    
    // Step 1: Detect impulse-like events in the signal
    std::vector<int> impulseIndices = detectImpulses(audio);
    if (impulseIndices.empty()) {
        // No clear impulses, estimate from overall decay
        return estimateRT60FromDecay(audio);
    }
    
    // Step 2: For each impulse, calculate the decay curve
    std::vector<float> rt60Estimates;
    
    for (int impulseIdx : impulseIndices) {
        if (impulseIdx + audio.sampleRate * 2 > (int)audio.samples.size()) continue;
        
        // Extract decay portion after impulse
        int startIdx = impulseIdx;
        int endIdx = std::min(impulseIdx + audio.sampleRate * 2, (int)audio.samples.size());
        
        std::vector<float> decaySegment(audio.samples.begin() + startIdx, 
                                       audio.samples.begin() + endIdx);
        
        // Calculate energy decay curve
        std::vector<float> energyCurve = calculateEnergyCurve(decaySegment, windowSize, hopSize);
        
        // Apply Schroeder backward integration
        std::vector<float> schroederCurve = schroederBackwardIntegration(energyCurve);
        
        // Fit linear regression to find RT60
        float rt60 = fitRT60(schroederCurve, hopSize / (float)audio.sampleRate);
        
        if (rt60 > 0.05f && rt60 < 10.0f) { // Reasonable RT60 range
            rt60Estimates.push_back(rt60);
        }
    }
    
    if (rt60Estimates.empty()) {
        return estimateRT60FromDecay(audio);
    }
    
    // Return median of estimates
    std::sort(rt60Estimates.begin(), rt60Estimates.end());
    return rt60Estimates[rt60Estimates.size() / 2];
}

std::vector<int> LivenessDetector::detectImpulses(const AudioBuffer& audio) {
    std::vector<int> impulses;
    const int windowSize = static_cast<int>(0.01f * audio.sampleRate); // 10ms
    
    // Calculate short-term energy
    std::vector<float> energy;
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += windowSize/2) {
        float e = 0.0f;
        for (int j = 0; j < windowSize; ++j) {
            e += audio.samples[i+j] * audio.samples[i+j];
        }
        energy.push_back(e / windowSize);
    }
    
    if (energy.size() < 3) return impulses;
    
    // Find local maxima with high energy
    float meanEnergy = std::accumulate(energy.begin(), energy.end(), 0.0f) / energy.size();
    float threshold = meanEnergy * 4.0f; // Peaks 4x above average
    
    for (size_t i = 1; i < energy.size() - 1; ++i) {
        if (energy[i] > threshold &&
            energy[i] > energy[i-1] &&
            energy[i] > energy[i+1]) {
            
            int sampleIdx = i * windowSize / 2;
            
            // Ensure minimum distance between impulses (500ms)
            if (impulses.empty() || sampleIdx - impulses.back() > audio.sampleRate * 0.5f) {
                impulses.push_back(sampleIdx);
            }
        }
    }
    
    return impulses;
}

std::vector<float> LivenessDetector::calculateEnergyCurve(const std::vector<float>& signal,
                                                         int windowSize, int hopSize) {
    std::vector<float> energy;
    
    for (int i = 0; i <= (int)signal.size() - windowSize; i += hopSize) {
        float e = 0.0f;
        for (int j = 0; j < windowSize; ++j) {
            e += signal[i+j] * signal[i+j];
        }
        energy.push_back(e);
    }
    
    return energy;
}

std::vector<float> LivenessDetector::schroederBackwardIntegration(const std::vector<float>& energy) {
    std::vector<float> schroeder(energy.size());
    
    // Backward integration: sum energy from end to beginning
    float totalEnergy = std::accumulate(energy.begin(), energy.end(), 0.0f);
    
    if (totalEnergy <= 0) return schroeder;
    
    float cumulativeEnergy = 0.0f;
    for (int i = energy.size() - 1; i >= 0; --i) {
        cumulativeEnergy += energy[i];
        schroeder[i] = 10.0f * std::log10(cumulativeEnergy / totalEnergy + 1e-10f);
    }
    
    return schroeder;
}

float LivenessDetector::fitRT60(const std::vector<float>& schroederCurve, float timeStep) {
    if (schroederCurve.size() < 10) return 0.1f;
    
    // Find -5dB and -35dB points for robust estimation (avoiding noise floor)
    int idx5dB = -1, idx35dB = -1;
    
    for (size_t i = 0; i < schroederCurve.size(); ++i) {
        if (idx5dB < 0 && schroederCurve[i] <= -5.0f) {
            idx5dB = i;
        }
        if (idx35dB < 0 && schroederCurve[i] <= -35.0f) {
            idx35dB = i;
            break;
        }
    }
    
    if (idx5dB < 0 || idx35dB < 0 || idx35dB <= idx5dB) {
        // Fallback: use available range
        idx5dB = schroederCurve.size() / 10;
        idx35dB = schroederCurve.size() / 2;
    }
    
    // Linear regression between -5dB and -35dB
    float sumX = 0.0f, sumY = 0.0f, sumXY = 0.0f, sumX2 = 0.0f;
    int count = 0;
    
    for (int i = idx5dB; i <= idx35dB; ++i) {
        float x = i * timeStep;
        float y = schroederCurve[i];
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
        count++;
    }
    
    if (count < 2 || sumX2 * count - sumX * sumX == 0) return 0.1f;
    
    // Calculate slope (dB/s)
    float slope = (sumXY * count - sumX * sumY) / (sumX2 * count - sumX * sumX);
    
    if (slope >= 0) return 0.1f; // No decay
    
    // RT60 is time to decay 60dB
    float rt60 = -60.0f / slope;
    
    return std::max(0.05f, std::min(10.0f, rt60)); // Clamp to reasonable range
}

float LivenessDetector::estimateRT60FromDecay(const AudioBuffer& audio) {
    // Fallback method using overall energy decay
    const int blockSize = static_cast<int>(0.1f * audio.sampleRate); // 100ms blocks
    std::vector<float> blockEnergy;
    
    for (int i = 0; i <= (int)audio.samples.size() - blockSize; i += blockSize) {
        float energy = 0.0f;
        for (int j = 0; j < blockSize; ++j) {
            energy += audio.samples[i+j] * audio.samples[i+j];
        }
        blockEnergy.push_back(10.0f * std::log10(energy / blockSize + 1e-10f));
    }
    
    if (blockEnergy.size() < 5) return 0.1f;
    
    // Find peak and measure decay
    auto maxIt = std::max_element(blockEnergy.begin(), blockEnergy.end());
    int peakIdx = std::distance(blockEnergy.begin(), maxIt);
    
    if (peakIdx >= (int)blockEnergy.size() - 2) return 0.1f;
    
    // Find where energy drops 20dB from peak
    float peak = *maxIt;
    float threshold20dB = peak - 20.0f;
    
    int decayIdx = -1;
    for (int i = peakIdx + 1; i < (int)blockEnergy.size(); ++i) {
        if (blockEnergy[i] <= threshold20dB) {
            decayIdx = i;
            break;
        }
    }
    
    if (decayIdx < 0) return 0.1f;
    
    // Extrapolate to 60dB
    float time20dB = (decayIdx - peakIdx) * 0.1f; // 100ms blocks
    float rt60 = time20dB * 3.0f; // 20dB * 3 = 60dB
    
    return std::max(0.05f, std::min(2.0f, rt60));
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

// DanceabilityAnalyzer implementations moved to ai_algorithms_part2.cpp
#if 0
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

// DanceabilityAnalyzer helper methods moved to ai_algorithms_part2.cpp

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
#endif

// isOptimalDanceTempo moved to ai_algorithms_part2.cpp

// ========================================
// 😊 AI_VALENCE - Musical Positivity
// ========================================

// ValenceAnalyzer implementations moved to ai_algorithms_part2.cpp
#if 0
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

// ValenceAnalyzer helper methods moved to ai_algorithms_part2.cpp

float ValenceAnalyzer::analyzeMelodicPositivity(const AudioBuffer& audio) {
    // Complete melodic contour analysis with pitch tracking and trajectory analysis
    const int windowSize = static_cast<int>(0.05f * audio.sampleRate); // 50ms windows
    const int hopSize = windowSize / 4;
    
    // Extract melodic contour using enhanced pitch tracking
    std::vector<MelodicSegment> melodicSegments = extractMelodicSegments(audio, windowSize, hopSize);
    
    if (melodicSegments.empty()) {
        // Fallback to spectral analysis
        SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
        return features.spectralCentroid / 4000.0f;
    }
    
    // Analyze melodic characteristics
    float positivityScore = 0.0f;
    float totalWeight = 0.0f;
    
    for (const auto& segment : melodicSegments) {
        // 1. Analyze melodic direction (ascending = positive)
        float directionScore = analyzeMelodicDirection(segment);
        positivityScore += directionScore * segment.confidence * 0.3f;
        
        // 2. Analyze interval patterns (major intervals = positive)
        float intervalScore = analyzeIntervalPositivity(segment);
        positivityScore += intervalScore * segment.confidence * 0.3f;
        
        // 3. Analyze melodic stability (stable = positive)
        float stabilityScore = analyzeMelodicStability(segment);
        positivityScore += stabilityScore * segment.confidence * 0.2f;
        
        // 4. Analyze pitch range (moderate range = positive)
        float rangeScore = analyzePitchRange(segment);
        positivityScore += rangeScore * segment.confidence * 0.2f;
        
        totalWeight += segment.confidence;
    }
    
    if (totalWeight > 0) {
        positivityScore /= totalWeight;
    }
    
    return std::min(1.0f, positivityScore);
}

std::vector<ValenceAnalyzer::MelodicSegment> ValenceAnalyzer::extractMelodicSegments(
    const AudioBuffer& audio, int windowSize, int hopSize) {
    
    std::vector<MelodicSegment> segments;
    
    // Use monophonic pitch detection (simplified autocorrelation)
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += hopSize) {
        std::vector<float> window(audio.samples.begin() + i,
                                 audio.samples.begin() + i + windowSize);
        
        // Apply window function
        for (size_t j = 0; j < window.size(); ++j) {
            window[j] *= 0.5f * (1.0f - std::cos(2.0f * M_PI * j / (window.size() - 1)));
        }
        
        // Detect pitch using autocorrelation
        float pitch = detectPitchAutocorrelation(window, audio.sampleRate);
        float confidence = calculatePitchConfidence(window, pitch, audio.sampleRate);
        
        if (pitch > 0 && confidence > 0.5f) {
            if (!segments.empty() && 
                std::abs(pitch - segments.back().pitches.back()) < 50.0f) {
                // Continue existing segment
                segments.back().pitches.push_back(pitch);
                segments.back().confidences.push_back(confidence);
            } else if (segments.empty() || segments.back().pitches.size() >= 5) {
                // Start new segment
                MelodicSegment newSegment;
                newSegment.pitches.push_back(pitch);
                newSegment.confidences.push_back(confidence);
                newSegment.startTime = i / (float)audio.sampleRate;
                segments.push_back(newSegment);
            }
        }
    }
    
    // Calculate segment-level statistics
    for (auto& segment : segments) {
        if (segment.pitches.size() >= 3) {
            segment.confidence = std::accumulate(segment.confidences.begin(),
                                               segment.confidences.end(), 0.0f) / 
                                segment.confidences.size();
            segment.duration = segment.pitches.size() * hopSize / (float)audio.sampleRate;
        }
    }
    
    // Filter out very short segments
    segments.erase(
        std::remove_if(segments.begin(), segments.end(),
                      [](const MelodicSegment& s) { return s.pitches.size() < 3; }),
        segments.end()
    );
    
    return segments;
}

float ValenceAnalyzer::detectPitchAutocorrelation(const std::vector<float>& window, float sampleRate) {
    const float minFreq = 80.0f;
    const float maxFreq = 1000.0f;
    
    int minLag = static_cast<int>(sampleRate / maxFreq);
    int maxLag = static_cast<int>(sampleRate / minFreq);
    
    std::vector<float> autocorr(maxLag - minLag + 1);
    
    // Calculate autocorrelation
    for (int lag = minLag; lag <= maxLag; ++lag) {
        float sum = 0.0f;
        for (size_t i = 0; i < window.size() - lag; ++i) {
            sum += window[i] * window[i + lag];
        }
        autocorr[lag - minLag] = sum;
    }
    
    // Find peak
    auto maxIt = std::max_element(autocorr.begin(), autocorr.end());
    if (*maxIt <= 0) return 0.0f;
    
    int peakLag = std::distance(autocorr.begin(), maxIt) + minLag;
    return sampleRate / peakLag;
}

float ValenceAnalyzer::calculatePitchConfidence(const std::vector<float>& window, 
                                               float pitch, float sampleRate) {
    if (pitch <= 0) return 0.0f;
    
    // Simple confidence based on periodicity strength
    int period = static_cast<int>(sampleRate / pitch);
    if (period >= (int)window.size() / 2) return 0.0f;
    
    float correlation = 0.0f;
    int count = 0;
    
    for (size_t i = 0; i < window.size() - period; ++i) {
        correlation += window[i] * window[i + period];
        count++;
    }
    
    if (count == 0) return 0.0f;
    
    // Normalize
    float energy1 = 0.0f, energy2 = 0.0f;
    for (size_t i = 0; i < window.size() - period; ++i) {
        energy1 += window[i] * window[i];
        energy2 += window[i + period] * window[i + period];
    }
    
    if (energy1 <= 0 || energy2 <= 0) return 0.0f;
    
    return correlation / std::sqrt(energy1 * energy2);
}

float ValenceAnalyzer::analyzeMelodicDirection(const MelodicSegment& segment) {
    if (segment.pitches.size() < 3) return 0.5f;
    
    // Calculate overall melodic trajectory
    float totalChange = 0.0f;
    float upwardMovement = 0.0f;
    
    for (size_t i = 1; i < segment.pitches.size(); ++i) {
        float change = segment.pitches[i] - segment.pitches[i-1];
        totalChange += std::abs(change);
        if (change > 0) {
            upwardMovement += change;
        }
    }
    
    if (totalChange <= 0) return 0.5f;
    
    // More upward movement = more positive
    float directionRatio = upwardMovement / totalChange;
    return 0.3f + 0.4f * directionRatio; // Scale to 0.3-0.7 range
}

float ValenceAnalyzer::analyzeIntervalPositivity(const MelodicSegment& segment) {
    if (segment.pitches.size() < 2) return 0.5f;
    
    float positiveIntervals = 0.0f;
    float totalIntervals = 0.0f;
    
    for (size_t i = 1; i < segment.pitches.size(); ++i) {
        // Convert to semitones
        float interval = 12.0f * std::log2(segment.pitches[i] / segment.pitches[i-1]);
        float absInterval = std::abs(interval);
        
        // Classify interval positivity
        // Major 3rd (4), Perfect 5th (7), Major 6th (9), Octave (12)
        if (std::abs(absInterval - 4.0f) < 0.5f ||  // Major 3rd
            std::abs(absInterval - 7.0f) < 0.5f ||  // Perfect 5th
            std::abs(absInterval - 9.0f) < 0.5f ||  // Major 6th
            std::abs(absInterval - 12.0f) < 0.5f) { // Octave
            positiveIntervals += 1.0f;
        }
        // Minor intervals are less positive but not negative
        else if (std::abs(absInterval - 3.0f) < 0.5f ||  // Minor 3rd
                 std::abs(absInterval - 8.0f) < 0.5f) {  // Minor 6th
            positiveIntervals += 0.5f;
        }
        
        totalIntervals += 1.0f;
    }
    
    return totalIntervals > 0 ? positiveIntervals / totalIntervals : 0.5f;
}

float ValenceAnalyzer::analyzeMelodicStability(const MelodicSegment& segment) {
    if (segment.pitches.size() < 3) return 0.5f;
    
    // Calculate pitch variance
    float mean = std::accumulate(segment.pitches.begin(), segment.pitches.end(), 0.0f) / 
                 segment.pitches.size();
    
    float variance = 0.0f;
    for (float pitch : segment.pitches) {
        variance += (pitch - mean) * (pitch - mean);
    }
    variance /= segment.pitches.size();
    
    // Convert to semitones for perceptual scaling
    float stdDevSemitones = 12.0f * std::log2(1.0f + std::sqrt(variance) / mean);
    
    // Moderate variation is most positive (not too static, not too wild)
    if (stdDevSemitones < 2.0f) {
        return 0.4f; // Too static
    } else if (stdDevSemitones < 4.0f) {
        return 0.8f; // Optimal range
    } else if (stdDevSemitones < 8.0f) {
        return 0.6f; // Getting unstable
    } else {
        return 0.3f; // Too unstable
    }
}

float ValenceAnalyzer::analyzePitchRange(const MelodicSegment& segment) {
    if (segment.pitches.empty()) return 0.5f;
    
    float minPitch = *std::min_element(segment.pitches.begin(), segment.pitches.end());
    float maxPitch = *std::max_element(segment.pitches.begin(), segment.pitches.end());
    
    // Convert to semitones
    float rangeSemitones = 12.0f * std::log2(maxPitch / minPitch);
    
    // Moderate range is most positive
    if (rangeSemitones < 6.0f) {
        return 0.5f; // Limited range
    } else if (rangeSemitones < 12.0f) {
        return 0.8f; // Optimal range (within octave)
    } else if (rangeSemitones < 18.0f) {
        return 0.6f; // Wide range
    } else {
        return 0.4f; // Very wide range
    }
}

// analyzeTempoFactor moved to ai_algorithms_part2.cpp

float ValenceAnalyzer::analyzeTimbralBrightness(const SpectralFeatures& features) {
    // Bright timbres are generally more positive
    float brightness = features.spectralCentroid / 4000.0f; // Normalize to ~0-1
    return std::min(1.0f, brightness);
}

// calculateConsonanceDissonance moved to ai_algorithms_part2.cpp

float ValenceAnalyzer::analyzeMelodicContour(const AudioBuffer& audio) {
    // Complete melodic contour analysis - delegates to analyzeMelodicPositivity
    return analyzeMelodicPositivity(audio);
}
#endif

// ========================================
// 🎼 AI_MODE - Major/Minor Detection
// ========================================

// ModeDetector implementations moved to ai_algorithms_part2.cpp
#if 0
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
#endif

// ========================================
// 🎵 AI_TIME_SIGNATURE - Meter Detection
// ========================================

// TimeSignatureDetector implementations moved to ai_algorithms_part2.cpp
#if 0
int TimeSignatureDetector::detectTimeSignature(const AudioBuffer& audio) {
    BeatVector beats = detectBeats(audio);
    std::vector<float> accentPattern = analyzeAccentPattern(beats);
    return analyzeMeter(accentPattern);
}

// TimeSignatureDetector::detectBeats moved to ai_algorithms_base.cpp

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
#endif

// TimeSignatureDetector helper methods moved to ai_algorithms_part2.cpp
#if 0
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
#endif

// ========================================
// 🎨 AI_CHARACTERISTICS - Timbral Features
// ========================================

// CharacteristicsExtractor implementations moved to ai_algorithms_part2.cpp
#if 0
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
#endif

// ========================================
// 📊 AI_CONFIDENCE - Quality Assessment
// ========================================

// ConfidenceCalculator implementations moved to ai_algorithms_part3.cpp
#if 0
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
#endif

// ========================================
// 🎭 GENRE & MOOD CLASSIFICATION
// ========================================

// GenreClassifier and MoodAnalyzer implementations moved to ai_algorithms_part3.cpp
#if 0
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

std::string GenreClassifier::analyzeCulturalContext(const AudioBuffer& audio, const AIAnalysisResult& features) {
    // Complete cultural context analysis using multiple cultural indicators
    
    // Extract cultural features
    CulturalFeatures cultural = extractCulturalFeatures(audio, features);
    
    // Analyze regional characteristics
    std::string regionalStyle = analyzeRegionalCharacteristics(cultural);
    
    // Analyze temporal cultural context
    std::string temporalContext = analyzeTemporalContext(cultural, features.AI_ERA);
    
    // Analyze production culture
    std::string productionCulture = analyzeProductionCulture(cultural);
    
    // Combine analyses for comprehensive cultural context
    return synthesizeCulturalContext(regionalStyle, temporalContext, productionCulture, cultural);
}

GenreClassifier::CulturalFeatures GenreClassifier::extractCulturalFeatures(
    const AudioBuffer& audio, const AIAnalysisResult& features) {
    
    CulturalFeatures cultural;
    
    // Rhythmic patterns analysis
    cultural.rhythmicPatterns = analyzeRhythmicPatterns(audio);
    
    // Scale and mode analysis
    cultural.scaleType = analyzeScaleType(audio, features);
    
    // Instrumentation analysis
    cultural.instrumentationType = analyzeInstrumentationType(audio, features);
    
    // Production style indicators
    cultural.productionEra = features.AI_ERA;
    cultural.acousticness = features.AI_ACOUSTICNESS;
    cultural.energy = features.AI_ENERGY;
    cultural.valence = features.AI_VALENCE;
    cultural.danceability = features.AI_DANCEABILITY;
    
    // Melodic characteristics
    AudioProcessor processor;
    ChromaVector chroma = processor.calculateChroma(audio);
    cultural.melodicComplexity = analyzeMelodicComplexity(chroma);
    
    // Harmonic characteristics
    cultural.harmonicComplexity = analyzeHarmonicComplexity(chroma, features);
    
    return cultural;
}

std::string GenreClassifier::analyzeRhythmicPatterns(const AudioBuffer& audio) {
    // Analyze rhythm for cultural patterns
    TimeSignatureDetector tsDetector;
    BeatVector beats = tsDetector.detectBeats(audio);
    
    if (beats.beatTimes.empty()) return "undefined";
    
    // Calculate inter-beat intervals
    std::vector<float> intervals;
    for (size_t i = 1; i < beats.beatTimes.size(); ++i) {
        intervals.push_back(beats.beatTimes[i] - beats.beatTimes[i-1]);
    }
    
    if (intervals.empty()) return "undefined";
    
    // Analyze rhythm regularity
    float mean = std::accumulate(intervals.begin(), intervals.end(), 0.0f) / intervals.size();
    float variance = 0.0f;
    for (float interval : intervals) {
        variance += (interval - mean) * (interval - mean);
    }
    variance /= intervals.size();
    float cv = std::sqrt(variance) / mean; // Coefficient of variation
    
    // Analyze syncopation
    int syncopations = 0;
    for (size_t i = 0; i < beats.beatStrengths.size(); ++i) {
        if (i % 4 == 1 || i % 4 == 3) { // Weak beats
            if (beats.beatStrengths[i] > 0.7f) {
                syncopations++;
            }
        }
    }
    float syncopationRate = (float)syncopations / beats.beatStrengths.size();
    
    // Classify rhythm pattern
    if (cv < 0.05f && syncopationRate < 0.1f) {
        return "straight";  // Western pop/rock
    } else if (cv < 0.1f && syncopationRate > 0.3f) {
        return "syncopated"; // Jazz, funk, R&B influence
    } else if (cv > 0.15f && syncopationRate > 0.2f) {
        return "complex";    // Latin, African, or experimental
    } else if (cv > 0.1f && syncopationRate < 0.2f) {
        return "swing";      // Jazz or blues influence
    }
    
    return "hybrid";
}

std::string GenreClassifier::analyzeScaleType(const AudioBuffer& audio, const AIAnalysisResult& features) {
    AudioProcessor processor;
    ChromaVector chroma = processor.calculateChroma(audio);
    
    // Define scale patterns (relative to tonic)
    std::vector<float> majorScale = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    std::vector<float> minorScale = {1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    std::vector<float> pentatonic = {1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    std::vector<float> blues = {1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f};
    std::vector<float> chromatic = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    
    // Find best matching scale
    float bestScore = 0.0f;
    std::string bestScale = "undefined";
    
    auto testScale = [&](const std::vector<float>& scale, const std::string& name) {
        for (int shift = 0; shift < 12; ++shift) {
            float score = 0.0f;
            for (int i = 0; i < 12; ++i) {
                int idx = (i + shift) % 12;
                score += chroma.chroma[i] * scale[idx];
            }
            if (score > bestScore) {
                bestScore = score;
                bestScale = name;
            }
        }
    };
    
    testScale(majorScale, "major");
    testScale(minorScale, "minor");
    testScale(pentatonic, "pentatonic");
    testScale(blues, "blues");
    
    // Check for chromaticism
    int activeNotes = 0;
    for (float c : chroma.chroma) {
        if (c > 0.1f) activeNotes++;
    }
    if (activeNotes > 9) {
        bestScale = "chromatic";
    }
    
    return bestScale;
}

std::string GenreClassifier::analyzeInstrumentationType(const AudioBuffer& audio, const AIAnalysisResult& features) {
    // Analyze instrumentation characteristics
    if (features.AI_ACOUSTICNESS > 0.8f) {
        if (features.AI_INSTRUMENTALNESS > 0.8f) {
            return "acoustic_instrumental";
        } else {
            return "acoustic_vocal";
        }
    } else if (features.AI_ACOUSTICNESS < 0.3f) {
        if (features.AI_ENERGY > 0.7f) {
            return "electronic_energetic";
        } else {
            return "electronic_ambient";
        }
    } else {
        return "hybrid";
    }
}

float GenreClassifier::analyzeMelodicComplexity(const ChromaVector& chroma) {
    // Calculate entropy of pitch class distribution
    float entropy = 0.0f;
    float total = std::accumulate(chroma.chroma.begin(), chroma.chroma.end(), 0.0f);
    
    if (total > 0) {
        for (float c : chroma.chroma) {
            if (c > 0) {
                float p = c / total;
                entropy -= p * std::log2(p);
            }
        }
    }
    
    // Normalize to 0-1 range (max entropy for 12 equally likely notes is log2(12) ≈ 3.58)
    return entropy / 3.58f;
}

float GenreClassifier::analyzeHarmonicComplexity(const ChromaVector& chroma, const AIAnalysisResult& features) {
    // Combine multiple indicators of harmonic complexity
    float complexity = 0.0f;
    
    // 1. Number of active pitch classes
    int activePitches = 0;
    for (float c : chroma.chroma) {
        if (c > 0.1f) activePitches++;
    }
    complexity += (float)activePitches / 12.0f * 0.3f;
    
    // 2. Mode complexity (minor is slightly more complex)
    if (features.AI_MODE == "minor") {
        complexity += 0.2f;
    }
    
    // 3. Consonance/dissonance analysis
    float consonance = 0.0f;
    // Major thirds and perfect fifths are consonant
    for (int i = 0; i < 12; i++) {
        int majorThird = (i + 4) % 12;
        int perfectFifth = (i + 7) % 12;
        consonance += chroma.chroma[i] * (chroma.chroma[majorThird] + chroma.chroma[perfectFifth]);
    }
    // Normalize
    float totalChroma = std::accumulate(chroma.chroma.begin(), chroma.chroma.end(), 0.0f);
    if (totalChroma > 0) {
        consonance = consonance / (totalChroma * totalChroma);
    }
    complexity += (1.0f - consonance) * 0.3f;
    
    // 4. Spectral complexity indicator
    complexity += analyzeMelodicComplexity(chroma) * 0.2f;
    
    return std::min(1.0f, complexity);
}

std::string GenreClassifier::analyzeRegionalCharacteristics(const CulturalFeatures& cultural) {
    // Map cultural features to regional styles
    
    if (cultural.scaleType == "pentatonic") {
        if (cultural.rhythmicPatterns == "straight" && cultural.acousticness > 0.7f) {
            return "East Asian influence";
        } else if (cultural.rhythmicPatterns == "syncopated") {
            return "African diaspora influence";
        }
    } else if (cultural.scaleType == "blues") {
        if (cultural.rhythmicPatterns == "swing" || cultural.rhythmicPatterns == "syncopated") {
            return "African-American tradition";
        }
    } else if (cultural.scaleType == "minor" && cultural.harmonicComplexity > 0.7f) {
        if (cultural.acousticness > 0.8f) {
            return "Eastern European influence";
        } else {
            return "Middle Eastern influence";
        }
    } else if (cultural.scaleType == "major") {
        if (cultural.rhythmicPatterns == "straight" && cultural.harmonicComplexity < 0.3f) {
            return "Western pop tradition";
        }
    }
    
    // Check for Latin influences
    if (cultural.rhythmicPatterns == "complex" && cultural.danceability > 0.7f) {
        return "Latin American influence";
    }
    
    // Electronic/global fusion
    if (cultural.instrumentationType.find("electronic") != std::string::npos &&
        cultural.harmonicComplexity > 0.5f) {
        return "Global electronic fusion";
    }
    
    return "Contemporary international";
}

std::string GenreClassifier::analyzeTemporalContext(const CulturalFeatures& cultural, const std::string& era) {
    // Map era and features to temporal cultural movements
    
    if (era == "1950s" || era == "1960s") {
        if (cultural.scaleType == "blues" || cultural.rhythmicPatterns == "swing") {
            return "Early rock and roll era";
        } else if (cultural.acousticness > 0.8f) {
            return "Folk revival movement";
        }
    } else if (era == "1970s") {
        if (cultural.energy > 0.7f && cultural.acousticness < 0.5f) {
            return "Progressive rock era";
        } else if (cultural.danceability > 0.7f) {
            return "Disco era influence";
        }
    } else if (era == "1980s") {
        if (cultural.acousticness < 0.3f) {
            return "Synthesizer revolution";
        } else if (cultural.energy > 0.8f) {
            return "MTV generation sound";
        }
    } else if (era == "1990s") {
        if (cultural.energy > 0.7f && cultural.valence < 0.4f) {
            return "Alternative/grunge movement";
        } else if (cultural.acousticness < 0.2f) {
            return "Electronic music explosion";
        }
    } else if (era == "2000s" || era == "2010s" || era == "Contemporary") {
        if (cultural.harmonicComplexity > 0.7f) {
            return "Post-modern eclecticism";
        } else if (cultural.acousticness < 0.2f && cultural.danceability > 0.7f) {
            return "EDM mainstream era";
        }
    }
    
    return "Contemporary fusion";
}

std::string GenreClassifier::analyzeProductionCulture(const CulturalFeatures& cultural) {
    // Analyze production aesthetics and culture
    
    if (cultural.acousticness > 0.8f) {
        if (cultural.productionEra == "Contemporary" || cultural.productionEra == "2010s") {
            return "Neo-acoustic movement";
        } else {
            return "Traditional acoustic production";
        }
    } else if (cultural.acousticness < 0.2f) {
        if (cultural.energy > 0.8f && cultural.danceability > 0.8f) {
            return "Club/festival production";
        } else if (cultural.energy < 0.4f) {
            return "Ambient/experimental production";
        } else {
            return "Studio electronic production";
        }
    } else {
        if (cultural.harmonicComplexity > 0.6f) {
            return "Art/progressive production";
        } else {
            return "Commercial crossover production";
        }
    }
}

std::string GenreClassifier::synthesizeCulturalContext(
    const std::string& regional, const std::string& temporal,
    const std::string& production, const CulturalFeatures& cultural) {
    
    // Create comprehensive cultural context description
    std::string context;
    
    // Primary influence
    if (regional.find("influence") != std::string::npos) {
        context = regional;
    } else if (temporal.find("movement") != std::string::npos || 
               temporal.find("era") != std::string::npos) {
        context = temporal;
    } else {
        context = production;
    }
    
    // Add secondary characteristics
    if (cultural.rhythmicPatterns == "complex" || cultural.rhythmicPatterns == "syncopated") {
        context += " with rhythmic sophistication";
    }
    
    if (cultural.harmonicComplexity > 0.7f) {
        context += " and harmonic complexity";
    }
    
    if (cultural.melodicComplexity > 0.7f) {
        context += ", featuring melodic intricacy";
    }
    
    // Add fusion elements if detected
    bool isFusion = (regional != "Contemporary international" && 
                    temporal != "Contemporary fusion" &&
                    regional != temporal);
    
    if (isFusion) {
        context = "Fusion of " + context;
    }
    
    return context;
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
#endif

// Master analyzer implementation moved to ai_algorithms_master.cpp

} // namespace MusicAnalysis