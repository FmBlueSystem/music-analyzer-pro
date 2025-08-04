#!/usr/bin/env node

const { execSync } = require('child_process');
const MetadataWriter = require('./metadata-writer.js');
const path = require('path');

async function testGenreUpdate() {
    console.log('🧪 PRUEBA: Validación de actualización GENRE con LLM_GENRE');
    
    // Obtener un archivo de prueba de la base de datos
    const testFileQuery = execSync('sqlite3 music_analyzer.db "SELECT file_path FROM audio_files WHERE file_extension = \'.flac\' LIMIT 1;"', { encoding: 'utf8' });
    const testFilePath = testFileQuery.trim();
    
    if (!testFilePath) {
        console.log('❌ No se encontró archivo FLAC para prueba');
        return;
    }
    
    console.log(`📁 Archivo de prueba: ${path.basename(testFilePath)}`);
    
    // Ver metadatos actuales
    console.log('\n📋 ANTES - Metadatos actuales:');
    try {
        const beforeMeta = execSync(`metaflac --export-tags-to=- "${testFilePath}" | grep -E "^(GENRE|AI_GENRE|LLM_GENRE)="`, { encoding: 'utf8' });
        console.log(beforeMeta || 'No hay campos GENRE actualmente');
    } catch (error) {
        console.log('No hay campos GENRE actualmente');
    }
    
    // Crear metadatos LLM de prueba
    const testLLMMetadata = {
        LLM_GENRE: 'Electronic Dance Music',
        LLM_SUBGENRE: 'Progressive House',
        AI_BPM: 128,
        AI_ENERGY: 0.85
    };
    
    // Inicializar MetadataWriter
    const writer = new MetadataWriter();
    await writer.initMusicMetadata();
    
    console.log('\n🔧 Escribiendo metadatos de prueba...');
    const result = await writer.writeLLMMetadataSafe(testFilePath, testLLMMetadata, true);
    
    if (result.success) {
        console.log('✅ Escritura exitosa');
        
        // Ver metadatos después
        console.log('\n📋 DESPUÉS - Metadatos actualizados:');
        try {
            const afterMeta = execSync(`metaflac --export-tags-to=- "${testFilePath}" | grep -E "^(GENRE|AI_GENRE|LLM_GENRE)="`, { encoding: 'utf8' });
            console.log(afterMeta);
            
            // Validar que GENRE se actualizó
            if (afterMeta.includes('GENRE=Electronic Dance Music')) {
                console.log('✅ VALIDACIÓN EXITOSA: GENRE se actualizó con LLM_GENRE');
            } else {
                console.log('❌ VALIDACIÓN FALLIDA: GENRE no se actualizó correctamente');
            }
            
        } catch (error) {
            console.log('❌ Error leyendo metadatos después de escritura');
        }
        
    } else {
        console.log(`❌ Error en escritura: ${result.error}`);
    }
}

testGenreUpdate().catch(console.error);