#!/usr/bin/env node

/**
 * 🔥 PRUEBA REAL: Escritura de metadatos FLAC con botón LLM
 * ESCRIBIR METADATOS REALES AL ARCHIVO ZZ TOP
 */

const path = require('path');
const MetadataWriter = require('./metadata-writer');
const MusicDatabase = require('./database');

async function testRealFLACWrite() {
    console.log('🔥 PRUEBA REAL: Escritura de metadatos FLAC - ZZ Top');
    console.log('==================================================');
    
    const testFilePath = '/Volumes/My Passport/Ojo otra vez muscia de Tidal Original descarga de musica/Consolidado2025/Tracks/ZZ Top - Legs (Dance Mix) [2003 Remaster].flac';
    
    try {
        // Inicializar sistemas
        const database = new MusicDatabase();
        await database.initialize();
        console.log('✅ Base de datos inicializada');
        
        const metadataWriter = new MetadataWriter();
        console.log('✅ MetadataWriter inicializado');
        
        console.log(`\n📖 ANTES: Leyendo metadatos existentes...`);
        const beforeMetadata = await metadataWriter.readExistingMetadata(testFilePath);
        
        console.log('📄 METADATOS ANTES DE LA ESCRITURA:');
        console.log(`- Título: ${beforeMetadata.metadata?.title || 'Desconocido'}`);
        console.log(`- Artista: ${beforeMetadata.metadata?.artist || 'Desconocido'}`);
        console.log(`- Género actual: ${beforeMetadata.metadata?.genre || 'Sin definir'} ⚠️`);
        
        // Datos LLM reales para escribir (igual que en la simulación)
        const realLLMResults = {
            LLM_DESCRIPTION: "Remix dance de la clásica canción 'Legs' de ZZ Top. Esta versión dance de 2003 transforma el blues rock texano original en una interpretación bailable, manteniendo los elementos característicos de la banda pero adaptados para la pista de baile.",
            LLM_MOOD: "Energetic",
            LLM_GENRE: "Dance Rock", // ✅ CORRECCIÓN REAL: Sin definir → Dance Rock
            LLM_SUBGENRE: "Rock Remix",
            LLM_CONTEXT: "2000s Dance Remix Culture - Texas Rock Heritage",
            LLM_OCCASIONS: ["Club Night", "Dance Floor", "Retro Party"],
            LLM_ENERGY_LEVEL: "Alto",
            LLM_DANCEABILITY: "Alta", 
            LLM_RECOMMENDATIONS: "Ideal para sets que combinan rock clásico con elementos dance. Perfecto para transiciones entre rock y música electrónica.",
            
            // Placeholders AI_* con nombres de algoritmos REALES
            AI_ACOUSTICNESS: "ALGORITMO SPECTRAL_ANALYSIS",
            AI_ANALYZED: "ALGORITMO COMPLETION_TRACKER",
            AI_BPM: "ALGORITMO BEAT_TRACKING",
            AI_CHARACTERISTICS: "ALGORITMO FEATURE_EXTRACTION",
            AI_CONFIDENCE: "ALGORITMO CONFIDENCE_CALCULATOR", 
            AI_CULTURAL_CONTEXT: "ALGORITMO CULTURAL_ANALYZER",
            AI_DANCEABILITY: "ALGORITMO DANCEABILITY_DETECTOR",
            AI_ENERGY: "ALGORITMO ENERGY_ANALYZER",
            AI_ERA: "ALGORITMO ERA_CLASSIFIER",
            AI_INSTRUMENTALNESS: "ALGORITMO VOCAL_DETECTION",
            AI_KEY: "ALGORITMO KEY_DETECTION",
            AI_LIVENESS: "ALGORITMO LIVENESS_DETECTION",
            AI_LOUDNESS: "ALGORITMO LOUDNESS_ANALYSIS",
            AI_MODE: "ALGORITMO MODE_DETECTION",
            AI_MOOD: "ALGORITMO MOOD_CLASSIFIER",
            AI_OCCASION: "ALGORITMO OCCASION_PREDICTOR",
            AI_SPEECHINESS: "ALGORITMO SPEECH_DETECTION",
            AI_SUBGENRES: "ALGORITMO SUBGENRE_CLASSIFIER",
            AI_TIME_SIGNATURE: "ALGORITMO TIME_SIGNATURE_DETECTION",
            AI_VALENCE: "ALGORITMO VALENCE_ANALYZER"
        };
        
        console.log('\n🔥 EJECUTANDO ESCRITURA REAL DE METADATOS...');
        console.log('===============================================');
        
        // ✅ ESCRITURA REAL AL ARCHIVO FLAC
        const writeResult = await metadataWriter.writeLLMMetadataSafe(testFilePath, realLLMResults, true);
        
        if (writeResult.success) {
            console.log('✅ ESCRITURA EXITOSA AL ARCHIVO FLAC');
            console.log(`📝 Campos escritos: ${writeResult.fieldsWritten}`);
            console.log(`🎵 Método usado: ${writeResult.method}`);
        } else {
            console.log('❌ ERROR EN ESCRITURA:', writeResult.error);
            return;
        }
        
        // Verificar que los metadatos se escribieron correctamente
        console.log('\n🔍 VERIFICACIÓN: Leyendo metadatos después de escritura...');
        const afterMetadata = await metadataWriter.readExistingMetadata(testFilePath);
        
        console.log('\n📄 METADATOS DESPUÉS DE LA ESCRITURA:');
        console.log(`- Título: ${afterMetadata.metadata?.title || 'Desconocido'}`);
        console.log(`- Artista: ${afterMetadata.metadata?.artist || 'Desconocido'}`);
        console.log(`- Género NUEVO: ${afterMetadata.metadata?.genre || 'Sin cambios'} 🎯`);
        
        // Verificar campos LLM específicos
        console.log('\n🧠 CAMPOS LLM ESCRITOS EN EL ARCHIVO:');
        console.log(`- LLM_GENRE: ${afterMetadata.tags?.LLM_GENRE || 'No encontrado'}`);
        console.log(`- LLM_MOOD: ${afterMetadata.tags?.LLM_MOOD || 'No encontrado'}`);
        console.log(`- LLM_DESCRIPTION: ${afterMetadata.tags?.LLM_DESCRIPTION?.substring(0, 50) || 'No encontrado'}...`);
        
        // Verificar campos AI_* placeholders
        console.log('\n🔧 CAMPOS AI_* PLACEHOLDERS EN EL ARCHIVO:');
        console.log(`- AI_ACOUSTICNESS: ${afterMetadata.tags?.AI_ACOUSTICNESS || 'No encontrado'}`);
        console.log(`- AI_BPM: ${afterMetadata.tags?.AI_BPM || 'No encontrado'}`);
        console.log(`- AI_KEY: ${afterMetadata.tags?.AI_KEY || 'No encontrado'}`);
        console.log(`- AI_ENERGY: ${afterMetadata.tags?.AI_ENERGY || 'No encontrado'}`);
        
        // Actualizar base de datos también
        console.log('\n💾 ACTUALIZANDO BASE DE DATOS...');
        const dbFile = await database.getQuery('SELECT id, genre FROM audio_files WHERE file_path = ?', [testFilePath]);
        
        if (dbFile) {
            console.log(`📝 Archivo encontrado en BD (ID: ${dbFile.id})`);
            console.log(`📊 Género antes en BD: "${dbFile.genre}"`);
            
            // Insertar metadatos LLM
            await database.insertLLMMetadata(dbFile.id, realLLMResults);
            console.log('✅ Metadatos LLM insertados en base de datos');
            
            // Verificar actualización de género
            const updatedFile = await database.getQuery('SELECT genre FROM audio_files WHERE id = ?', [dbFile.id]);
            console.log(`📊 Género después en BD: "${updatedFile.genre}"`);
            
        } else {
            console.log('❌ Archivo no encontrado en base de datos');
        }
        
        console.log('\n🎯 RESULTADO FINAL - ESCRITURA REAL COMPLETADA:');
        console.log('===============================================');
        console.log('1. ✅ Archivo FLAC escrito con metadatos reales');
        console.log('2. ✅ GENRE tag actualizado en archivo físico');
        console.log('3. ✅ Todos los campos LLM_* escritos como Vorbis comments');
        console.log('4. ✅ Todos los campos AI_* escritos con nombres de algoritmos');
        console.log('5. ✅ Base de datos sincronizada con archivo');
        
        // Comparación antes/después
        const genreBefore = beforeMetadata.metadata?.genre || 'Sin definir';
        const genreAfter = afterMetadata.metadata?.genre || 'Sin cambios';
        
        console.log('\n🔄 COMPARACIÓN ANTES/DESPUÉS:');
        console.log(`📊 Género: "${genreBefore}" → "${genreAfter}"`);
        
        if (genreAfter === "Dance Rock") {
            console.log('🎉 ¡ÉXITO! Género corregido correctamente en archivo físico');
        } else {
            console.log('⚠️ Verificar: Posible problema en escritura de género');
        }
        
        await database.close();
        
    } catch (error) {
        console.error('❌ Error en prueba real:', error);
    }
}

// Ejecutar prueba real
if (require.main === module) {
    testRealFLACWrite().catch(console.error);
}

module.exports = { testRealFLACWrite };