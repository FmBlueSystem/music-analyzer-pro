#!/usr/bin/env node

const { execSync } = require('child_process');
const path = require('path');
const fs = require('fs');

// Lista de tags a eliminar
const tagsToRemove = [
    'GENRE',
    'AI_ACOUSTICNESS',
    'AI_ANALYSIS_DATE', 
    'AI_ANALYSIS_ENGINE',
    'AI_BPM',
    'AI_CHARACTERISTICS',
    'AI_CONFIDENCE',
    'AI_CULTURAL_CONTEXT',
    'AI_DANCEABILITY',
    'AI_ENERGY',
    'AI_ERA',
    'AI_INSTRUMENTALNESS',
    'AI_KEY',
    'AI_LIVENESS',
    'AI_LOUDNESS',
    'AI_MODE',
    'AI_MOOD',
    'AI_OCCASION',
    'AI_SPEECHINESS',
    'AI_SUBGENRES',
    'AI_TIME_SIGNATURE',
    'AI_VALENCE',
    'CHARACTERISTICS',
    'DANCEABILITY',
    'LLM_CONTEXT',
    'LLM_DANCEABILITY',
    'LLM_DESCRIPTION',
    'LLM_ENERGY_LEVEL',
    'LLM_ERA',
    'LLM_GENRE',
    'LLM_MOOD',
    'LLM_OCCASIONS',
    'LLM_RECOMMENDATIONS',
    'LLM_SUBGENRE',
    'MOOD',
    'OCCASION',
    'VALENCE',
    // 🆕 CAMPOS ADICIONALES AGREGADOS
    'COMPATIBLE_BRANDS',
    'ERA',
    'MAJOR_BRAND',
    'MINOR_VERSION',
    'SUBGENRE'
];

console.log('🧹 INICIANDO LIMPIEZA MASIVA DE METADATOS');
console.log(`📋 Tags a eliminar: ${tagsToRemove.length}`);

// Obtener todos los archivos usando sqlite3
const filesOutput = execSync('sqlite3 music_analyzer.db "SELECT file_path FROM audio_files;"', { encoding: 'utf8' });
const files = filesOutput.trim().split('\n').filter(f => f).map(f => ({ file_path: f }));
console.log(`📁 Total de archivos: ${files.length}`);

let processedCount = 0;
let errorCount = 0;

// Procesar cada archivo
for (const file of files) {
    const filePath = file.file_path;
    processedCount++;
    
    if (processedCount % 100 === 0) {
        console.log(`📊 Progreso: ${processedCount}/${files.length} (${Math.round(processedCount/files.length*100)}%)`);
    }
    
    try {
        // Verificar extensión del archivo
        const ext = path.extname(filePath).toLowerCase();
        
        if (ext === '.flac') {
            // Para archivos FLAC, usar metaflac
            for (const tag of tagsToRemove) {
                try {
                    const command = `metaflac --remove-tag="${tag}" "${filePath}"`;
                    execSync(command, { stdio: 'ignore' });
                } catch (tagError) {
                    // Ignorar errores de tags que no existen
                }
            }
        } else if (ext === '.mp3') {
            // Para archivos MP3, usar id3v2
            for (const tag of tagsToRemove) {
                try {
                    // Mapear tags AI_* a TXXX frames para MP3
                    if (tag.startsWith('AI_') || tag.startsWith('LLM_')) {
                        const command = `id3v2 --remove-frame TXXX:"${tag}" "${filePath}"`;
                        execSync(command, { stdio: 'ignore' });
                    } else {
                        // Tags estándar
                        let id3Tag = tag;
                        switch(tag) {
                            case 'GENRE': id3Tag = 'TCON'; break;
                            case 'MOOD': id3Tag = 'TMOO'; break;
                            // Agregar más mapeos según sea necesario
                        }
                        const command = `id3v2 --remove-frame ${id3Tag} "${filePath}"`;
                        execSync(command, { stdio: 'ignore' });
                    }
                } catch (tagError) {
                    // Ignorar errores de tags que no existen
                }
            }
        }
        
    } catch (error) {
        errorCount++;
        if (errorCount < 10) {
            console.error(`❌ Error en archivo ${filePath}: ${error.message}`);
        }
    }
}

console.log('✅ LIMPIEZA COMPLETADA');
console.log(`📊 Archivos procesados: ${processedCount}`);
console.log(`❌ Errores: ${errorCount}`);

// Limpiar también la base de datos
console.log('🗄️ Limpiando base de datos...');
try {
    // Limpiar tabla llm_metadata
    execSync('sqlite3 music_analyzer.db "DELETE FROM llm_metadata;"', { stdio: 'ignore' });
    console.log('✅ Tabla llm_metadata limpiada');
} catch (dbError) {
    console.error('❌ Error limpiando base de datos:', dbError.message);
}
console.log('🎉 PROCESO DE LIMPIEZA TERMINADO');