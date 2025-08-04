#!/usr/bin/env node

const { execSync } = require('child_process');
const path = require('path');

console.log('🎯 LIMPIEZA SELECTIVA: Preservar metadatos originales, eliminar solo corruptos');

// Lista de campos problemáticos que se eliminan completamente
const problematicFields = [
    'AI_ACOUSTICNESS', 'AI_ANALYSIS_DATE', 'AI_ANALYSIS_ENGINE', 'AI_BPM', 'AI_ENERGY',
    'AI_GENRE', 'AI_SUBGENRE_LLM', 'BPM_LLM', 'CHARACTERISTICS_LLM', 'CONFIDENCE_LLM',
    'CULTURAL_CONTEXT_LLM', 'DANCEABILITY_LLM', 'ENERGY_LLM', 'ERA_LLM', 'INSTRUMENTALNESS_LLM',
    'KEY_LLM', 'LIVENESS_LLM', 'LLM_ANALYSIS_DATE', 'LLM_ANALYSIS_ENGINE', 'LOUDNESS_LLM',
    'MODE_LLM', 'MOOD_LLM', 'OCCASION_LLM', 'SPEECHINESS_LLM', 'SUBGENRES_LLM',
    'TIME_SIGNATURE_LLM', 'VALENCE_LLM',
    // Campos AI_* principales
    'AI_ANALYZED', 'AI_CHARACTERISTICS', 'AI_CONFIDENCE', 'AI_CULTURAL_CONTEXT', 
    'AI_DANCEABILITY', 'AI_ERA', 'AI_INSTRUMENTALNESS', 'AI_KEY', 'AI_LIVENESS', 
    'AI_LOUDNESS', 'AI_MODE', 'AI_MOOD', 'AI_OCCASION', 'AI_SPEECHINESS', 
    'AI_SUBGENRES', 'AI_TIME_SIGNATURE', 'AI_VALENCE',
    // Campos LLM adicionales
    'AI_CONTEXT', 'AI_DANCEABILITY_LLM', 'AI_DESCRIPTION', 'AI_ENERGY_LEVEL',
    'AI_MOOD_LLM', 'AI_OCCASIONS', 'AI_RECOMMENDATIONS'
];

// Obtener archivos de la base de datos
const filesOutput = execSync('sqlite3 music_analyzer.db "SELECT file_path FROM audio_files;"', { encoding: 'utf8' });
const files = filesOutput.trim().split('\n').filter(f => f).map(f => ({ file_path: f }));

console.log(`📁 Total archivos a procesar: ${files.length}`);

let cleaned = 0;
let errors = 0;
let preserved = 0;

for (const file of files) {
    const filePath = file.file_path;
    const ext = path.extname(filePath).toLowerCase();
    
    try {
        if (ext === '.flac') {
            console.log(`🔍 Analizando: ${path.basename(filePath)}...`);
            
            // PASO 1: LEER metadatos originales del archivo
            let originalMetadata = {};
            try {
                const currentTags = execSync(`metaflac --export-tags-to=- "${filePath}"`, { encoding: 'utf8' });
                const lines = currentTags.split('\n').filter(line => line.includes('='));
                
                for (const line of lines) {
                    const [key, ...valueParts] = line.split('=');
                    const value = valueParts.join('=');
                    originalMetadata[key] = value;
                }
                
                console.log(`  📖 Leídos ${Object.keys(originalMetadata).length} campos originales`);
            } catch (readError) {
                console.log(`  ⚠️ No se pudieron leer metadatos de ${path.basename(filePath)}`);
                continue;
            }
            
            // PASO 2: IDENTIFICAR qué preservar y qué eliminar
            const fieldsToPreserve = {};
            const fieldsToDelete = [];
            let hasCorruption = false;
            
            for (const [key, value] of Object.entries(originalMetadata)) {
                // Si es un campo problemático, marcarlo para eliminación
                if (problematicFields.includes(key)) {
                    fieldsToDelete.push(key);
                    if (value.includes(';')) {
                        hasCorruption = true;
                        console.log(`  🚨 Corrupción detectada en ${key}: ${value.substring(0, 50)}...`);
                    }
                } else {
                    // Campo básico: preservar solo si NO tiene concatenación
                    if (value.includes(';')) {
                        console.log(`  ⚠️ Campo básico corrupto: ${key}=${value}`);
                        // Para campos básicos corruptos, tomar solo la PRIMERA parte
                        const cleanValue = value.split(';')[0].trim();
                        if (cleanValue && cleanValue !== '') {
                            fieldsToPreserve[key] = cleanValue;
                            console.log(`  🔧 Limpiado: ${key}=${cleanValue}`);
                        }
                    } else {
                        // Campo limpio, preservar completo
                        fieldsToPreserve[key] = value;
                    }
                }
            }
            
            // PASO 3: APLICAR LIMPIEZA si hay corrupción
            if (hasCorruption || fieldsToDelete.length > 0) {
                console.log(`  🧹 Aplicando limpieza selectiva...`);
                
                // Eliminar solo campos problemáticos
                for (const fieldToDelete of fieldsToDelete) {
                    try {
                        execSync(`metaflac --remove-tag="${fieldToDelete}" "${filePath}"`, { stdio: 'ignore' });
                    } catch (removeError) {
                        // Ignorar si el tag no existe
                    }
                }
                
                // Limpiar campos básicos corruptos y reescribir valores limpios
                for (const [key, cleanValue] of Object.entries(fieldsToPreserve)) {
                    if (originalMetadata[key] && originalMetadata[key].includes(';')) {
                        // Reescribir campo básico con valor limpio
                        execSync(`metaflac --remove-tag="${key}" "${filePath}"`, { stdio: 'ignore' });
                        execSync(`metaflac --set-tag="${key}=${cleanValue.replace(/"/g, '\\"')}" "${filePath}"`, { stdio: 'ignore' });
                    }
                }
                
                console.log(`  ✅ Limpieza completada: ${fieldsToDelete.length} campos eliminados, ${Object.keys(fieldsToPreserve).length} preservados`);
                cleaned++;
                
            } else {
                console.log(`  👍 Archivo limpio, no requiere procesamiento`);
                preserved++;
            }
        }
        
        if (cleaned % 10 === 0 && cleaned > 0) {
            console.log(`📊 Progreso: ${cleaned + preserved}/${files.length} archivos procesados`);
        }
        
    } catch (error) {
        errors++;
        if (errors < 5) {
            console.error(`❌ Error procesando ${path.basename(filePath)}: ${error.message}`);
        }
    }
}

// Limpiar base de datos
console.log('🗄️ Limpiando base de datos...');
try {
    execSync('sqlite3 music_analyzer.db "DELETE FROM llm_metadata;"', { stdio: 'ignore' });
    console.log('✅ Tabla llm_metadata limpiada');
} catch (dbError) {
    console.error('❌ Error limpiando base de datos:', dbError.message);
}

console.log('✅ LIMPIEZA SELECTIVA COMPLETADA');
console.log(`📊 Archivos limpiados: ${cleaned}`);
console.log(`👍 Archivos ya limpios: ${preserved}`);
console.log(`❌ Errores: ${errors}`);
console.log('🔄 Metadatos originales preservados, solo corrupción eliminada');