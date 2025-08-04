const MusicDatabase = require('./database');

async function checkDuplicates() {
    const database = new MusicDatabase();
    await database.initialize();
    
    console.log('🔍 Analizando duplicados en la base de datos...');
    
    // 1. Buscar duplicados por file_path
    const pathDuplicates = await database.allQuery(`
        SELECT file_path, COUNT(*) as count 
        FROM audio_files 
        GROUP BY file_path 
        HAVING count > 1
        ORDER BY count DESC
    `);
    
    console.log(`📊 Duplicados por file_path: ${pathDuplicates.length}`);
    if (pathDuplicates.length > 0) {
        console.log('🚨 Rutas duplicadas encontradas:');
        pathDuplicates.forEach(dup => {
            console.log(`   - ${dup.file_path} (${dup.count} veces)`);
        });
    }
    
    // 2. Buscar duplicados por file_name + file_size (archivos idénticos)
    const contentDuplicates = await database.allQuery(`
        SELECT file_name, file_size, COUNT(*) as count 
        FROM audio_files 
        GROUP BY file_name, file_size 
        HAVING count > 1
        ORDER BY count DESC
    `);
    
    console.log(`📊 Duplicados por contenido (name+size): ${contentDuplicates.length}`);
    if (contentDuplicates.length > 0) {
        console.log('🚨 Archivos duplicados por contenido:');
        contentDuplicates.slice(0, 10).forEach(dup => {
            console.log(`   - ${dup.file_name} (${dup.file_size} bytes) - ${dup.count} veces`);
        });
    }
    
    // 3. Estadísticas generales
    const totalFiles = await database.getQuery('SELECT COUNT(*) as total FROM audio_files');
    const uniquePaths = await database.getQuery('SELECT COUNT(DISTINCT file_path) as unique_paths FROM audio_files');
    const uniqueNames = await database.getQuery('SELECT COUNT(DISTINCT file_name) as unique_names FROM audio_files');
    
    console.log('\n📈 Estadísticas:');
    console.log(`   Total registros: ${totalFiles.total}`);
    console.log(`   Rutas únicas: ${uniquePaths.unique_paths}`);
    console.log(`   Nombres únicos: ${uniqueNames.unique_names}`);
    console.log(`   Duplicados de ruta: ${totalFiles.total - uniquePaths.unique_paths}`);
    
    // 4. Buscar registros con misma ruta pero diferente ID
    if (pathDuplicates.length > 0) {
        console.log('\n🔎 Detalles de registros duplicados:');
        for (const dup of pathDuplicates.slice(0, 5)) {
            const records = await database.allQuery(`
                SELECT id, file_path, date_added, analysis_status 
                FROM audio_files 
                WHERE file_path = ?
                ORDER BY id
            `, [dup.file_path]);
            
            console.log(`\n📁 ${dup.file_path}:`);
            records.forEach(record => {
                console.log(`   ID: ${record.id}, Agregado: ${record.date_added}, Status: ${record.analysis_status}`);
            });
        }
    }
    
    await database.close();
}

checkDuplicates().catch(console.error);