#!/usr/bin/env node

const { execSync } = require('child_process');
const path = require('path');

// Campos adicionales a eliminar
const additionalTags = [
    'COMPATIBLE_BRANDS',
    'ERA', 
    'MAJOR_BRAND',
    'MINOR_VERSION',
    'SUBGENRE'
];

console.log('🆕 LIMPIEZA ADICIONAL DE METADATOS');
console.log(`📋 Tags adicionales a eliminar: ${additionalTags.join(', ')}`);

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
    
    if (processedCount % 500 === 0) {
        console.log(`📊 Progreso: ${processedCount}/${files.length} (${Math.round(processedCount/files.length*100)}%)`);
    }
    
    try {
        const ext = path.extname(filePath).toLowerCase();
        
        if (ext === '.flac') {
            // Para archivos FLAC, usar metaflac
            for (const tag of additionalTags) {
                try {
                    const command = `metaflac --remove-tag="${tag}" "${filePath}"`;
                    execSync(command, { stdio: 'ignore' });
                } catch (tagError) {
                    // Ignorar errores de tags que no existen
                }
            }
        } else if (ext === '.mp3') {
            // Para archivos MP3, usar id3v2  
            for (const tag of additionalTags) {
                try {
                    let id3Tag = tag;
                    switch(tag) {
                        case 'ERA': id3Tag = 'TYER'; break;
                        case 'SUBGENRE': id3Tag = 'TCON'; break;
                        default:
                            // Campos personalizados como TXXX
                            const command = `id3v2 --remove-frame TXXX:"${tag}" "${filePath}"`;
                            execSync(command, { stdio: 'ignore' });
                            continue;
                    }
                    const command = `id3v2 --remove-frame ${id3Tag} "${filePath}"`;
                    execSync(command, { stdio: 'ignore' });
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

console.log('✅ LIMPIEZA ADICIONAL COMPLETADA');
console.log(`📊 Archivos procesados: ${processedCount}`);
console.log(`❌ Errores: ${errorCount}`);
console.log(`🆕 Campos eliminados: ${additionalTags.join(', ')}`);