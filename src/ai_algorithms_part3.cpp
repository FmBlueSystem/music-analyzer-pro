// Continuation of ai_algorithms.cpp - Part 3

#include "ai_algorithms.h"
#include <numeric>

namespace MusicAnalysis {

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
    float qualityScore = 0.0f;
    
    // Signal-to-noise ratio
    float snr = calculateSNR(audio);
    if (snr > 40.0f) qualityScore += 0.3f;
    else if (snr > 20.0f) qualityScore += 0.2f;
    else if (snr > 10.0f) qualityScore += 0.1f;
    
    // Dynamic range
    EnergyAnalyzer energyAnalyzer;
    float dynamicRange = energyAnalyzer.analyzeDynamicRange(audio);
    qualityScore += dynamicRange * 0.3f; // Already normalized to 0-1
    
    // Frequency response completeness
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    if (isFrequencyResponseComplete(features)) {
        qualityScore += 0.2f;
    }
    
    // Compression artifacts
    float compressionArtifacts = detectCompressionArtifacts(audio);
    qualityScore += (1.0f - compressionArtifacts) * 0.2f;
    
    return std::min(1.0f, qualityScore);
}

float ConfidenceCalculator::calculateSNR(const AudioBuffer& audio) {
    // Simplified SNR calculation
    // Find quiet sections for noise estimation
    int windowSize = (int)(0.1f * audio.sampleRate); // 100ms
    std::vector<float> rmsValues;
    
    for (int i = 0; i <= (int)audio.samples.size() - windowSize; i += windowSize) {
        float rms = AudioProcessor::calculateRMS(
            std::vector<float>(audio.samples.begin() + i, audio.samples.begin() + i + windowSize)
        );
        rmsValues.push_back(rms);
    }
    
    if (rmsValues.empty()) return 0.0f;
    
    std::sort(rmsValues.begin(), rmsValues.end());
    
    float noiseFloor = rmsValues[rmsValues.size() / 10]; // 10th percentile as noise
    float signalLevel = rmsValues[rmsValues.size() * 9 / 10]; // 90th percentile as signal
    
    if (noiseFloor == 0.0f) return 60.0f; // Very quiet noise floor
    
    return 20.0f * std::log10(signalLevel / noiseFloor);
}

float ConfidenceCalculator::detectCompressionArtifacts(const AudioBuffer& audio) {
    // Look for typical compression artifacts
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    float artifactScore = 0.0f;
    
    // Pre-echo artifacts (simplified detection)
    if (features.zeroCrossingRate > 0.2f) {
        artifactScore += 0.3f;
    }
    
    // Frequency response irregularities
    float totalEnergy = std::accumulate(features.magnitude.begin(), features.magnitude.end(), 0.0f);
    if (totalEnergy > 0) {
        float highFreqEnergy = 0.0f;
        for (size_t i = 0; i < features.magnitude.size(); i++) {
            if (features.frequencies[i] > 15000.0f) {
                highFreqEnergy += features.magnitude[i];
            }
        }
        
        float highFreqRatio = highFreqEnergy / totalEnergy;
        if (highFreqRatio < 0.01f) { // Very little high-frequency content
            artifactScore += 0.4f;
        }
    }
    
    // Quantization noise
    CharacteristicsExtractor extractor;
    if (extractor.hasCompression(audio)) {
        artifactScore += 0.3f;
    }
    
    return std::min(1.0f, artifactScore);
}

bool ConfidenceCalculator::isFrequencyResponseComplete(const SpectralFeatures& features) {
    if (features.frequencies.empty()) return false;
    
    // Check if we have reasonable energy across the spectrum
    float totalEnergy = std::accumulate(features.magnitude.begin(), features.magnitude.end(), 0.0f);
    if (totalEnergy == 0) return false;
    
    // Check energy distribution
    float lowEnergy = 0.0f, midEnergy = 0.0f, highEnergy = 0.0f;
    
    for (size_t i = 0; i < features.magnitude.size(); i++) {
        if (features.frequencies[i] < 500.0f) {
            lowEnergy += features.magnitude[i];
        } else if (features.frequencies[i] < 4000.0f) {
            midEnergy += features.magnitude[i];
        } else {
            highEnergy += features.magnitude[i];
        }
    }
    
    float lowRatio = lowEnergy / totalEnergy;
    float midRatio = midEnergy / totalEnergy;
    float highRatio = highEnergy / totalEnergy;
    
    // Complete response should have energy in all bands
    return (lowRatio > 0.05f && midRatio > 0.3f && highRatio > 0.02f);
}

float ConfidenceCalculator::validateConsistency(const AIAnalysisResult& results) {
    float consistencyScore = 0.0f;
    int validationCount = 0;
    
    // Cross-validate BPM with danceability
    if (results.AI_BPM > 0 && results.AI_DANCEABILITY >= 0) {
        bool bpmDanceConsistent = false;
        if (results.AI_BPM >= 90.0f && results.AI_BPM <= 160.0f && results.AI_DANCEABILITY > 0.5f) {
            bpmDanceConsistent = true;
        } else if (results.AI_BPM < 80.0f && results.AI_DANCEABILITY < 0.5f) {
            bpmDanceConsistent = true;
        }
        
        if (bpmDanceConsistent) consistencyScore += 0.25f;
        validationCount++;
    }
    
    // Cross-validate energy with valence
    if (results.AI_ENERGY >= 0 && results.AI_VALENCE >= 0) {
        bool energyValenceConsistent = 
            (results.AI_ENERGY > 0.7f && results.AI_VALENCE > 0.6f) ||
            (results.AI_ENERGY < 0.3f && results.AI_VALENCE < 0.4f) ||
            (results.AI_ENERGY >= 0.3f && results.AI_ENERGY <= 0.7f);
        
        if (energyValenceConsistent) consistencyScore += 0.25f;
        validationCount++;
    }
    
    // Cross-validate instrumentalness with speechiness
    if (results.AI_INSTRUMENTALNESS >= 0 && results.AI_SPEECHINESS >= 0) {
        float totalVocalContent = results.AI_SPEECHINESS + (1.0f - results.AI_INSTRUMENTALNESS);
        bool vocalConsistent = totalVocalContent >= 0.8f && totalVocalContent <= 1.2f;
        
        if (vocalConsistent) consistencyScore += 0.25f;
        validationCount++;
    }
    
    // Cross-validate key with mode
    if (!results.AI_KEY.empty() && !results.AI_MODE.empty()) {
        bool keyModeConsistent = 
            (results.AI_KEY.find("major") != std::string::npos && results.AI_MODE == "Major") ||
            (results.AI_KEY.find("minor") != std::string::npos && results.AI_MODE == "Minor");
        
        if (keyModeConsistent) consistencyScore += 0.25f;
        validationCount++;
    }
    
    return (validationCount > 0) ? consistencyScore : 0.5f; // Default to moderate confidence
}

float ConfidenceCalculator::calculateFeatureCertainty(const AIAnalysisResult& results) {
    float certaintyScore = 0.0f;
    int featureCount = 0;
    
    // Features with clear ranges have higher certainty
    if (results.AI_BPM >= 60.0f && results.AI_BPM <= 200.0f) {
        certaintyScore += 0.1f;
        featureCount++;
    }
    
    if (results.AI_ENERGY >= 0.0f && results.AI_ENERGY <= 1.0f) {
        // Higher certainty for extreme values
        if (results.AI_ENERGY < 0.2f || results.AI_ENERGY > 0.8f) {
            certaintyScore += 0.1f;
        } else {
            certaintyScore += 0.05f; // Lower certainty for middle values
        }
        featureCount++;
    }
    
    if (results.AI_VALENCE >= 0.0f && results.AI_VALENCE <= 1.0f) {
        if (results.AI_VALENCE < 0.2f || results.AI_VALENCE > 0.8f) {
            certaintyScore += 0.1f;
        } else {
            certaintyScore += 0.05f;
        }
        featureCount++;
    }
    
    if (results.AI_DANCEABILITY >= 0.0f && results.AI_DANCEABILITY <= 1.0f) {
        certaintyScore += 0.08f;
        featureCount++;
    }
    
    if (results.AI_ACOUSTICNESS >= 0.0f && results.AI_ACOUSTICNESS <= 1.0f) {
        if (results.AI_ACOUSTICNESS < 0.2f || results.AI_ACOUSTICNESS > 0.8f) {
            certaintyScore += 0.08f;
        } else {
            certaintyScore += 0.04f;
        }
        featureCount++;
    }
    
    if (!results.AI_KEY.empty() && !results.AI_MODE.empty()) {
        certaintyScore += 0.1f;
        featureCount++;
    }
    
    if (results.AI_TIME_SIGNATURE >= 3 && results.AI_TIME_SIGNATURE <= 7) {
        certaintyScore += 0.05f;
        featureCount++;
    }
    
    if (!results.AI_CHARACTERISTICS.empty()) {
        certaintyScore += 0.05f;
        featureCount++;
    }
    
    if (results.AI_LOUDNESS >= -60.0f && results.AI_LOUDNESS <= 0.0f) {
        certaintyScore += 0.05f;
        featureCount++;
    }
    
    if (results.AI_LIVENESS >= 0.0f && results.AI_LIVENESS <= 1.0f) {
        certaintyScore += 0.05f;
        featureCount++;
    }
    
    return (featureCount > 0) ? certaintyScore : 0.1f;
}

// ========================================
// 🎭 CLASSIFICATION ALGORITHMS
// ========================================

std::vector<std::string> GenreClassifier::classifySubgenres(const AudioBuffer& audio, const AIAnalysisResult& features) {
    std::vector<std::string> subgenres;
    
    // Electronic music classification
    if (features.AI_ACOUSTICNESS < 0.3f && features.AI_ENERGY > 0.6f) {
        if (features.AI_BPM >= 120.0f && features.AI_BPM <= 135.0f) {
            if (features.AI_DANCEABILITY > 0.8f) {
                subgenres.push_back("House");
            } else {
                subgenres.push_back("Electronic");
            }
        } else if (features.AI_BPM >= 160.0f && features.AI_BPM <= 180.0f) {
            subgenres.push_back("Drum & Bass");
        } else if (features.AI_BPM >= 135.0f && features.AI_BPM <= 155.0f) {
            subgenres.push_back("Trance");
        }
    }
    
    // Rock music classification
    else if (features.AI_ACOUSTICNESS > 0.3f && features.AI_ACOUSTICNESS < 0.7f && features.AI_ENERGY > 0.5f) {
        if (features.AI_VALENCE > 0.6f) {
            subgenres.push_back("Pop Rock");
        } else if (features.AI_ENERGY > 0.8f) {
            subgenres.push_back("Hard Rock");
        } else {
            subgenres.push_back("Alternative Rock");
        }
    }
    
    // Acoustic music classification
    else if (features.AI_ACOUSTICNESS > 0.7f) {
        if (features.AI_ENERGY < 0.4f && features.AI_VALENCE > 0.5f) {
            subgenres.push_back("Folk");
        } else if (features.AI_INSTRUMENTALNESS > 0.8f) {
            subgenres.push_back("Classical");
        } else {
            subgenres.push_back("Acoustic");
        }
    }
    
    // Hip-hop/Rap classification
    else if (features.AI_SPEECHINESS > 0.6f && features.AI_BPM >= 70.0f && features.AI_BPM <= 140.0f) {
        if (features.AI_ENERGY > 0.7f) {
            subgenres.push_back("Hip-Hop");
        } else {
            subgenres.push_back("Rap");
        }
    }
    
    // Jazz classification
    else if (features.AI_ACOUSTICNESS > 0.6f && features.AI_INSTRUMENTALNESS > 0.5f) {
        if (features.AI_BPM >= 60.0f && features.AI_BPM <= 120.0f) {
            subgenres.push_back("Jazz");
        }
    }
    
    // Default classification
    if (subgenres.empty()) {
        if (features.AI_ENERGY > 0.7f) {
            subgenres.push_back("High Energy");
        } else if (features.AI_ENERGY < 0.3f) {
            subgenres.push_back("Ambient");
        } else {
            subgenres.push_back("Pop");
        }
    }
    
    // Limit to 2-3 subgenres as per specification
    if (subgenres.size() > 3) {
        subgenres.resize(3);
    }
    
    return subgenres;
}

std::string GenreClassifier::classifyEra(const AudioBuffer& audio, const AIAnalysisResult& features) {
    std::string productionStyle = analyzeProductionTechniques(AudioProcessor::calculateSpectralFeatures(audio));
    std::string instrumentation = analyzeInstrumentationPatterns(audio);
    
    // Era classification based on production characteristics
    SpectralFeatures spectralFeatures = AudioProcessor::calculateSpectralFeatures(audio);
    
    // 2010s-2020s: Heavy compression, loud mastering
    if (features.AI_LOUDNESS > -8.0f && hasVintageCharacteristics(spectralFeatures) == false) {
        if (features.AI_ACOUSTICNESS < 0.3f && features.AI_ENERGY > 0.7f) {
            return "2010s";
        } else {
            return "2000s";
        }
    }
    
    // 1990s: Alternative rock era, specific production
    else if (features.AI_LOUDNESS > -15.0f && features.AI_ACOUSTICNESS > 0.4f && features.AI_ACOUSTICNESS < 0.8f) {
        if (features.AI_ENERGY > 0.6f && features.AI_VALENCE < 0.6f) {
            return "1990s";
        }
    }
    
    // 1980s: Digital reverb, synthesizers
    else if (features.AI_ACOUSTICNESS < 0.5f && features.AI_LIVENESS > 0.3f) {
        if (spectralFeatures.spectralCentroid > 2000.0f) {
            return "1980s";
        }
    }
    
    // 1970s: Analog warmth, moderate production
    else if (features.AI_LOUDNESS < -18.0f && features.AI_ACOUSTICNESS > 0.5f) {
        return "1970s";
    }
    
    // 1960s: Very analog, lower fidelity
    else if (hasVintageCharacteristics(spectralFeatures) && features.AI_LOUDNESS < -20.0f) {
        return "1960s";
    }
    
    // Default to modern era
    return "2000s";
}

std::string GenreClassifier::analyzeProductionTechniques(const SpectralFeatures& features) {
    std::string techniques = "";
    
    // Digital vs analog characteristics
    if (features.spectralRolloff > 15000.0f) {
        techniques += "Digital";
    } else if (features.spectralRolloff < 8000.0f) {
        techniques += "Analog";
    }
    
    // Compression characteristics
    if (features.zeroCrossingRate > 0.1f) {
        techniques += " Compressed";
    }
    
    return techniques;
}

std::string GenreClassifier::analyzeInstrumentationPatterns(const AudioBuffer& audio) {
    SpectralFeatures features = AudioProcessor::calculateSpectralFeatures(audio);
    
    if (features.spectralCentroid > 3000.0f && features.zeroCrossingRate > 0.08f) {
        return "Electronic instruments";
    } else if (features.spectralCentroid > 1500.0f && features.spectralCentroid < 3000.0f) {
        return "Electric instruments";
    } else {
        return "Acoustic instruments";
    }
}

bool GenreClassifier::hasVintageCharacteristics(const SpectralFeatures& features) {
    // Vintage recordings typically have:
    // - Lower frequency response
    // - Specific harmonic distortion patterns
    // - Lower overall fidelity
    
    return (features.spectralRolloff < 10000.0f && 
            features.spectralCentroid < 2000.0f && 
            features.zeroCrossingRate < 0.05f);
}

std::string GenreClassifier::analyzeCulturalContext(const AudioBuffer& audio, const AIAnalysisResult& features) {
    // Simplified cultural context analysis
    
    // Latin rhythms
    if (features.AI_DANCEABILITY > 0.8f && features.AI_BPM >= 90.0f && features.AI_BPM <= 130.0f) {
        if (features.AI_ACOUSTICNESS > 0.6f) {
            return "Latin American traditional";
        } else {
            return "Latin fusion";
        }
    }
    
    // African polyrhythms
    if (features.AI_TIME_SIGNATURE != 4 && features.AI_DANCEABILITY > 0.7f) {
        return "African polyrhythmic traditions";
    }
    
    // British characteristics
    if (features.AI_ACOUSTICNESS > 0.5f && features.AI_ENERGY > 0.6f && 
        !features.AI_SUBGENRES.empty() && features.AI_SUBGENRES[0].find("Rock") != std::string::npos) {
        return "British rock tradition";
    }
    
    // American blues/country
    if (features.AI_ACOUSTICNESS > 0.7f && features.AI_VALENCE < 0.5f && features.AI_BPM < 100.0f) {
        return "American blues tradition";
    }
    
    // Electronic/European
    if (features.AI_ACOUSTICNESS < 0.2f && features.AI_ENERGY > 0.8f) {
        return "European electronic tradition";
    }
    
    // Asian pop
    if (features.AI_VALENCE > 0.7f && features.AI_ENERGY > 0.6f && features.AI_ACOUSTICNESS < 0.6f) {
        return "Asian pop influence";
    }
    
    return "Western popular music";
}

// ========================================
// 😊 MOOD ANALYSIS
// ========================================

std::string MoodAnalyzer::analyzeMood(const AIAnalysisResult& features) {
    return mapEnergyValenceToMood(features.AI_ENERGY, features.AI_VALENCE);
}

std::string MoodAnalyzer::mapEnergyValenceToMood(float energy, float valence) {
    // Mood mapping matrix from documentation
    if (energy > 0.7f && valence > 0.7f) {
        return "Energetic, Joyful, Euphoric";
    } else if (energy > 0.7f && valence > 0.4f) {
        return "Energetic, Uplifting";
    } else if (energy > 0.7f && valence < 0.4f) {
        return "Aggressive, Intense, Powerful";
    } else if (energy > 0.4f && valence > 0.7f) {
        return "Happy, Upbeat";
    } else if (energy > 0.4f && valence > 0.4f) {
        return "Positive, Moderate";
    } else if (energy > 0.4f && valence < 0.4f) {
        return "Serious, Focused";
    } else if (energy < 0.4f && valence > 0.6f) {
        return "Peaceful, Content, Relaxed";
    } else if (energy < 0.4f && valence > 0.3f) {
        return "Calm, Neutral";
    } else {
        return "Sad, Melancholic, Contemplative";
    }
}

std::vector<std::string> MoodAnalyzer::analyzeOccasions(const AIAnalysisResult& features) {
    return mapBPMEnergyToOccasions(features.AI_BPM, features.AI_ENERGY);
}

std::vector<std::string> MoodAnalyzer::mapBPMEnergyToOccasions(float bpm, float energy) {
    std::vector<std::string> occasions;
    
    // High BPM + High Energy
    if (bpm > 120.0f && energy > 0.7f) {
        occasions.push_back("Party");
        occasions.push_back("Workout");
        if (bpm > 140.0f) {
            occasions.push_back("Dancing");
        } else {
            occasions.push_back("Driving");
        }
    }
    // Medium BPM + Moderate Energy
    else if (bpm >= 90.0f && bpm <= 120.0f && energy >= 0.4f && energy <= 0.7f) {
        occasions.push_back("Background");
        occasions.push_back("Casual listening");
        if (energy > 0.5f) {
            occasions.push_back("Driving");
        } else {
            occasions.push_back("Coffee shop");
        }
    }
    // Low BPM + Low Energy
    else if (bpm < 90.0f && energy < 0.4f) {
        occasions.push_back("Study");
        occasions.push_back("Relaxation");
        occasions.push_back("Meditation");
    }
    // High Energy but lower BPM
    else if (energy > 0.6f) {
        occasions.push_back("Gym");
        occasions.push_back("Motivation");
    }
    // Default occasions
    else {
        occasions.push_back("General listening");
        occasions.push_back("Background");
    }
    
    // Limit to 2-3 occasions as per specification
    if (occasions.size() > 3) {
        occasions.resize(3);
    }
    
    return occasions;
}

} // namespace MusicAnalysis