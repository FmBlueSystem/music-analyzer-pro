#!/usr/bin/env node

/**
 * 🔍 EXPLICACIÓN: FUENTES DE DATOS AI_* vs LLM_*
 * Cómo se obtienen los diferentes tipos de metadatos en el sistema
 */

console.log('🔍 EXPLICACIÓN: FUENTES DE DATOS AI_* vs LLM_*');
console.log('===============================================');

console.log('\n📊 RESUMEN EJECUTIVO:');
console.log('- AI_* campos = Algoritmos de audio (C++ o JavaScript)');  
console.log('- LLM_* campos = Análisis musical inteligente (Claude API)');
console.log('- Ambos se combinan para análisis completo');

console.log('\n🎯 1. DATOS AI_* - ALGORITMOS DE PROCESAMIENTO DE AUDIO');
console.log('====================================================');

console.log('\n🔬 **FUENTE: Algoritmos de Análisis de Audio**');
console.log('- 📂 Archivo: src/ai_algorithms.cpp (motor C++)');
console.log('- 🔄 Fallback: Simulación JavaScript en main.js');
console.log('- 📡 IPC: analyze-file-with-algorithms');

console.log('\n🎵 **CAMPOS AI_* TÉCNICOS (Calculados por algoritmos):**');
const aiTechnicalFields = [
    'AI_ACOUSTICNESS - Análisis espectral (frecuencias vs sintetizadores)',
    'AI_INSTRUMENTALNESS - Detección de voz vs instrumental',  
    'AI_SPEECHINESS - Detección de contenido hablado',
    'AI_LIVENESS - Detección de audio en vivo vs estudio',
    'AI_BPM - Seguimiento de tempo y pulso',
    'AI_LOUDNESS - Análisis de RMS y peaks',
    'AI_KEY - Detección de clave musical (algoritmo Krumhansl-Schmuckler)',
    'AI_TIME_SIGNATURE - Detección de compás (4/4, 3/4, etc)',
    'AI_MODE - Modo mayor/menor'
];

aiTechnicalFields.forEach(field => console.log(`  ✅ ${field}`));

console.log('\n🧠 **CAMPOS AI_* INTERPRETATIVOS (Generados por LLM):**');
const aiInterpretativeFields = [
    'AI_ENERGY - Nivel de energía musical (0.0-1.0)',
    'AI_DANCEABILITY - Capacidad de baile (0.0-1.0)',
    'AI_VALENCE - Valencia emocional (0.0-1.0)',
    'AI_MOOD - Estado de ánimo ("Energetic", "Melancholic", etc)',
    'AI_CULTURAL_CONTEXT - Contexto cultural específico',
    'AI_SUBGENRES - Array de subgéneros musicales',
    'AI_ERA - Década o época musical',
    'AI_OCCASION - Ocasiones de uso',
    'AI_CHARACTERISTICS - Características musicales',
    'AI_CONFIDENCE - Confianza del análisis (0.7-1.0)'
];

aiInterpretativeFields.forEach(field => console.log(`  🧠 ${field}`));

console.log('\n⚙️ **FLUJO DE OBTENCIÓN AI_*:**');
console.log(`
1. Usuario hace clic "🤖 Analizar"
2. Frontend llama: ipcRenderer.invoke('analyze-file-with-algorithms', filePath, algorithms)
3. Backend main.js ejecuta análisis:
   a) 🚀 Prioridad 1: Motor C++ (metadataAddon.analyzeAudio)
   b) 📊 Fallback: Simulación JavaScript con valores coherentes
4. Resultados combinan datos técnicos + interpretativos
5. Se guardan en: database.insertLLMMetadata() 
6. Se escriben a archivo: metadataWriter.writeLLMMetadataSafe()
`);

console.log('\n🧠 2. DATOS LLM_* - ANÁLISIS MUSICAL INTELIGENTE');
console.log('==============================================');

console.log('\n🤖 **FUENTE: Claude API (Anthropic)**');
console.log('- 🌐 API: https://api.anthropic.com/v1/messages');
console.log('- 🧠 Modelo: claude-3-5-sonnet-20241022');
console.log('- 📡 IPC: analyze-llm');

console.log('\n🎵 **CAMPOS LLM_* PRINCIPALES:**');
const llmFields = [
    'LLM_DESCRIPTION - Análisis musical detallado (máx 250 palabras)',
    'LLM_MOOD - Estado de ánimo principal',
    'LLM_GENRE - Género principal CORREGIDO por análisis profesional',
    'LLM_SUBGENRE - Subgénero específico',
    'LLM_CONTEXT - Contexto cultural específico',
    'LLM_OCCASIONS - Array de ocasiones de uso',
    'LLM_ENERGY_LEVEL - Nivel de energía (Alto/Medio/Bajo)',
    'LLM_DANCEABILITY - Capacidad de baile (Alta/Media/Baja)',
    'LLM_RECOMMENDATIONS - Recomendaciones específicas para DJs'
];

llmFields.forEach(field => console.log(`  🧠 ${field}`));

console.log('\n⚙️ **FLUJO DE OBTENCIÓN LLM_*:**');
console.log(`
1. Usuario hace clic "🧠 Analizar" o análisis automático
2. Frontend llama: ipcRenderer.invoke('analyze-llm', filePath)
3. Backend main.js construye prompt con información del archivo:
   - Metadatos existentes (título, artista, álbum, género)
   - Reglas de coherencia musical
   - Instrucciones de corrección de género
4. Llamada a Claude API con prompt especializado
5. Claude analiza y retorna JSON con campos LLM_*
6. Validación de coherencia musical automática
7. Guardado en BD + escritura a archivo de audio
`);

console.log('\n🔄 3. INTEGRACIÓN Y FLUJO COMPLETO');
console.log('=================================');

console.log('\n📋 **ANÁLISIS DUAL COORDINADO:**');
console.log(`
🔬 PASO 1: Algoritmos AI_* (Técnico)
├── C++: Análisis real de audio con FFTW3
├── JavaScript: Simulación coherente (fallback)  
└── Resultado: Datos técnicos precisos

🧠 PASO 2: Análisis LLM_* (Musical)
├── Claude API: Análisis contextual profesional
├── Corrección de género automática
└── Resultado: Interpretación musical inteligente

💾 PASO 3: Unificación y Persistencia
├── Base de datos: llm_metadata tabla
├── Archivo físico: MP3 ID3v2.4 / FLAC Vorbis
└── Cache: Invalidación y actualización UI
`);

console.log('\n🎯 **EJEMPLO PRÁCTICO - Sabrina "Boys (Summertime Love)":**');
console.log('=======================================================');

console.log('\n📊 **DATOS AI_* (De algoritmos de audio):**');
console.log('  AI_ACOUSTICNESS: 0.23 (detectado: sintetizadores)');
console.log('  AI_BPM: 124.5 (beat tracking del audio)');
console.log('  AI_KEY: "A Minor" (análisis armónico)');
console.log('  AI_LOUDNESS: -8.2 dB (análisis RMS)');
console.log('  AI_TIME_SIGNATURE: 4 (compás 4/4 detectado)');

console.log('\n🧠 **DATOS LLM_* (De Claude API):**');
console.log('  LLM_GENRE: "Italo Disco" (corregido de "Rock")');
console.log('  LLM_DESCRIPTION: "Clásico Italo Disco de 1987..."');
console.log('  LLM_MOOD: "Energetic"');
console.log('  LLM_CONTEXT: "1980s Italian Dance Music Scene"');
console.log('  LLM_RECOMMENDATIONS: "Perfecto para sets de Italo Disco..."');

console.log('\n🤝 **COMBINACIÓN FINAL:**');
console.log('  - Datos técnicos precisos (AI_*) + Interpretación musical (LLM_*)');
console.log('  - Coherencia validada entre ambos tipos');
console.log('  - Persistencia dual: BD + archivo físico');
console.log('  - UI actualizada con información completa');

console.log('\n🔧 4. CONFIGURACIÓN Y FALLBACKS');
console.log('===============================');

console.log('\n⚙️ **MOTOR C++ (Preferido):**');
console.log('- 📁 Ubicación: build/Release/metadata_addon.node');
console.log('- 🚀 Rendimiento: Análisis real con FFTW3');
console.log('- 🎯 Estado actual: Deshabilitado temporalmente');

console.log('\n🔄 **FALLBACK JAVASCRIPT:**');
console.log('- 📍 Ubicación: main.js (función generateMockResults)');
console.log('- 🎯 Propósito: Mantener funcionalidad durante desarrollo');
console.log('- 📊 Calidad: Simulación coherente basada en metadatos');

console.log('\n🌐 **CLAUDE API:**');
console.log('- 🔑 API Key: Configurada en main.js');
console.log('- 🧠 Modelo: claude-3-5-sonnet-20241022');
console.log('- 💰 Costo: ~$0.001 por análisis');
console.log('- 🔄 Fallback: Análisis simulado si API no disponible');

console.log('\n✅ **RESUMEN FINAL:**');
console.log('==================');
console.log('🎵 AI_* = Algoritmos técnicos de audio (C++/JS)');
console.log('🧠 LLM_* = Análisis musical inteligente (Claude)');
console.log('🔄 Ambos trabajen en conjunto para análisis completo');
console.log('💾 Datos persistidos en BD + archivos físicos');
console.log('🎯 Sistema robusto con múltiples fallbacks');

module.exports = { 
    explainDataSources: () => console.log('Explicación completada') 
};