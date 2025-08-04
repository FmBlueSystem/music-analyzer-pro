#!/usr/bin/env node

const { execSync } = require('child_process');
const path = require('path');

console.log('🛡️ LIMPIEZA ROBUSTA: Eliminación completa de metadatos corruptos');
console.log('🎯 ESTRATEGIA: Eliminación total + reconstrucción desde base de datos');

// Obtener archivos de la base de datos con información básica
const filesQuery = `
SELECT 
    file_path,
    file_name,
    artist,
    title,
    album,
    year
FROM audio_files;
`;

console.log('📋 Obteniendo información básica desde base de datos...');
const filesOutput = execSync(`sqlite3 music_analyzer.db "${filesQuery}"`, { encoding: 'utf8' });
const fileLines = filesOutput.trim().split('\n').filter(f => f);

console.log(`📁 Total archivos a procesar: ${fileLines.length}`);

let cleaned = 0;
let errors = 0;

for (const fileLine of fileLines) {
    const [filePath, fileName, artist, title, album, year] = fileLine.split('|');
    
    if (!filePath || !fileName) {
        continue;
    }
    
    const ext = path.extname(filePath).toLowerCase();
    
    try {
        if (ext === '.flac') {
            console.log(`🧹 Procesando: ${fileName}...`);
            
            // PASO 1: ELIMINACIÓN TOTAL DE TODOS LOS TAGS
            console.log(`  🗑️  Eliminando TODOS los tags...`);
            execSync(`metaflac --remove-all-tags "${filePath}"`, { stdio: 'ignore' });
            
            // PASO 2: RECONSTRUCCIÓN INTELIGENTE DESDE BD
            console.log(`  🔧 Reconstruyendo metadatos básicos...`);
            
            // Solo agregar metadatos que NO contengan punto y coma
            const safeMetadata = [];
            
            if (artist && !artist.includes(';')) {
                safeMetadata.push(`ARTIST=${artist.replace(/"/g, '\\"')}`);
            }
            
            if (title && !title.includes(';')) {
                safeMetadata.push(`TITLE=${title.replace(/"/g, '\\"')}`);
            }
            
            if (album && !album.includes(';')) {
                safeMetadata.push(`ALBUM=${album.replace(/"/g, '\\"')}`);
            }
            
            if (year && !year.includes(';')) {
                // Usar el año directamente de la base de datos
                if (year.length === 4 && !isNaN(year)) {
                    safeMetadata.push(`DATE=${year}`);
                }
            }
            
            // Escribir solo metadatos seguros
            for (const metadata of safeMetadata) {
                const command = `metaflac --set-tag="${metadata}" "${filePath}"`;
                execSync(command, { stdio: 'ignore' });
            }
            
            console.log(`  ✅ Completado: ${safeMetadata.length} campos seguros escritos`);
            cleaned++;
        }
        
        if (cleaned % 10 === 0 && cleaned > 0) {
            console.log(`📊 Progreso: ${cleaned}/${fileLines.length} archivos procesados`);
        }
        
    } catch (error) {
        errors++;
        if (errors < 5) {
            console.error(`❌ Error procesando ${fileName}: ${error.message}`);
        }
    }
}

// PASO 3: LIMPIAR BASE DE DATOS COMPLETAMENTE
console.log('🗄️ Limpiando base de datos...');
try {
    execSync('sqlite3 music_analyzer.db "DELETE FROM llm_metadata;"', { stdio: 'ignore' });
    console.log('✅ Tabla llm_metadata limpiada completamente');
} catch (dbError) {
    console.error('❌ Error limpiando base de datos:', dbError.message);
}

console.log('✅ LIMPIEZA ROBUSTA COMPLETADA');
console.log(`📊 Archivos procesados exitosamente: ${cleaned}`);
console.log(`❌ Errores encontrados: ${errors}`);
console.log('🔄 Sistema preparado para análisis limpio');
console.log('');
console.log('🎯 PRÓXIMOS PASOS:');
console.log('1. Verificar que los archivos tienen solo metadatos básicos');
console.log('2. Ejecutar análisis controlado de algunos archivos');
console.log('3. Confirmar que no hay concatenación');
console.log('4. Solo entonces proceder con análisis masivo');