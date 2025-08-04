const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const fs = require('fs');

class MusicDatabase {
    constructor() {
        this.db = null;
        this.dbPath = path.join(__dirname, 'music_analyzer.db');
        this.isInitialized = false;
    }

    /**
     * 🚀 Inicializar base de datos con esquema optimizado para 10K+ archivos
     */
    async initialize() {
        if (this.isInitialized) return;

        return new Promise((resolve, reject) => {
            // Crear base de datos SQLite
            this.db = new sqlite3.Database(this.dbPath, (err) => {
                if (err) {
                    console.error('❌ Error creando base de datos:', err);
                    reject(err);
                    return;
                }
                
                console.log('✅ Base de datos SQLite conectada');
                this.createTables()
                    .then(() => this.migrateLLMFields())
                    .then(() => {
                        this.isInitialized = true;
                        resolve();
                    })
                    .catch(reject);
            });
        });
    }

    /**
     * 🔄 Migrar base de datos - agregar campos LLM si no existen
     */
    async migrateLLMFields() {
        try {
            // Verificar si las columnas LLM ya existen
            const tableInfo = await this.allQuery("PRAGMA table_info(llm_metadata)");
            const existingColumns = tableInfo.map(col => col.name);
            
            const llmFields = [
                'LLM_DESCRIPTION', 'LLM_MOOD', 'LLM_GENRE', 'LLM_SUBGENRE', 'LLM_CONTEXT',
                'LLM_OCCASIONS', 'LLM_ENERGY_LEVEL', 'LLM_DANCEABILITY', 'LLM_RECOMMENDATIONS',
                'LLM_ANALYZED', 'LLM_ANALYSIS_DATE'
            ];
            
            for (const field of llmFields) {
                if (!existingColumns.includes(field)) {
                    let columnType = 'TEXT';
                    if (field === 'LLM_ANALYZED') {
                        columnType = 'BOOLEAN DEFAULT 0';
                    }
                    
                    await this.runQuery(`ALTER TABLE llm_metadata ADD COLUMN ${field} ${columnType}`);
                    console.log(`✅ Agregada columna LLM: ${field}`);
                }
            }
            
            console.log('✅ Migración de campos LLM completada');
        } catch (error) {
            console.error('❌ Error en migración LLM:', error);
            throw error;
        }
    }

    /**
     * 📋 Crear tablas optimizadas para rendimiento
     */
    async createTables() {
        const tables = [
            // Tabla principal de archivos de audio
            `CREATE TABLE IF NOT EXISTS audio_files (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_path TEXT UNIQUE NOT NULL,
                file_name TEXT NOT NULL,
                file_size INTEGER,
                file_extension TEXT,
                date_modified TEXT,
                date_added TEXT DEFAULT CURRENT_TIMESTAMP,
                folder_path TEXT,
                
                -- Metadatos básicos
                title TEXT,
                artist TEXT,
                album TEXT,
                genre TEXT,
                year TEXT,
                
                -- Mixed In Key detection
                mixed_in_key_detected BOOLEAN DEFAULT 0,
                existing_bmp REAL,
                existing_key TEXT,
                preservation_source TEXT,
                should_preserve BOOLEAN DEFAULT 0,
                
                -- Status
                analysis_status TEXT DEFAULT 'pending', -- pending, analyzing, completed, error
                last_analyzed TEXT,
                
                -- File hash for integrity
                file_hash TEXT,
                
                -- Indexing para búsquedas rápidas
                FOREIGN KEY (folder_path) REFERENCES folders(path)
            )`,

            // Tabla de metadatos LLM (separada para optimización)
            `CREATE TABLE IF NOT EXISTS llm_metadata (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_id INTEGER NOT NULL,
                
                -- ✅ CAMPOS AI_* PRINCIPALES (nuevos estándares)
                AI_ACOUSTICNESS REAL,
                AI_ANALYZED BOOLEAN DEFAULT 1,
                AI_BPM REAL,
                AI_CHARACTERISTICS TEXT, -- JSON array
                AI_CONFIDENCE REAL,
                AI_CULTURAL_CONTEXT TEXT,
                AI_DANCEABILITY REAL,
                AI_ENERGY REAL,
                AI_ERA TEXT,
                AI_INSTRUMENTALNESS REAL,
                AI_KEY TEXT,
                AI_LIVENESS REAL,
                AI_LOUDNESS REAL,
                AI_MODE TEXT,
                AI_MOOD TEXT,
                AI_OCCASION TEXT, -- JSON array
                AI_SPEECHINESS REAL,
                AI_SUBGENRES TEXT, -- JSON array
                AI_TIME_SIGNATURE INTEGER,
                AI_VALENCE REAL,
                
                -- 🧠 CAMPOS LLM (análisis con Claude)
                LLM_DESCRIPTION TEXT,
                LLM_MOOD TEXT,
                LLM_GENRE TEXT,
                LLM_SUBGENRE TEXT,
                LLM_CONTEXT TEXT,
                LLM_OCCASIONS TEXT, -- JSON array
                LLM_ENERGY_LEVEL TEXT,
                LLM_DANCEABILITY TEXT,
                LLM_RECOMMENDATIONS TEXT,
                LLM_ANALYZED BOOLEAN DEFAULT 0,
                LLM_ANALYSIS_DATE TEXT,
                
                -- 🔄 CAMPOS LEGACY (compatibilidad hacia atrás)
                bpm_llm REAL,
                energy INTEGER,
                mood TEXT,
                danceability REAL,
                valence REAL,
                analyzed_by TEXT,
                
                -- Metadatos detallados para DJs (legacy)
                subgenre TEXT,
                era TEXT,
                vocal_presence TEXT,
                structure TEXT,
                drop_time TEXT,
                energy_curve TEXT,
                crowd_response TEXT,
                occasion TEXT,
                characteristics TEXT,
                
                -- Metadatos técnicos (legacy)
                tempo_stability REAL,
                production_quality REAL,
                mastering_loudness REAL,
                dynamic_range REAL,
                
                -- HAMMS vector (JSON)
                hamms_vector TEXT, -- JSON string
                
                -- Custom tags (JSON array)
                custom_tags TEXT, -- JSON string
                
                -- Metadata de análisis
                analysis_date TEXT DEFAULT CURRENT_TIMESTAMP,
                analysis_timestamp TEXT,
                llm_version TEXT DEFAULT '2.0.0',
                
                FOREIGN KEY (file_id) REFERENCES audio_files(id) ON DELETE CASCADE
            )`,

            // Tabla de carpetas para optimizar navegación
            `CREATE TABLE IF NOT EXISTS folders (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                path TEXT UNIQUE NOT NULL,
                name TEXT NOT NULL,
                parent_path TEXT,
                total_files INTEGER DEFAULT 0,
                analyzed_files INTEGER DEFAULT 0,
                last_scanned TEXT DEFAULT CURRENT_TIMESTAMP,
                is_favorite BOOLEAN DEFAULT 0
            )`,

            // Tabla de estadísticas para dashboard
            `CREATE TABLE IF NOT EXISTS statistics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                date TEXT DEFAULT CURRENT_TIMESTAMP,
                total_files INTEGER,
                analyzed_files INTEGER,
                mixed_in_key_files INTEGER,
                avg_analysis_time REAL,
                library_size_mb REAL
            )`,

            // Tabla de configuración de usuario
            `CREATE TABLE IF NOT EXISTS user_settings (
                key TEXT PRIMARY KEY,
                value TEXT,
                last_updated TEXT DEFAULT CURRENT_TIMESTAMP
            )`
        ];

        // Crear índices para optimización
        const indexes = [
            'CREATE INDEX IF NOT EXISTS idx_audio_files_path ON audio_files(file_path)',
            'CREATE INDEX IF NOT EXISTS idx_audio_files_folder ON audio_files(folder_path)',
            'CREATE INDEX IF NOT EXISTS idx_audio_files_status ON audio_files(analysis_status)',
            'CREATE INDEX IF NOT EXISTS idx_audio_files_mixed_key ON audio_files(mixed_in_key_detected)',
            'CREATE INDEX IF NOT EXISTS idx_audio_files_artist ON audio_files(artist)',
            'CREATE INDEX IF NOT EXISTS idx_audio_files_genre ON audio_files(genre)',
            'CREATE INDEX IF NOT EXISTS idx_llm_metadata_file_id ON llm_metadata(file_id)',
            'CREATE INDEX IF NOT EXISTS idx_llm_metadata_energy ON llm_metadata(energy)',
            'CREATE INDEX IF NOT EXISTS idx_llm_metadata_bpm ON llm_metadata(bpm_llm)',
            'CREATE INDEX IF NOT EXISTS idx_folders_path ON folders(path)'
        ];

        try {
            // Crear tablas
            for (const tableSQL of tables) {
                await this.runQuery(tableSQL);
            }
            
            // Crear índices
            for (const indexSQL of indexes) {
                await this.runQuery(indexSQL);
            }
            
            console.log('✅ Esquema de base de datos creado con índices optimizados');
            
            // Configurar WAL mode para mejor rendimiento con múltiples lecturas
            await this.runQuery('PRAGMA journal_mode=WAL');
            await this.runQuery('PRAGMA synchronous=NORMAL');
            await this.runQuery('PRAGMA cache_size=10000'); // 10MB cache
            await this.runQuery('PRAGMA temp_store=memory');
            
            console.log('⚡ Base de datos optimizada para alto rendimiento');
            
        } catch (error) {
            console.error('❌ Error creando esquema:', error);
            throw error;
        }
    }

    /**
     * 🔧 Ejecutar query con promesa
     */
    runQuery(sql, params = []) {
        return new Promise((resolve, reject) => {
            this.db.run(sql, params, function(err) {
                if (err) {
                    console.error('❌ SQL Error:', err.message);
                    console.error('📝 Query:', sql);
                    reject(err);
                } else {
                    resolve({ lastID: this.lastID, changes: this.changes });
                }
            });
        });
    }

    /**
     * 📖 Obtener todos los registros
     */
    allQuery(sql, params = []) {
        return new Promise((resolve, reject) => {
            this.db.all(sql, params, (err, rows) => {
                if (err) {
                    console.error('❌ SQL Error:', err.message);
                    reject(err);
                } else {
                    resolve(rows);
                }
            });
        });
    }

    /**
     * 📄 Obtener un registro
     */
    getQuery(sql, params = []) {
        return new Promise((resolve, reject) => {
            this.db.get(sql, params, (err, row) => {
                if (err) {
                    console.error('❌ SQL Error:', err.message);
                    reject(err);
                } else {
                    resolve(row);
                }
            });
        });
    }

    /**
     * 📁 Registrar carpeta en base de datos
     */
    async registerFolder(folderPath) {
        const folderName = path.basename(folderPath);
        const parentPath = path.dirname(folderPath);
        
        const sql = `
            INSERT OR REPLACE INTO folders (path, name, parent_path, last_scanned)
            VALUES (?, ?, ?, CURRENT_TIMESTAMP)
        `;
        
        try {
            const result = await this.runQuery(sql, [folderPath, folderName, parentPath]);
            console.log(`📁 Carpeta registrada: ${folderName}`);
            return result.lastID;
        } catch (error) {
            console.error('Error registrando carpeta:', error);
            throw error;
        }
    }

    /**
     * 🎵 Insertar o actualizar archivo de audio
     */
    async upsertAudioFile(fileData) {
        const {
            filePath, fileName, fileSize, fileExtension, dateModified,
            folderPath, title, artist, album, genre, year,
            mixedInKeyDetected, existingBpm, existingKey, preservationSource,
            shouldPreserve, fileHash
        } = fileData;

        const sql = `
            INSERT OR REPLACE INTO audio_files (
                file_path, file_name, file_size, file_extension, date_modified,
                folder_path, title, artist, album, genre, year,
                mixed_in_key_detected, existing_bmp, existing_key, preservation_source,
                should_preserve, file_hash, date_added
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 
                     COALESCE((SELECT date_added FROM audio_files WHERE file_path = ?), CURRENT_TIMESTAMP))
        `;

        const params = [
            filePath, fileName, fileSize, fileExtension, dateModified,
            folderPath, title, artist, album, genre, year,
            mixedInKeyDetected ? 1 : 0, existingBpm, existingKey, preservationSource,
            shouldPreserve ? 1 : 0, fileHash, filePath
        ];

        try {
            const result = await this.runQuery(sql, params);
            return result.lastID;
        } catch (error) {
            console.error('Error insertando archivo:', error);
            throw error;
        }
    }

    /**
     * 🤖 Insertar metadatos LLM
     */
    async insertLLMMetadata(fileId, llmData) {
        try {
            // 🔧 ESTRATEGIA: Usar UPDATE/INSERT híbrido para evitar problemas de columnas
            
            // 1. Primero intentar UPDATE de registros existentes
            const updateSql = `
                UPDATE llm_metadata SET
                    -- Campos LLM
                    LLM_DESCRIPTION = ?, LLM_MOOD = ?, LLM_GENRE = ?, LLM_SUBGENRE = ?, LLM_CONTEXT = ?,
                    LLM_ANALYZED = ?, LLM_ANALYSIS_DATE = ?,
                    -- Campos AI_* (reemplazo total)
                    AI_ACOUSTICNESS = ?, AI_ANALYZED = ?, AI_BPM = ?, AI_CHARACTERISTICS = ?, AI_CONFIDENCE = ?,
                    AI_CULTURAL_CONTEXT = ?, AI_DANCEABILITY = ?, AI_ENERGY = ?, AI_ERA = ?, AI_INSTRUMENTALNESS = ?,
                    AI_KEY = ?, AI_LIVENESS = ?, AI_LOUDNESS = ?, AI_MODE = ?, AI_MOOD = ?, AI_OCCASION = ?,
                    AI_SPEECHINESS = ?, AI_SUBGENRES = ?, AI_TIME_SIGNATURE = ?, AI_VALENCE = ?
                WHERE file_id = ?
            `;
            
            const updateParams = [
                // Campos LLM
                llmData.LLM_DESCRIPTION || null,
                llmData.LLM_MOOD || null,
                llmData.LLM_GENRE || null,
                llmData.LLM_SUBGENRE || null,
                llmData.LLM_CONTEXT || null,
                1, // LLM_ANALYZED = true
                new Date().toISOString(), // LLM_ANALYSIS_DATE
                
                // ✅ TODOS los campos AI_* (reemplazo completo)
                llmData.AI_ACOUSTICNESS || null,
                llmData.AI_ANALYZED || 1,
                llmData.AI_BPM || null,
                llmData.AI_CHARACTERISTICS ? JSON.stringify(llmData.AI_CHARACTERISTICS) : null,
                llmData.AI_CONFIDENCE || null,
                llmData.AI_CULTURAL_CONTEXT || null,
                llmData.AI_DANCEABILITY || null,
                llmData.AI_ENERGY || null,
                llmData.AI_ERA || null,
                llmData.AI_INSTRUMENTALNESS || null,
                llmData.AI_KEY || null,
                llmData.AI_LIVENESS || null,
                llmData.AI_LOUDNESS || null,
                llmData.AI_MODE || null,
                llmData.AI_MOOD || null,
                llmData.AI_OCCASION ? JSON.stringify(llmData.AI_OCCASION) : null,
                llmData.AI_SPEECHINESS || null,
                llmData.AI_SUBGENRES ? JSON.stringify(llmData.AI_SUBGENRES) : null,
                llmData.AI_TIME_SIGNATURE || null,
                llmData.AI_VALENCE || null,
                
                fileId // WHERE clause
            ];
            
            const result = await this.runQuery(updateSql, updateParams);
            
            // 2. Si no se actualizó ningún registro, hacer INSERT básico
            if (result.changes === 0) {
                const insertSql = `
                    INSERT INTO llm_metadata (file_id, LLM_DESCRIPTION, LLM_ANALYZED, LLM_ANALYSIS_DATE)
                    VALUES (?, ?, ?, ?)
                `;
                await this.runQuery(insertSql, [
                    fileId,
                    llmData.LLM_DESCRIPTION || 'LLM Analysis completed',
                    1,
                    new Date().toISOString()
                ]);
                
                // Luego hacer el UPDATE completo
                await this.runQuery(updateSql, updateParams);
            }
            
            // ✅ OBLIGATORIO: ACTUALIZAR audio_files.genre CON LLM_GENRE (análisis profesional)
            if (llmData.LLM_GENRE) {
                await this.runQuery(
                    'UPDATE audio_files SET genre = ? WHERE id = ?',
                    [llmData.LLM_GENRE, fileId]  
                );
                console.log(`🎵 Género actualizado en BD: ${llmData.LLM_GENRE} (archivo ID: ${fileId})`);
            }
            
            // Actualizar status del archivo
            await this.runQuery(
                'UPDATE audio_files SET analysis_status = ?, last_analyzed = CURRENT_TIMESTAMP WHERE id = ?',
                ['completed', fileId]
            );
            
        } catch (error) {
            console.error('Error insertando metadatos LLM:', error);
            throw error;
        }
    }

    /**
     * 📚 Obtener archivos de una carpeta y sus subcarpetas con paginación
     */
    async getFilesFromFolder(folderPath, limit = 1000, offset = 0) {
        const sql = `
            SELECT 
                af.*,
                -- AI_* campos principales
                lm.AI_ACOUSTICNESS, lm.AI_ANALYZED, lm.AI_BPM, lm.AI_CHARACTERISTICS, lm.AI_CONFIDENCE,
                lm.AI_CULTURAL_CONTEXT, lm.AI_DANCEABILITY, lm.AI_ENERGY, lm.AI_ERA, lm.AI_INSTRUMENTALNESS,
                lm.AI_KEY, lm.AI_LIVENESS, lm.AI_LOUDNESS, lm.AI_MODE, lm.AI_MOOD, lm.AI_OCCASION,
                lm.AI_SPEECHINESS, lm.AI_SUBGENRES, lm.AI_TIME_SIGNATURE, lm.AI_VALENCE,
                -- Campos legacy para compatibilidad
                lm.bpm_llm, lm.energy, lm.mood, lm.danceability, lm.valence,
                lm.analyzed_by, lm.subgenre, lm.era,
                lm.vocal_presence, lm.structure, lm.drop_time, lm.energy_curve,
                lm.crowd_response, lm.occasion, lm.characteristics,
                lm.tempo_stability, lm.production_quality, lm.mastering_loudness,
                lm.dynamic_range, lm.hamms_vector, lm.custom_tags,
                lm.analysis_date, lm.llm_version
            FROM audio_files af
            LEFT JOIN llm_metadata lm ON af.id = lm.file_id
            WHERE af.file_path LIKE ? || '%'
            ORDER BY af.file_name
            LIMIT ? OFFSET ?
        `;

        try {
            const rows = await this.allQuery(sql, [folderPath, limit, offset]);
            
            // Procesar datos JSON
            return rows.map(row => ({
                ...row,
                mixed_in_key_detected: Boolean(row.mixed_in_key_detected),
                should_preserve: Boolean(row.should_preserve),
                hamms_vector: row.hamms_vector ? JSON.parse(row.hamms_vector) : null,
                custom_tags: row.custom_tags ? JSON.parse(row.custom_tags) : [],
                // Procesar campos AI_* que son arrays JSON
                AI_CHARACTERISTICS: row.AI_CHARACTERISTICS ? JSON.parse(row.AI_CHARACTERISTICS) : null,
                AI_OCCASION: row.AI_OCCASION ? JSON.parse(row.AI_OCCASION) : null,
                AI_SUBGENRES: row.AI_SUBGENRES ? JSON.parse(row.AI_SUBGENRES) : null
            }));
        } catch (error) {
            console.error('Error obteniendo archivos:', error);
            throw error;
        }
    }

    /**
     * 🗂️ Obtener todos los archivos de la base de datos (para carga inicial)
     */
    async getAllFiles(limit = 10000, offset = 0) {
        const sql = `
            SELECT 
                af.*,
                -- AI_* campos principales
                lm.AI_ACOUSTICNESS, lm.AI_ANALYZED, lm.AI_BPM, lm.AI_CHARACTERISTICS, lm.AI_CONFIDENCE,
                lm.AI_CULTURAL_CONTEXT, lm.AI_DANCEABILITY, lm.AI_ENERGY, lm.AI_ERA, lm.AI_INSTRUMENTALNESS,
                lm.AI_KEY, lm.AI_LIVENESS, lm.AI_LOUDNESS, lm.AI_MODE, lm.AI_MOOD, lm.AI_OCCASION,
                lm.AI_SPEECHINESS, lm.AI_SUBGENRES, lm.AI_TIME_SIGNATURE, lm.AI_VALENCE,
                -- LLM campos (análisis con Claude)
                lm.LLM_DESCRIPTION, lm.LLM_MOOD, lm.LLM_GENRE, lm.LLM_SUBGENRE, lm.LLM_CONTEXT,
                lm.LLM_OCCASIONS, lm.LLM_ENERGY_LEVEL, lm.LLM_DANCEABILITY, lm.LLM_RECOMMENDATIONS,
                lm.LLM_ANALYZED, lm.LLM_ANALYSIS_DATE,
                -- Campos legacy para compatibilidad
                lm.bpm_llm, lm.energy, lm.mood, lm.danceability, lm.valence,
                lm.analyzed_by, lm.subgenre, lm.era,
                lm.vocal_presence, lm.structure, lm.drop_time, lm.energy_curve,
                lm.crowd_response, lm.occasion, lm.characteristics,
                lm.tempo_stability, lm.production_quality, lm.mastering_loudness,
                lm.dynamic_range, lm.hamms_vector, lm.custom_tags,
                lm.analysis_date, lm.llm_version
            FROM audio_files af
            LEFT JOIN llm_metadata lm ON af.id = lm.file_id
            GROUP BY af.id
            ORDER BY af.file_name
            LIMIT ? OFFSET ?
        `;

        try {
            const rows = await this.allQuery(sql, [limit, offset]);
            
            // Procesar datos JSON
            return rows.map(row => ({
                ...row,
                mixed_in_key_detected: Boolean(row.mixed_in_key_detected),
                should_preserve: Boolean(row.should_preserve),
                hamms_vector: row.hamms_vector ? JSON.parse(row.hamms_vector) : null,
                custom_tags: row.custom_tags ? JSON.parse(row.custom_tags) : [],
                // Procesar campos AI_* que son arrays JSON
                AI_CHARACTERISTICS: row.AI_CHARACTERISTICS ? JSON.parse(row.AI_CHARACTERISTICS) : null,
                AI_OCCASION: row.AI_OCCASION ? JSON.parse(row.AI_OCCASION) : null,
                AI_SUBGENRES: row.AI_SUBGENRES ? JSON.parse(row.AI_SUBGENRES) : null,
                // Procesar campos LLM que son arrays JSON
                LLM_OCCASIONS: row.LLM_OCCASIONS ? JSON.parse(row.LLM_OCCASIONS) : null
            }));
        } catch (error) {
            console.error('Error obteniendo todos los archivos:', error);
            throw error;
        }
    }

    /**
     * 🔍 Buscar archivos por múltiples criterios
     */
    async searchFiles(searchParams, limit = 1000) {
        let whereClause = '1=1';
        let params = [];
        
        if (searchParams.artist) {
            whereClause += ' AND af.artist LIKE ?';
            params.push(`%${searchParams.artist}%`);
        }
        
        if (searchParams.genre) {
            whereClause += ' AND af.genre LIKE ?';
            params.push(`%${searchParams.genre}%`);
        }
        
        if (searchParams.title) {
            whereClause += ' AND af.title LIKE ?';
            params.push(`%${searchParams.title}%`);
        }
        
        if (searchParams.energy) {
            whereClause += ' AND lm.energy = ?';
            params.push(searchParams.energy);
        }
        
        if (searchParams.mixedInKeyOnly) {
            whereClause += ' AND af.mixed_in_key_detected = 1';
        }
        
        if (searchParams.analyzedOnly) {
            whereClause += ' AND af.analysis_status = "completed"';
        }

        const sql = `
            SELECT 
                af.*,
                -- AI_* campos principales
                lm.AI_ACOUSTICNESS, lm.AI_ANALYZED, lm.AI_BPM, lm.AI_CHARACTERISTICS, lm.AI_CONFIDENCE,
                lm.AI_CULTURAL_CONTEXT, lm.AI_DANCEABILITY, lm.AI_ENERGY, lm.AI_ERA, lm.AI_INSTRUMENTALNESS,
                lm.AI_KEY, lm.AI_LIVENESS, lm.AI_LOUDNESS, lm.AI_MODE, lm.AI_MOOD, lm.AI_OCCASION,
                lm.AI_SPEECHINESS, lm.AI_SUBGENRES, lm.AI_TIME_SIGNATURE, lm.AI_VALENCE,
                -- Campos legacy
                lm.bpm_llm, lm.energy, lm.mood, lm.danceability, lm.valence,
                lm.analyzed_by, lm.hamms_vector, lm.custom_tags
            FROM audio_files af
            LEFT JOIN llm_metadata lm ON af.id = lm.file_id
            WHERE ${whereClause}
            ORDER BY af.artist, af.title
            LIMIT ?
        `;

        params.push(limit);

        try {
            const rows = await this.allQuery(sql, params);
            return rows.map(row => ({
                ...row,
                mixed_in_key_detected: Boolean(row.mixed_in_key_detected),
                hamms_vector: row.hamms_vector ? JSON.parse(row.hamms_vector) : null,
                custom_tags: row.custom_tags ? JSON.parse(row.custom_tags) : [],
                // Procesar campos AI_* que son arrays JSON
                AI_CHARACTERISTICS: row.AI_CHARACTERISTICS ? JSON.parse(row.AI_CHARACTERISTICS) : null,
                AI_OCCASION: row.AI_OCCASION ? JSON.parse(row.AI_OCCASION) : null,
                AI_SUBGENRES: row.AI_SUBGENRES ? JSON.parse(row.AI_SUBGENRES) : null
            }));
        } catch (error) {
            console.error('Error en búsqueda:', error);
            throw error;
        }
    }

    /**
     * 📊 Obtener estadísticas de la biblioteca
     */
    async getLibraryStats() {
        const queries = [
            'SELECT COUNT(*) as total_files FROM audio_files',
            'SELECT COUNT(*) as analyzed_files FROM audio_files WHERE analysis_status = "completed"',
            'SELECT COUNT(*) as mixed_in_key_files FROM audio_files WHERE mixed_in_key_detected = 1',
            'SELECT SUM(file_size) as total_size_bytes FROM audio_files',
            'SELECT COUNT(DISTINCT folder_path) as total_folders FROM audio_files'
        ];

        try {
            const results = await Promise.all(
                queries.map(query => this.getQuery(query))
            );

            return {
                totalFiles: results[0].total_files || 0,
                analyzedFiles: results[1].analyzed_files || 0,
                mixedInKeyFiles: results[2].mixed_in_key_files || 0,
                totalSizeGB: ((results[3].total_size_bytes || 0) / (1024 * 1024 * 1024)).toFixed(2),
                totalFolders: results[4].total_folders || 0,
                analysisProgress: results[0].total_files > 0 ? 
                    ((results[1].analyzed_files / results[0].total_files) * 100).toFixed(1) : 0
            };
        } catch (error) {
            console.error('Error obteniendo estadísticas:', error);
            throw error;
        }
    }

    /**
     * 🧹 Limpiar archivos que ya no existen
     */
    async cleanupMissingFiles() {
        const sql = 'SELECT id, file_path FROM audio_files';
        
        try {
            const files = await this.allQuery(sql);
            let removedCount = 0;
            
            for (const file of files) {
                if (!fs.existsSync(file.file_path)) {
                    await this.runQuery('DELETE FROM audio_files WHERE id = ?', [file.id]);
                    removedCount++;
                }
            }
            
            console.log(`🧹 Limpieza completada: ${removedCount} archivos faltantes eliminados`);
            return removedCount;
        } catch (error) {
            console.error('Error en limpieza:', error);
            throw error;
        }
    }

    /**
     * 🔒 Cerrar conexión a base de datos
     */
    close() {
        return new Promise((resolve) => {
            if (this.db && this.isInitialized) {
                this.db.close((err) => {
                    if (err) {
                        console.error('Error cerrando base de datos:', err);
                    } else {
                        console.log('✅ Base de datos cerrada');
                    }
                    this.db = null;
                    this.isInitialized = false;
                    resolve();
                });
            } else {
                resolve();
            }
        });
    }

    /**
     * 🔧 Limpiar datos AI_* duplicados y contradictorios
     */
    async cleanDuplicateAIFields() {
        console.log('🔧 Iniciando limpieza de datos AI_* duplicados...');
        
        try {
            // Campos AI que pueden tener datos duplicados separados por ";"
            const aiFields = [
                'AI_ACOUSTICNESS', 'AI_BPM', 'AI_CHARACTERISTICS', 'AI_CONFIDENCE',
                'AI_CULTURAL_CONTEXT', 'AI_DANCEABILITY', 'AI_ENERGY', 'AI_ERA',
                'AI_INSTRUMENTALNESS', 'AI_KEY', 'AI_LIVENESS', 'AI_LOUDNESS',
                'AI_MODE', 'AI_MOOD', 'AI_OCCASION', 'AI_SPEECHINESS',
                'AI_SUBGENRES', 'AI_TIME_SIGNATURE', 'AI_VALENCE'
            ];

            // Obtener todos los metadatos LLM
            const records = await this.allQuery('SELECT * FROM llm_metadata');
            let cleanedCount = 0;

            for (const record of records) {
                let needsUpdate = false;
                const updates = {};

                // Procesar cada campo AI
                for (const field of aiFields) {
                    const value = record[field];
                    
                    if (value && typeof value === 'string' && value.includes(';')) {
                        // Tiene valores duplicados separados por ";"
                        const values = value.split(';').map(v => v.trim()).filter(v => v);
                        
                        if (values.length > 1) {
                            // Lógica de consolidación según el tipo de campo
                            const cleanedValue = this.consolidateAIValue(field, values);
                            if (cleanedValue !== value) {
                                updates[field] = cleanedValue;
                                needsUpdate = true;
                                console.log(`🔧 Limpiando ${field}: "${value}" → "${cleanedValue}"`);
                            }
                        }
                    }
                }

                // Aplicar actualizaciones si es necesario
                if (needsUpdate) {
                    const updateFields = Object.keys(updates).map(field => `${field} = ?`).join(', ');
                    const updateValues = Object.values(updates);
                    updateValues.push(record.file_id);

                    await this.runQuery(
                        `UPDATE llm_metadata SET ${updateFields} WHERE file_id = ?`,
                        updateValues
                    );
                    cleanedCount++;
                }
            }

            console.log(`✅ Limpieza completada: ${cleanedCount} registros actualizados`);
            return { cleanedRecords: cleanedCount };

        } catch (error) {
            console.error('❌ Error en limpieza de datos duplicados:', error);
            throw error;
        }
    }

    /**
     * 🎯 Consolidar valores AI duplicados usando lógica inteligente
     */
    consolidateAIValue(field, values) {
        // Filtrar valores vacíos o null
        const validValues = values.filter(v => v && v !== 'null' && v !== 'undefined');
        
        if (validValues.length === 0) return null;
        if (validValues.length === 1) return validValues[0];

        switch (field) {
            case 'AI_ENERGY':
            case 'AI_DANCEABILITY':
            case 'AI_VALENCE':
            case 'AI_CONFIDENCE':
            case 'AI_ACOUSTICNESS':
            case 'AI_INSTRUMENTALNESS':
            case 'AI_SPEECHINESS':
            case 'AI_LIVENESS':
                // Para valores numéricos: usar el más alto (más confiable)
                const numValues = validValues.map(v => parseFloat(v)).filter(v => !isNaN(v));
                return numValues.length > 0 ? Math.max(...numValues).toString() : validValues[0];

            case 'AI_BPM':
                // Para BPM: usar el valor más probable (evitar extremos)
                const bpmValues = validValues.map(v => parseFloat(v)).filter(v => !isNaN(v) && v >= 60 && v <= 200);
                if (bpmValues.length === 0) return validValues[0];
                // Usar mediana para evitar outliers
                bpmValues.sort((a, b) => a - b);
                const median = bpmValues[Math.floor(bpmValues.length / 2)];
                return median.toString();

            case 'AI_CULTURAL_CONTEXT':
                // Para contexto cultural: usar el más específico (no genérico)
                const specificContexts = validValues.filter(v => 
                    !v.toLowerCase().includes('world music') && 
                    !v.toLowerCase().includes('alternative rock') &&
                    v.length > 10 // Más específico
                );
                return specificContexts.length > 0 ? specificContexts[0] : validValues[0];

            case 'AI_MOOD':
                // Para mood: preferir energéticos sobre agresivos para música dance
                if (validValues.includes('Energetic') && validValues.includes('Aggressive')) {
                    return 'Energetic';
                }
                return validValues[0]; // Usar el primero si no hay conflicto claro

            case 'AI_ERA':
                // Para era: usar el más específico (con década)
                const decadeValues = validValues.filter(v => v.includes('s') && v.length <= 8);
                return decadeValues.length > 0 ? decadeValues[0] : validValues[0];

            case 'AI_CHARACTERISTICS':
            case 'AI_SUBGENRES':
            case 'AI_OCCASION':
                // Para arrays: consolidar todos los valores únicos
                try {
                    const allItems = [];
                    for (const value of validValues) {
                        const parsed = JSON.parse(value);
                        if (Array.isArray(parsed)) {
                            allItems.push(...parsed);
                        } else {
                            allItems.push(value);
                        }
                    }
                    const uniqueItems = [...new Set(allItems)];
                    return JSON.stringify(uniqueItems);
                } catch {
                    return validValues[0];
                }

            default:
                // Para otros campos: usar el primer valor válido
                return validValues[0];
        }
    }

    /**
     * 🔍 Validar consistencia de datos AI
     */
    async validateAIConsistency() {
        console.log('🔍 Validando consistencia de datos AI...');
        
        try {
            const inconsistencies = [];
            const records = await this.allQuery(
                'SELECT file_id, AI_ENERGY, AI_DANCEABILITY, AI_VALENCE, AI_MOOD, AI_CULTURAL_CONTEXT FROM llm_metadata WHERE AI_ANALYZED = 1'
            );

            for (const record of records) {
                const issues = [];
                
                // Validar coherencia energy vs danceability
                if (record.AI_ENERGY && record.AI_DANCEABILITY) {
                    const energy = parseFloat(record.AI_ENERGY);
                    const danceability = parseFloat(record.AI_DANCEABILITY);
                    
                    if (energy > 0.8 && danceability < 0.4) {
                        issues.push('Alta energía con baja bailabilidad');
                    }
                    if (energy < 0.3 && danceability > 0.8) {
                        issues.push('Baja energía con alta bailabilidad');
                    }
                }

                // Validar mood vs valence
                if (record.AI_MOOD && record.AI_VALENCE) {
                    const valence = parseFloat(record.AI_VALENCE);
                    const mood = record.AI_MOOD.toLowerCase();
                    
                    if ((mood.includes('happy') || mood.includes('energetic')) && valence < 0.3) {
                        issues.push('Mood positivo con valencia baja');
                    }
                    if ((mood.includes('sad') || mood.includes('melancholic')) && valence > 0.7) {
                        issues.push('Mood negativo con valencia alta');
                    }
                }

                // Validar contexto cultural específico
                if (record.AI_CULTURAL_CONTEXT) {
                    const context = record.AI_CULTURAL_CONTEXT.toLowerCase();
                    if (context.includes('world music') || context === 'alternative rock') {
                        issues.push('Contexto cultural demasiado genérico');
                    }
                }

                if (issues.length > 0) {
                    inconsistencies.push({
                        file_id: record.file_id,
                        issues: issues
                    });
                }
            }

            console.log(`🔍 Validación completada: ${inconsistencies.length} inconsistencias encontradas`);
            return { inconsistencies, totalRecords: records.length };

        } catch (error) {
            console.error('❌ Error validando consistencia:', error);
            throw error;
        }
    }
}

module.exports = MusicDatabase;