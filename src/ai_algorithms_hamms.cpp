#include "ai_algorithms.h"
#include <numeric>
#include <algorithm>

namespace MusicAnalysis {

HAMMSVector HAMMSAnalyzer::calculateHAMMS(const AudioBuffer& audio) {
    HAMMSVector hamms;
    
    // Calculate each dimension of the HAMMS vector
    hamms.harmonicity = analyzeHarmonicity(audio);
    hamms.melodicity = analyzeMelodicity(audio);
    hamms.rhythmicity = analyzeRhythmicity(audio);
    hamms.timbrality = analyzeTimbrality(audio);
    hamms.dynamics = analyzeDynamics(audio);
    hamms.tonality = analyzeTonality(audio);
    hamms.temporality = analyzeTemporality(audio);
    
    return hamms;
}

// Harmonic Analysis
float HAMMSAnalyzer::analyzeHarmonicity(const AudioBuffer& audio) {
    AudioProcessor processor;
    SpectralFeatures features = processor.calculateSpectralFeatures(audio);
    
    float hnr = calculateHarmonicToNoiseRatio(features);
    float harmonicSeries = detectHarmonicSeries(features.magnitude);
    
    // Combine metrics (weighted average)
    return (0.6f * hnr + 0.4f * harmonicSeries);
}

float HAMMSAnalyzer::calculateHarmonicToNoiseRatio(const SpectralFeatures& features) {
    // Simplified HNR calculation
    float harmonicEnergy = 0.0f;
    float totalEnergy = 0.0f;
    
    for (size_t i = 0; i < features.magnitude.size(); ++i) {
        totalEnergy += features.magnitude[i] * features.magnitude[i];
        
        // Check if frequency is near a harmonic
        float freq = features.frequencies[i];
        float fundamental = 100.0f; // Assume fundamental around 100Hz
        
        for (int harmonic = 1; harmonic <= 10; ++harmonic) {
            float expectedFreq = fundamental * harmonic;
            if (std::abs(freq - expectedFreq) < 20.0f) {
                harmonicEnergy += features.magnitude[i] * features.magnitude[i];
                break;
            }
        }
    }
    
    return totalEnergy > 0 ? harmonicEnergy / totalEnergy : 0.0f;
}

float HAMMSAnalyzer::detectHarmonicSeries(const std::vector<float>& spectrum) {
    // Detect presence of harmonic series
    if (spectrum.empty()) return 0.0f;
    
    // Find peaks in spectrum
    std::vector<float> peaks;
    for (size_t i = 1; i < spectrum.size() - 1; ++i) {
        if (spectrum[i] > spectrum[i-1] && spectrum[i] > spectrum[i+1]) {
            peaks.push_back(i);
        }
    }
    
    // Check if peaks form harmonic series
    float harmonicScore = 0.0f;
    if (peaks.size() >= 2) {
        float fundamental = peaks[0];
        for (size_t i = 1; i < peaks.size() && i < 5; ++i) {
            float ratio = peaks[i] / fundamental;
            float deviation = std::abs(ratio - std::round(ratio));
            if (deviation < 0.1f) {
                harmonicScore += (1.0f - deviation);
            }
        }
        harmonicScore = std::min(1.0f, harmonicScore / 4.0f);
    }
    
    return harmonicScore;
}

// Melodic Analysis
float HAMMSAnalyzer::analyzeMelodicity(const AudioBuffer& audio) {
    std::vector<float> contour = extractMelodicContour(audio);
    float complexity = calculateMelodicComplexity(contour);
    
    // Higher complexity indicates more melodic content
    return std::min(1.0f, complexity);
}

std::vector<float> HAMMSAnalyzer::extractMelodicContour(const AudioBuffer& audio) {
    // Simplified pitch tracking
    std::vector<float> pitches;
    const int windowSize = 2048;
    const int hopSize = windowSize / 2;
    
    for (int i = 0; i < audio.length - windowSize; i += hopSize) {
        // Extract window
        std::vector<float> window(audio.samples.begin() + i, 
                                 audio.samples.begin() + i + windowSize);
        
        // Simple autocorrelation for pitch detection
        float maxCorr = 0.0f;
        int bestLag = 0;
        
        for (int lag = 20; lag < windowSize / 2; ++lag) {
            float corr = 0.0f;
            for (int j = 0; j < windowSize - lag; ++j) {
                corr += window[j] * window[j + lag];
            }
            if (corr > maxCorr) {
                maxCorr = corr;
                bestLag = lag;
            }
        }
        
        if (bestLag > 0) {
            float pitch = (float)audio.sampleRate / bestLag;
            pitches.push_back(pitch);
        }
    }
    
    return pitches;
}

float HAMMSAnalyzer::calculateMelodicComplexity(const std::vector<float>& contour) {
    if (contour.size() < 2) return 0.0f;
    
    // Calculate pitch variation
    float totalVariation = 0.0f;
    for (size_t i = 1; i < contour.size(); ++i) {
        totalVariation += std::abs(contour[i] - contour[i-1]);
    }
    
    // Normalize by length
    float avgVariation = totalVariation / contour.size();
    
    // Convert to 0-1 range (assuming max variation of 1000Hz)
    return std::min(1.0f, avgVariation / 1000.0f);
}

// Rhythmic Analysis
float HAMMSAnalyzer::analyzeRhythmicity(const AudioBuffer& audio) {
    BPMDetector bpmDetector;
    OnsetVector onsets = bpmDetector.detectOnsets(audio);
    
    float regularity = calculateRhythmicRegularity(onsets);
    
    // Invert for complexity (less regular = more complex)
    return 1.0f - regularity;
}

float HAMMSAnalyzer::calculateRhythmicRegularity(const OnsetVector& onsets) {
    if (onsets.onsetTimes.size() < 3) return 0.0f;
    
    // Calculate inter-onset intervals
    std::vector<float> intervals;
    for (size_t i = 1; i < onsets.onsetTimes.size(); ++i) {
        intervals.push_back(onsets.onsetTimes[i] - onsets.onsetTimes[i-1]);
    }
    
    // Calculate variance
    float mean = std::accumulate(intervals.begin(), intervals.end(), 0.0f) / intervals.size();
    float variance = 0.0f;
    for (float interval : intervals) {
        variance += std::pow(interval - mean, 2);
    }
    variance /= intervals.size();
    
    // Convert to regularity score (0-1)
    float cv = std::sqrt(variance) / mean; // Coefficient of variation
    return std::exp(-cv); // Higher CV = lower regularity
}

float HAMMSAnalyzer::analyzeSyncopation(const BeatVector& beats) {
    if (beats.beatTimes.size() < 3) return 0.0f;
    
    // Calculate average beat interval
    std::vector<float> intervals;
    for (size_t i = 1; i < beats.beatTimes.size(); ++i) {
        intervals.push_back(beats.beatTimes[i] - beats.beatTimes[i-1]);
    }
    
    float avgInterval = std::accumulate(intervals.begin(), intervals.end(), 0.0f) / intervals.size();
    float intervalStdDev = 0.0f;
    
    // Calculate standard deviation
    for (float interval : intervals) {
        intervalStdDev += std::pow(interval - avgInterval, 2);
    }
    intervalStdDev = std::sqrt(intervalStdDev / intervals.size());
    
    // Detect syncopation patterns
    int syncopatedBeats = 0;
    int totalStrongBeats = 0;
    
    for (size_t i = 0; i < beats.beatTimes.size(); ++i) {
        // Calculate beat position within measure (assuming 4/4)
        float measurePosition = fmod(beats.beatTimes[i] / (avgInterval * 4), 1.0f);
        
        // Strong beats typically fall on 1 and 3 (0.0 and 0.5 in normalized measure)
        bool isStrongPosition = (measurePosition < 0.1f || 
                                (measurePosition > 0.45f && measurePosition < 0.55f));
        
        // Check if this is actually a weak beat based on strength
        if (beats.strengths[i] > 0.7f) {
            totalStrongBeats++;
            
            // Syncopation: strong accent on weak position
            if (!isStrongPosition) {
                syncopatedBeats++;
            }
        }
    }
    
    // Calculate syncopation score
    float syncopationRatio = totalStrongBeats > 0 ? 
        static_cast<float>(syncopatedBeats) / totalStrongBeats : 0.0f;
    
    // Factor in rhythm regularity (more irregular = more syncopated feel)
    float regularityFactor = 1.0f - std::exp(-intervalStdDev / avgInterval);
    
    // Combine factors
    return std::min(1.0f, syncopationRatio * 0.7f + regularityFactor * 0.3f);
}


// Timbral Analysis
float HAMMSAnalyzer::analyzeTimbrality(const AudioBuffer& audio) {
    AudioProcessor processor;
    SpectralFeatures features = processor.calculateSpectralFeatures(audio);
    
    float complexity = calculateSpectralComplexity(features);
    float variation = analyzeTimbralVariation(audio);
    
    return (0.5f * complexity + 0.5f * variation);
}

float HAMMSAnalyzer::calculateSpectralComplexity(const SpectralFeatures& features) {
    // Spectral entropy as complexity measure
    float totalEnergy = 0.0f;
    for (float mag : features.magnitude) {
        totalEnergy += mag * mag;
    }
    
    if (totalEnergy == 0.0f) return 0.0f;
    
    float entropy = 0.0f;
    for (float mag : features.magnitude) {
        float p = (mag * mag) / totalEnergy;
        if (p > 0.0f) {
            entropy -= p * std::log2(p);
        }
    }
    
    // Normalize entropy (max ~10 for typical spectra)
    return std::min(1.0f, entropy / 10.0f);
}

float HAMMSAnalyzer::analyzeTimbralVariation(const AudioBuffer& audio) {
    // Simplified: analyze spectral centroid variation over time
    const int windowSize = 2048;
    const int hopSize = windowSize / 2;
    std::vector<float> centroids;
    
    AudioProcessor processor;
    
    for (int i = 0; i < audio.length - windowSize; i += hopSize) {
        AudioBuffer window(
            std::vector<float>(audio.samples.begin() + i, 
                             audio.samples.begin() + i + windowSize),
            audio.sampleRate, audio.channels
        );
        
        SpectralFeatures features = processor.calculateSpectralFeatures(window);
        centroids.push_back(features.spectralCentroid);
    }
    
    // Calculate variation
    if (centroids.size() < 2) return 0.0f;
    
    float variance = 0.0f;
    float mean = std::accumulate(centroids.begin(), centroids.end(), 0.0f) / centroids.size();
    
    for (float centroid : centroids) {
        variance += std::pow(centroid - mean, 2);
    }
    variance /= centroids.size();
    
    // Normalize (assuming max centroid around 5000Hz)
    return std::min(1.0f, std::sqrt(variance) / 5000.0f);
}

// Dynamic Analysis
float HAMMSAnalyzer::analyzeDynamics(const AudioBuffer& audio) {
    float range = calculateDynamicRange(audio);
    
    // Simple envelope for variation analysis
    std::vector<float> envelope;
    const int blockSize = 1024;
    
    for (int i = 0; i < audio.length - blockSize; i += blockSize) {
        float blockEnergy = 0.0f;
        for (int j = 0; j < blockSize; ++j) {
            blockEnergy += audio.samples[i + j] * audio.samples[i + j];
        }
        envelope.push_back(std::sqrt(blockEnergy / blockSize));
    }
    
    float variation = analyzeDynamicVariation(envelope);
    
    return (0.7f * range + 0.3f * variation);
}

float HAMMSAnalyzer::calculateDynamicRange(const AudioBuffer& audio) {
    // Find RMS values in dB
    const int blockSize = audio.sampleRate / 10; // 100ms blocks
    std::vector<float> rmsValues;
    
    for (int i = 0; i < audio.length - blockSize; i += blockSize) {
        float rms = 0.0f;
        for (int j = 0; j < blockSize; ++j) {
            rms += audio.samples[i + j] * audio.samples[i + j];
        }
        rms = std::sqrt(rms / blockSize);
        
        if (rms > 0.001f) { // Avoid log(0)
            rmsValues.push_back(20.0f * std::log10(rms));
        }
    }
    
    if (rmsValues.empty()) return 0.0f;
    
    // Calculate range between 10th and 90th percentile
    std::sort(rmsValues.begin(), rmsValues.end());
    int idx10 = rmsValues.size() * 0.1;
    int idx90 = rmsValues.size() * 0.9;
    
    float range = rmsValues[idx90] - rmsValues[idx10];
    
    // Normalize (typical range 0-60dB)
    return std::min(1.0f, range / 60.0f);
}

float HAMMSAnalyzer::analyzeDynamicVariation(const std::vector<float>& envelope) {
    if (envelope.size() < 2) return 0.0f;
    
    // Calculate first derivative (rate of change)
    float totalChange = 0.0f;
    for (size_t i = 1; i < envelope.size(); ++i) {
        totalChange += std::abs(envelope[i] - envelope[i-1]);
    }
    
    // Normalize
    float avgChange = totalChange / envelope.size();
    return std::min(1.0f, avgChange * 10.0f); // Scale factor for typical ranges
}

// Tonal Analysis
float HAMMSAnalyzer::analyzeTonality(const AudioBuffer& audio) {
    AudioProcessor processor;
    ChromaVector chroma = processor.calculateChroma(audio);
    
    float clarity = calculateTonalClarity(chroma);
    float stability = analyzeKeyStability(audio);
    
    return (0.6f * clarity + 0.4f * stability);
}

float HAMMSAnalyzer::calculateTonalClarity(const ChromaVector& chroma) {
    // Find dominant pitches
    auto maxIt = std::max_element(chroma.chroma.begin(), chroma.chroma.end());
    float maxVal = *maxIt;
    
    if (maxVal == 0.0f) return 0.0f;
    
    // Calculate ratio of dominant pitches to total
    std::vector<float> sorted = chroma.chroma;
    std::sort(sorted.rbegin(), sorted.rend());
    
    // Sum of top 3 pitches vs total
    float topSum = sorted[0] + sorted[1] + sorted[2];
    float totalSum = std::accumulate(sorted.begin(), sorted.end(), 0.0f);
    
    return totalSum > 0 ? topSum / totalSum : 0.0f;
}

float HAMMSAnalyzer::analyzeKeyStability(const AudioBuffer& audio) {
    // Simplified: analyze chroma vector stability over time
    const int windowSize = audio.sampleRate * 2; // 2 second windows
    const int hopSize = audio.sampleRate; // 1 second hop
    
    std::vector<ChromaVector> chromaSequence;
    AudioProcessor processor;
    
    for (int i = 0; i < audio.length - windowSize; i += hopSize) {
        AudioBuffer window(
            std::vector<float>(audio.samples.begin() + i, 
                             audio.samples.begin() + i + windowSize),
            audio.sampleRate, audio.channels
        );
        
        chromaSequence.push_back(processor.calculateChroma(window));
    }
    
    if (chromaSequence.size() < 2) return 1.0f;
    
    // Calculate correlation between consecutive chroma vectors
    float totalCorrelation = 0.0f;
    for (size_t i = 1; i < chromaSequence.size(); ++i) {
        float correlation = 0.0f;
        for (int j = 0; j < 12; ++j) {
            correlation += chromaSequence[i].chroma[j] * chromaSequence[i-1].chroma[j];
        }
        totalCorrelation += correlation;
    }
    
    return totalCorrelation / (chromaSequence.size() - 1);
}

// Temporal Analysis
float HAMMSAnalyzer::analyzeTemporality(const AudioBuffer& audio) {
    float tempoStability = calculateTempoStability(audio);
    
    // Get beat tracking for consistency analysis
    TimeSignatureDetector detector;
    BeatVector beats = detector.detectBeats(audio);
    float consistency = analyzeRhythmicConsistency(beats);
    
    return (0.5f * tempoStability + 0.5f * consistency);
}

float HAMMSAnalyzer::calculateTempoStability(const AudioBuffer& audio) {
    // Analyze tempo variation over time
    const int segmentLength = audio.sampleRate * 10; // 10 second segments
    std::vector<float> tempos;
    
    BPMDetector bpmDetector;
    
    for (int i = 0; i < audio.length - segmentLength; i += segmentLength / 2) {
        AudioBuffer segment(
            std::vector<float>(audio.samples.begin() + i, 
                             audio.samples.begin() + i + segmentLength),
            audio.sampleRate, audio.channels
        );
        
        float tempo = bpmDetector.detectBPM(segment);
        if (tempo > 0) {
            tempos.push_back(tempo);
        }
    }
    
    if (tempos.size() < 2) return 1.0f;
    
    // Calculate coefficient of variation
    float mean = std::accumulate(tempos.begin(), tempos.end(), 0.0f) / tempos.size();
    float variance = 0.0f;
    
    for (float tempo : tempos) {
        variance += std::pow(tempo - mean, 2);
    }
    variance /= tempos.size();
    
    float cv = std::sqrt(variance) / mean;
    
    // Convert to stability score (lower CV = higher stability)
    return std::exp(-cv * 10.0f); // Scale factor for typical CV ranges
}

float HAMMSAnalyzer::analyzeRhythmicConsistency(const BeatVector& beats) {
    if (beats.beatTimes.size() < 3) return 1.0f;
    
    // Analyze beat interval consistency
    std::vector<float> intervals;
    for (size_t i = 1; i < beats.beatTimes.size(); ++i) {
        intervals.push_back(beats.beatTimes[i] - beats.beatTimes[i-1]);
    }
    
    // Remove outliers (tempo changes)
    std::sort(intervals.begin(), intervals.end());
    int trimSize = intervals.size() * 0.1; // Trim 10% from each end
    
    if (intervals.size() > 2 * trimSize) {
        intervals.erase(intervals.begin(), intervals.begin() + trimSize);
        intervals.erase(intervals.end() - trimSize, intervals.end());
    }
    
    // Calculate consistency
    float mean = std::accumulate(intervals.begin(), intervals.end(), 0.0f) / intervals.size();
    float variance = 0.0f;
    
    for (float interval : intervals) {
        variance += std::pow(interval - mean, 2);
    }
    variance /= intervals.size();
    
    // Convert to consistency score
    float cv = std::sqrt(variance) / mean;
    return std::exp(-cv * 20.0f); // Higher scale factor for beat consistency
}

} // namespace MusicAnalysis