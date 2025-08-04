#!/usr/bin/env node

const { execSync } = require('child_process');
const path = require('path');

console.log('🚨 LIMPIEZA DE EMERGENCIA: Eliminando metadatos corruptos');

// Obtener archivos de la base de datos
const filesOutput = execSync('sqlite3 music_analyzer.db "SELECT file_path FROM audio_files;"', { encoding: 'utf8' });
const files = filesOutput.trim().split('\n').filter(f => f).map(f => ({ file_path: f }));

console.log(`📁 Total archivos a limpiar: ${files.length}`);

let cleaned = 0;
let errors = 0;

for (const file of files) {
    const filePath = file.file_path;
    const ext = path.extname(filePath).toLowerCase();
    
    try {
        if (ext === '.flac') {
            // Para FLAC - eliminar TODOS los tags problemáticos pero preservar básicos
            console.log(`🧹 Limpiando FLAC: ${path.basename(filePath)}...`);
            
            // 1. Leer metadatos básicos ANTES de limpiar
            let basicMeta = {};
            try {
                const currentTags = execSync(`metaflac --export-tags-to=- "${filePath}"`, { encoding: 'utf8' });
                const lines = currentTags.split('\n');
                for (const line of lines) {
                    if (line.startsWith('ARTIST=')) basicMeta.artist = line.split('=')[1];
                    if (line.startsWith('TITLE=')) basicMeta.title = line.split('=')[1];  
                    if (line.startsWith('ALBUM=')) basicMeta.album = line.split('=')[1];
                    if (line.startsWith('DATE=')) basicMeta.date = line.split('=')[1];
                    if (line.startsWith('TRACKNUMBER=')) basicMeta.track = line.split('=')[1];
                    if (line.startsWith('BPM=')) basicMeta.bpm = line.split('=')[1];
                    if (line.startsWith('INITIALKEY=')) basicMeta.key = line.split('=')[1];
                }
            } catch (readError) {
                console.log(`⚠️ No se pudieron leer metadatos básicos de ${path.basename(filePath)}`);
            }
            
            // 2. Eliminar TODOS los tags corruptos
            const corruptedTags = [
                'ACOUSTICNESS_LLM', 'AI_ANALYSIS_DATE', 'AI_ANALYSIS_ENGINE', 'AI_BPM', 'AI_ENERGY',
                'AI_GENRE', 'AI_SUBGENRE_LLM', 'BPM_LLM', 'CHARACTERISTICS_LLM', 'CONFIDENCE_LLM',
                'CULTURAL_CONTEXT_LLM', 'DANCEABILITY_LLM', 'ENERGY_LLM', 'ERA_LLM', 'INSTRUMENTALNESS_LLM',
                'KEY_LLM', 'LIVENESS_LLM', 'LLM_ANALYSIS_DATE', 'LLM_ANALYSIS_ENGINE', 'LOUDNESS_LLM',
                'MODE_LLM', 'MOOD_LLM', 'OCCASION_LLM', 'SPEECHINESS_LLM', 'SUBGENRES_LLM',
                'TIME_SIGNATURE_LLM', 'VALENCE_LLM', 'GENRE',
                // Campos AI_* principales
                'AI_ACOUSTICNESS', 'AI_ANALYZED', 'AI_CHARACTERISTICS', 'AI_CONFIDENCE',
                'AI_CULTURAL_CONTEXT', 'AI_DANCEABILITY', 'AI_ERA', 'AI_INSTRUMENTALNESS',
                'AI_KEY', 'AI_LIVENESS', 'AI_LOUDNESS', 'AI_MODE', 'AI_MOOD', 'AI_OCCASION',
                'AI_SPEECHINESS', 'AI_SUBGENRES', 'AI_TIME_SIGNATURE', 'AI_VALENCE'
            ];
            
            for (const tag of corruptedTags) {
                try {
                    execSync(`metaflac --remove-tag="${tag}" "${filePath}"`, { stdio: 'ignore' });
                } catch (removeError) {
                    // Ignorar si el tag no existe
                }
            }
            
            // 3. Restaurar metadatos básicos si los teníamos
            if (basicMeta.artist) {
                execSync(`metaflac --set-tag="ARTIST=${basicMeta.artist}" "${filePath}"`, { stdio: 'ignore' });
            }
            if (basicMeta.title) {
                execSync(`metaflac --set-tag="TITLE=${basicMeta.title}" "${filePath}"`, { stdio: 'ignore' });
            }
            if (basicMeta.album) {
                execSync(`metaflac --set-tag="ALBUM=${basicMeta.album}" "${filePath}"`, { stdio: 'ignore' });
            }
            if (basicMeta.date) {
                execSync(`metaflac --set-tag="DATE=${basicMeta.date}" "${filePath}"`, { stdio: 'ignore' });
            }
            if (basicMeta.track) {
                execSync(`metaflac --set-tag="TRACKNUMBER=${basicMeta.track}" "${filePath}"`, { stdio: 'ignore' });
            }
            if (basicMeta.bpm) {
                execSync(`metaflac --set-tag="BPM=${basicMeta.bpm}" "${filePath}"`, { stdio: 'ignore' });
            }
            if (basicMeta.key) {
                execSync(`metaflac --set-tag="INITIALKEY=${basicMeta.key}" "${filePath}"`, { stdio: 'ignore' });
            }
            
            cleaned++;
        }
        
        if (cleaned % 100 === 0) {
            console.log(`📊 Progreso: ${cleaned}/${files.length} archivos limpiados`);
        }
        
    } catch (error) {
        errors++;
        if (errors < 10) {
            console.error(`❌ Error limpiando ${path.basename(filePath)}: ${error.message}`);
        }
    }
}

// Limpiar también la base de datos completamente
console.log('🗄️ Limpiando base de datos...');
try {
    execSync('sqlite3 music_analyzer.db "DELETE FROM llm_metadata;"', { stdio: 'ignore' });
    console.log('✅ Base de datos limpiada');
} catch (dbError) {
    console.error('❌ Error limpiando base de datos:', dbError.message);
}

console.log('✅ LIMPIEZA DE EMERGENCIA COMPLETADA');
console.log(`📊 Archivos limpiados: ${cleaned}`);
console.log(`❌ Errores: ${errors}`);
console.log('🔄 El sistema está listo para un nuevo análisis LIMPIO');