const { app, BrowserWindow, dialog, ipcMain } = require('electron');
const path = require('path');
const fs = require('fs');
const crypto = require('crypto');
const MetadataWriter = require('./metadata-writer');
const MusicDatabase = require('./database');
const CacheService = require('./cache-service');

// Load environment variables
require('dotenv').config();

// Calculate SHA-256 hash of file
async function calculateFileHash(filePath) {
    return new Promise((resolve, reject) => {
        const hash = crypto.createHash('sha256');
        const stream = fs.createReadStream(filePath);
        
        stream.on('data', data => hash.update(data));
        stream.on('end', () => resolve(hash.digest('hex')));
        stream.on('error', reject);
    });
}

// Initialize all systems
let metadataAddon = null;
let metadataWriter = null;
let database = null;
let cache = null;

// Initialize core systems
async function initializeSystems() {
    try {
        // Initialize database
        database = new MusicDatabase();
        await database.initialize();
        console.log('✅ Base de datos inicializada');
        
        // Initialize cache service
        cache = new CacheService();
        console.log('✅ Sistema de cache inicializado');
        
        // Initialize metadata writer with C++ addon
        metadataWriter = new MetadataWriter(metadataAddon);
        console.log('✅ JavaScript metadata writer initialized');
        
    } catch (error) {
        console.error('❌ Error inicializando sistemas:', error);
        throw error;
    }
}

// Try to load the metadata addon (optional)
try {
    metadataAddon = require('./build/Release/metadata_addon');
    console.log('✅ High-performance C++ engine loaded');
} catch (error) {
    // Fallback to JavaScript if C++ addon not available
    console.log('🔧 C++ engine not available - using JavaScript fallback');
    console.log('Error loading C++ addon:', error.message);
    metadataAddon = null;
}

let mainWindow;

async function createWindow() {
    // Initialize systems before creating window
    await initializeSystems();
    
    mainWindow = new BrowserWindow({
        width: 1200,
        height: 800,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false
        },
        title: '🎵 Music Analyzer Pro - Database Ready',
        icon: path.join(__dirname, 'icon.png') // opcional
    });

    mainWindow.loadFile('index.html');
    
    // Abrir DevTools en desarrollo
    if (process.argv.includes('--dev')) {
        mainWindow.webContents.openDevTools();
    }
}

// 📁 IPC Handler: Seleccionar carpeta
ipcMain.handle('select-folder', async () => {
    const result = await dialog.showOpenDialog(mainWindow, {
        properties: ['openDirectory'],
        title: 'Seleccionar carpeta con archivos de audio'
    });
    
    if (result.canceled) {
        return null;
    }
    
    return result.filePaths[0];
});

// 📂 IPC Handler: Cargar archivos de audio con sistema de persistencia
ipcMain.handle('load-audio-files', async (event, folderPath) => {
    try {
        console.log(`🚀 Iniciando carga optimizada para: ${folderPath}`);
        
        // Registrar carpeta en base de datos
        await database.registerFolder(folderPath);
        
        // Intentar usar cache inteligente primero
        let audioFiles = await cache.smartCache(database, folderPath);
        
        if (audioFiles && audioFiles.length > 0) {
            console.log(`⚡ Archivos cargados desde cache/DB: ${audioFiles.length}`);
            return audioFiles.map(file => formatFileForUI(file));
        }
        
        // Si no hay cache, escanear carpeta y poblar base de datos
        console.log('📁 Escaneando carpeta y poblando base de datos...');
        audioFiles = await scanAndPopulateFolder(folderPath);
        
        console.log(`✅ Carga completada: ${audioFiles.length} archivos`);
        return audioFiles.map(file => formatFileForUI(file));
        
    } catch (error) {
        console.error('❌ Error loading audio files:', error);
        return [];
    }
});

/**
 * 🔍 Escanear directorio recursivamente para encontrar todos los archivos de audio
 */
async function scanDirectoryRecursively(dirPath, audioExtensions) {
    const audioFiles = [];
    
    const scanDirectory = async (currentPath) => {
        try {
            const items = fs.readdirSync(currentPath, { withFileTypes: true });
            
            for (const item of items) {
                const itemPath = path.join(currentPath, item.name);
                
                if (item.isDirectory()) {
                    // Ignorar carpetas del sistema y ocultas
                    if (!item.name.startsWith('.') && !item.name.startsWith('$')) {
                        await scanDirectory(itemPath);
                    }
                } else if (item.isFile()) {
                    const ext = path.extname(item.name).toLowerCase();
                    if (audioExtensions.includes(ext)) {
                        audioFiles.push(itemPath);
                    }
                }
            }
        } catch (error) {
            // Directory access restricted, skipping
        }
    };
    
    await scanDirectory(dirPath);
    return audioFiles;
}

/**
 * 📁 Escanear carpeta recursivamente y poblar base de datos (para carpetas nuevas)
 */
async function scanAndPopulateFolder(folderPath) {
    const audioExtensions = ['.mp3', '.flac', '.wav', '.m4a', '.ogg', '.aac'];
    
    // Escanear recursivamente para encontrar todos los archivos de audio
    const allAudioFiles = await scanDirectoryRecursively(folderPath, audioExtensions);
    const audioFiles = [];
    
    let processed = 0;
    const total = allAudioFiles.length;
    
    console.log(`📊 Archivos de audio encontrados (recursivo): ${total}`);
    
    for (const filePath of allAudioFiles) {
        const file = path.basename(filePath);
        const ext = path.extname(file).toLowerCase();
        
        try {
            // SIMPLIFICADO: Solo obtener información básica del archivo
            const stats = fs.statSync(filePath);
            
            // Leer metadatos reales del archivo
            let metadata = {};
            try {
                const mm = await metadataWriter.musicMetadata.parseFile(filePath);
                metadata = {
                    title: mm.common.title || file.replace(ext, ''),
                    artist: mm.common.artist || mm.common.albumartist || 'Unknown',
                    album: mm.common.album || 'Unknown',
                    genre: mm.common.genre ? mm.common.genre.join(', ') : 'Unknown',
                    year: mm.common.year || null,
                    duration: mm.format.duration || 0,
                    bitrate: mm.format.bitrate || 0,
                    sampleRate: mm.format.sampleRate || 0,
                    channels: mm.format.numberOfChannels || 0
                };
                
                // Detectar Mixed In Key
                const mixedInKeyData = metadataWriter.detectMixedInKey(mm);
                metadata.mixedInKeyDetected = mixedInKeyData.detected;
                metadata.existingBpm = mixedInKeyData.bpm;
                metadata.existingKey = mixedInKeyData.key;
                metadata.preservationSource = mixedInKeyData.source;
                metadata.shouldPreserve = mixedInKeyData.detected;
            } catch (metadataError) {
                console.warn(`⚠️ No se pudieron leer metadatos de ${file}:`, metadataError.message);
                metadata = {
                    title: file.replace(ext, ''),
                    artist: 'Unknown',
                    album: 'Unknown',
                    genre: 'Unknown',
                    year: null
                };
            }
            
            // Calcular hash real del archivo
            const fileHash = await calculateFileHash(filePath);
            
            // Preparar datos para base de datos con metadatos reales
            const fileData = {
                filePath: filePath,
                fileName: file,
                fileSize: stats.size,
                fileExtension: ext,
                dateModified: stats.mtime.toISOString(),
                folderPath: path.dirname(filePath),
                title: metadata.title,
                artist: metadata.artist,
                album: metadata.album,
                genre: metadata.genre,
                year: metadata.year,
                duration: metadata.duration,
                bitrate: metadata.bitrate,
                sampleRate: metadata.sampleRate,
                channels: metadata.channels,
                mixedInKeyDetected: metadata.mixedInKeyDetected || false,
                existingBpm: metadata.existingBpm || null,
                existingKey: metadata.existingKey || null,
                preservationSource: metadata.preservationSource || 'none',
                shouldPreserve: metadata.shouldPreserve || false,
                fileHash: fileHash
            };
            
            // Insertar en base de datos
            const fileId = await database.upsertAudioFile(fileData);
            
            // Datos simplificados para UI (con campos normalizados para cache)
            const completeFileData = {
                id: fileId,
                ...fileData,
                file_path: fileData.filePath, // Para compatibilidad con cache
                file_name: fileData.fileName, // Para compatibilidad con cache
                analysis_status: 'pending'
            };
            
            audioFiles.push(completeFileData);
            processed++;
            
            // Log de progreso cada 100 archivos
            if (processed % 100 === 0) {
                console.log(`📦 Procesados: ${processed}/${total} archivos`);
            }
            
        } catch (error) {
            console.error(`⚠️ Error procesando ${file}:`, error.message);
        }
    }
    
    // Cachear los resultados
    cache.setFolder(folderPath, audioFiles);
    
    console.log(`✅ Escaneo completado: ${audioFiles.length} archivos procesados`);
    return audioFiles;
}

/**
 * 🔑 Generar hash de archivo para detectar cambios
 */
async function generateFileHash(filePath) {
    return new Promise((resolve, reject) => {
        const hash = crypto.createHash('md5');
        const stream = fs.createReadStream(filePath);
        
        stream.on('data', data => hash.update(data));
        stream.on('end', () => resolve(hash.digest('hex')));
        stream.on('error', reject);
    });
}

/**
 * 🛡️ JSON parsing seguro que maneja strings vacíos o mal formados
 */
function safeJsonParse(data) {
    try {
        // Si ya es un array, devolverlo directamente
        if (Array.isArray(data)) {
            return data;
        }
        
        // Si es un string, intentar parsearlo
        if (typeof data === 'string') {
            if (!data || data.trim() === '') {
                return null;
            }
            return JSON.parse(data);
        }
        
        // Para otros tipos, devolver null
        return null;
    } catch (error) {
        console.warn('Error parsing JSON:', data, error.message);
        return null;
    }
}

/**
 * 🎨 Formatear archivo para la UI (SIMPLIFICADO)
 */
function formatFileForUI(dbFile) {
    // Note: Files need to be analyzed to populate genre/subgenre data
    // Most files will show "-" until analyzed with AI algorithms

    // Calculate progress based on AI_* fields (19 total algorithms)
    const aiFields = [
        'AI_ACOUSTICNESS', 'AI_ANALYZED', 'AI_BPM', 'AI_CHARACTERISTICS', 'AI_CONFIDENCE',
        'AI_CULTURAL_CONTEXT', 'AI_DANCEABILITY', 'AI_ENERGY', 'AI_ERA', 'AI_INSTRUMENTALNESS',
        'AI_KEY', 'AI_LIVENESS', 'AI_LOUDNESS', 'AI_MODE', 'AI_MOOD', 'AI_OCCASION',
        'AI_SPEECHINESS', 'AI_SUBGENRES', 'AI_TIME_SIGNATURE', 'AI_VALENCE'
    ];
    
    let completed = 0;
    aiFields.forEach(field => {
        if (dbFile[field] !== null && dbFile[field] !== undefined && dbFile[field] !== '') {
            completed++;
        }
    });
    
    const progress = `${completed}/19`;
    
    return {
        id: dbFile.id,
        name: dbFile.file_name || dbFile.fileName || 'Unknown',
        path: dbFile.file_path || dbFile.filePath || '',
        size: dbFile.file_size ? Math.round(dbFile.file_size / 1024 / 1024 * 100) / 100 : 0,
        extension: dbFile.file_extension || dbFile.fileExtension || '',
        dateModified: dbFile.date_modified ? dbFile.date_modified.split('T')[0] : (dbFile.dateModified || ''),
        progress: progress, // X/19 format for UI
        
        // Metadatos estándar SIMPLIFICADOS
        standard_metadata: {
            title: dbFile.title || 'Unknown',
            artist: dbFile.artist || 'Unknown',
            album: dbFile.album || 'Unknown',
            genre: dbFile.genre || 'Unknown',
            year: dbFile.year || null
        },
        
        // Metadatos LLM SIMPLIFICADOS  
        llm_metadata: {
            bpm_llm: dbFile.AI_BPM || dbFile.bmp_llm || null,
            energy: dbFile.AI_ENERGY || dbFile.energy || null,
            mood: dbFile.AI_MOOD || dbFile.mood || null,
            AI_CULTURAL_CONTEXT: dbFile.AI_CULTURAL_CONTEXT || null,
            AI_SUBGENRES: Array.isArray(dbFile.AI_SUBGENRES) ? dbFile.AI_SUBGENRES : 
                         (dbFile.AI_SUBGENRES ? safeJsonParse(dbFile.AI_SUBGENRES) || [] : [])
        },
        
        // Metadatos Claude LLM
        claude_llm: {
            description: dbFile.LLM_DESCRIPTION || null,
            mood: dbFile.LLM_MOOD || null,
            genre: dbFile.LLM_GENRE || null,
            subgenre: dbFile.LLM_SUBGENRE || null,
            context: dbFile.LLM_CONTEXT || null,
            occasions: Array.isArray(dbFile.LLM_OCCASIONS) ? dbFile.LLM_OCCASIONS : 
                      (dbFile.LLM_OCCASIONS ? safeJsonParse(dbFile.LLM_OCCASIONS) || [] : []),
            energy_level: dbFile.LLM_ENERGY_LEVEL || null,
            danceability: dbFile.LLM_DANCEABILITY || null,
            recommendations: dbFile.LLM_RECOMMENDATIONS || null,
            analyzed: !!dbFile.LLM_ANALYZED,
            analysis_date: dbFile.LLM_ANALYSIS_DATE || null
        },
        
        // Campos alternativos disponibles
        subgenre: dbFile.subgenre || null,
        era: dbFile.era || null,
        analyzed_by: dbFile.analyzed_by || null,
        
        // Mixed In Key detection
        mixed_in_key_detected: !!(dbFile.bpm_mixed_in_key || dbFile.key_mixed_in_key),
        
        // Status based on progress
        status: completed === 19 ? 'completed' : completed > 0 ? 'partial' : 'pending'
    };
}

// 🎵 IPC Handler: Escribir metadata LLM a archivo
ipcMain.handle('write-llm-metadata', async (event, filePath, metadata) => {
    if (!metadataAddon) {
        throw new Error('Metadata addon not available. Use "npm run install-addon" to build C++ support.');
    }
    
    try {
        const result = await new Promise((resolve, reject) => {
            metadataAddon.writeMetadata(filePath, metadata, (err, success) => {
                if (err) reject(new Error(err));
                else resolve(success);
            });
        });
        
        return result;
    } catch (error) {
        console.error('Error writing metadata:', error);
        throw error;
    }
});

// 📖 IPC Handler: Leer metadatos existentes (Mixed In Key + estándar) - MEJORADO
ipcMain.handle('read-existing-metadata', async (event, filePath) => {
    try {
        const result = await metadataWriter.readExistingMetadata(filePath);
        return {
            exists: result.exists,
            fileName: result.fileName,
            hasMixedInKeyPattern: result.hasMixedInKeyPattern || false,
            existingBPM: result.preservation?.existingBPM || null,
            existingKey: result.preservation?.existingKey || null,
            source: result.preservation?.source || 'none',
            shouldPreserve: result.preservation?.shouldPreserve || false,
            metadata: result.metadata || {},
            llmMetadata: result.llmMetadata || {}
        };
    } catch (error) {
        console.error('Error reading existing metadata:', error);
        return { exists: false, error: error.message };
    }
});

// 🔍 IPC Handler: Verificar si se puede escribir metadata personalizada
ipcMain.handle('can-write-custom-fields', async (event, filePath) => {
    if (!metadataAddon) {
        return false;
    }
    
    try {
        return metadataAddon.canWriteCustomFields(filePath);
    } catch (error) {
        console.error('Error checking custom fields support:', error);
        return false;
    }
});

// 💾 IPC Handler: Escribir metadata con preservación de Mixed In Key y actualización de DB
ipcMain.handle('write-llm-metadata-safe', async (event, filePath, metadata, preserveMixedInKey = true) => {
    try {
        console.log('📝 Escribiendo metadatos LLM de forma segura:', path.basename(filePath));
        
        // Escribir al archivo de audio usando MetadataWriter
        const result = await metadataWriter.writeLLMMetadataSafe(filePath, metadata, preserveMixedInKey);
        
        if (result.success) {
            // Actualizar base de datos con los nuevos metadatos LLM
            try {
                // Buscar el archivo en la base de datos
                const dbFile = await database.getQuery(
                    'SELECT id FROM audio_files WHERE file_path = ?',
                    [filePath]
                );
                
                if (dbFile) {
                    // Actualizar metadatos LLM en la base de datos
                    await database.insertLLMMetadata(dbFile.id, metadata);
                    
                    // Invalidar cache para este archivo
                    cache.invalidateFile(filePath);
                    
                    console.log('✅ Base de datos actualizada con metadatos LLM');
                } else {
                    // File not found in database, skipping update
                }
            } catch (dbError) {
                console.error('⚠️ Error actualizando base de datos:', dbError);
                // No fallar si solo el DB update falla, el archivo se escribió correctamente
            }
        }
        
        return result;
    } catch (error) {
        console.error('Error writing safe metadata:', error);
        throw error;
    }
});

// 📊 IPC Handler: Obtener estadísticas de biblioteca desde base de datos
ipcMain.handle('get-library-stats', async () => {
    try {
        const stats = await database.getLibraryStats();
        const cacheStats = cache.getStats();
        
        return {
            ...stats,
            cache: cacheStats
        };
    } catch (error) {
        console.error('Error getting library stats:', error);
        return {
            totalFiles: 0,
            analyzedFiles: 0,
            mixedInKeyFiles: 0,
            totalSizeGB: 0,
            totalFolders: 0,
            analysisProgress: 0,
            error: error.message
        };
    }
});

// 🔍 IPC Handler: Buscar archivos con múltiples criterios
ipcMain.handle('search-files', async (event, searchParams) => {
    try {
        console.log('🔍 Búsqueda:', searchParams);
        
        // Intentar obtener resultados del cache primero
        let results = cache.getSearchResults(searchParams);
        
        if (!results) {
            // Buscar en base de datos
            results = await database.searchFiles(searchParams, searchParams.limit || 1000);
            
            // Cachear los resultados
            cache.setSearchResults(searchParams, results);
        }
        
        // Formatear para UI
        return results.map(file => formatFileForUI(file));
        
    } catch (error) {
        console.error('Error in search:', error);
        return [];
    }
});

// 🧹 IPC Handler: Limpiar archivos faltantes
ipcMain.handle('cleanup-missing-files', async () => {
    try {
        const removedCount = await database.cleanupMissingFiles();
        
        // Limpiar cache también
        cache.clear();
        
        return {
            success: true,
            removedFiles: removedCount,
            message: `${removedCount} archivos faltantes eliminados`
        };
    } catch (error) {
        console.error('Error cleaning up files:', error);
        return {
            success: false,
            error: error.message
        };
    }
});

// 📈 IPC Handler: Optimizar sistema (cache + base de datos)
ipcMain.handle('optimize-system', async () => {
    try {
        // Optimizar cache
        const cacheResult = cache.optimize();
        
        // Limpiar archivos faltantes
        const cleanupResult = await database.cleanupMissingFiles();
        
        // Ejecutar VACUUM en SQLite para optimizar espacio
        await database.runQuery('VACUUM');
        
        return {
            success: true,
            cache: cacheResult,
            removedFiles: cleanupResult,
            message: 'Sistema optimizado correctamente'
        };
    } catch (error) {
        console.error('Error optimizing system:', error);
        return {
            success: false,
            error: error.message
        };
    }
});

// 📄 IPC Handler: Obtener archivos similares
ipcMain.handle('get-similar-files', async (event, filePath, limit = 10) => {
    try {
        const similar = cache.getSimilarFiles(filePath, limit);
        return similar.map(file => formatFileForUI(file));
    } catch (error) {
        console.error('Error getting similar files:', error);
        return [];
    }
});

// 📦 IPC Handler: Cargar más archivos con paginación
ipcMain.handle('load-more-files', async (event, folderPath, offset = 0, limit = 1000) => {
    try {
        const files = await database.getFilesFromFolder(folderPath, limit, offset);
        return files.map(file => formatFileForUI(file));
    } catch (error) {
        console.error('Error loading more files:', error);
        return [];
    }
});

// 🚀 IPC Handler: Obtener archivos existentes de la base de datos
ipcMain.handle('get-existing-files', async (event, limit = 10000) => {
    try {
        const files = await database.getAllFiles(limit);
        console.log(`📁 Archivos existentes encontrados: ${files.length}`);
        
        const formatted = files.map(file => formatFileForUI(file));
        
        return formatted;
    } catch (error) {
        console.error('❌ Error getting existing files:', error);
        return [];
    }
});

// 🔬 IPC Handler: Ejecutar análisis con algoritmos C++
ipcMain.handle('analyze-file-with-algorithms', async (event, filePath, algorithms) => {
    try {
        console.log(`🔬 Analizando ${path.basename(filePath)} con ${algorithms.length} algoritmos`);
        
        // Si el addon C++ está disponible, usarlo
        if (metadataAddon && metadataAddon.analyzeAudio) {
            try {
                const results = await new Promise((resolve, reject) => {
                    metadataAddon.analyzeAudio(filePath, algorithms, (err, data) => {
                        if (err) reject(new Error(err));
                        else resolve(data);
                    });
                });
                
                console.log(`✅ Análisis C++ completado para ${path.basename(filePath)}`);
                
                // CRITICAL: Save C++ results to database AND write to audio file
                try {
                    const dbFile = await database.getQuery('SELECT id FROM audio_files WHERE file_path = ?', [filePath]);
                    
                    if (dbFile) {
                        // 1. Save to database
                        await database.insertLLMMetadata(dbFile.id, results);
                        console.log(`💾 C++ results saved to database for ${path.basename(filePath)}`);
                        
                        // 2. Write metadata directly to audio file
                        try {
                            if (metadataWriter) {
                                const writeResult = await metadataWriter.writeLLMMetadataSafe(filePath, results, true);
                                if (writeResult.success) {
                                    console.log(`🎵 C++ metadata written to audio file: ${path.basename(filePath)}`);
                                } else {
                                    console.warn(`⚠️ Could not write C++ metadata to audio file: ${writeResult.error}`);
                                }
                            }
                        } catch (fileWriteError) {
                            console.error(`❌ Error writing C++ metadata to audio file:`, fileWriteError);
                        }
                        
                        // 3. Invalidate cache
                        cache.invalidateFile(filePath);
                    }
                } catch (saveError) {
                    console.error(`❌ Error saving C++ results:`, saveError);
                }
                
                return {
                    success: true,
                    engine: 'cpp',
                    results: results,
                    algorithmsProcessed: algorithms.length
                };
            } catch (cppError) {
                console.error(`❌ Error en análisis C++: ${cppError.message}`);
                throw new Error(`C++ analysis failed: ${cppError.message}`);
            }
        } else {
            throw new Error('C++ addon not available. Please rebuild with: npx node-gyp rebuild');
        }
        
        
    } catch (error) {
        console.error(`❌ Error analizando ${path.basename(filePath)}:`, error);
        return {
            success: false,
            error: error.message,
            algorithmsProcessed: 0
        };
    }
});

// 🧠 IPC Handler: Análisis LLM con Claude
ipcMain.handle('analyze-llm', async (event, filePath) => {
    try {
        console.log(`🧠 Iniciando análisis LLM para: ${path.basename(filePath)}`);
        
        // ✅ VERIFICAR SI YA ESTÁ ANALIZADO (evitar re-análisis)
        try {
            const existingData = await database.getQuery(
                `SELECT af.id, lm.AI_ANALYZED, lm.LLM_ANALYSIS_DATE 
                 FROM audio_files af 
                 LEFT JOIN llm_metadata lm ON af.id = lm.file_id 
                 WHERE af.file_path = ?`, 
                [filePath]
            );
            
            if (existingData && existingData.AI_ANALYZED === 'true') {
                console.log(`⏭️ Archivo ya analizado, saltando: ${path.basename(filePath)}`);
                return {
                    success: true,
                    message: 'Archivo ya analizado previamente',
                    skipReason: 'already_analyzed',
                    analysisDate: existingData.LLM_ANALYSIS_DATE
                };
            }
        } catch (checkError) {
            console.warn(`⚠️ No se pudo verificar estado de análisis: ${checkError.message}`);
            // Continuar con análisis si no se puede verificar
        }
        
        // Get existing metadata from file
        const fileMetadata = await metadataWriter.readExistingMetadata(filePath);
        
        // 🧠 PROMPT MEJORADO CON VALIDACIÓN CRUZADA Y COHERENCIA MUSICAL
        const prompt = `Como EXPERTO MUSICÓLOGO y DJ PROFESIONAL con conocimiento profundo de análisis musical, teoría armónica y psicoacústica, analiza este archivo de música aplicando VALIDACIÓN CRUZADA entre todos los parámetros.

📄 INFORMACIÓN DEL ARCHIVO:
- Archivo: ${path.basename(filePath)}
- Título: ${fileMetadata.metadata?.title || 'Desconocido'}
- Artista: ${fileMetadata.metadata?.artist || 'Desconocido'}
- Álbum: ${fileMetadata.metadata?.album || 'Desconocido'}
- Género actual: ${fileMetadata.metadata?.genre || 'Sin definir'} (IMPORTANTE: Analiza y corrige si es necesario)
- Año: ${fileMetadata.metadata?.year || 'Desconocido'}

🎯 REGLAS DE COHERENCIA MUSICAL OBLIGATORIAS:

1. **COHERENCIA ENERGÍA-BAILABILIDAD:**
   - Si AI_ENERGY > 0.8 → AI_DANCEABILITY debe ser > 0.6
   - Si AI_ENERGY < 0.3 → AI_DANCEABILITY debe ser < 0.5
   - Música electrónica dance: ambos valores altos (0.7+)

2. **COHERENCIA MOOD-VALENCE:**
   - "Energetic", "Happy", "Euphoric" → AI_VALENCE > 0.6
   - "Melancholic", "Sad", "Dark" → AI_VALENCE < 0.4
   - "Aggressive" puede tener valence medio (0.4-0.7)

3. **COHERENCIA CULTURAL-TEMPORAL:**
   - AI_CULTURAL_CONTEXT debe ser ESPECÍFICO: "90s Belgian Eurodance", "Detroit Techno", "UK Garage 2000s"
   - NO uses: "World Music", "Alternative Rock", "Electronic Music" (demasiado genérico)
   - AI_ERA debe coincidir con el contexto cultural

4. **COHERENCIA GÉNERO-CARACTERÍSTICAS:**
   - House/Techno → AI_CHARACTERISTICS incluye "4/4 Beat", "Repetitive"
   - Rock/Pop → AI_CHARACTERISTICS incluye "Melodic", "Structured"
   - Hip-Hop → AI_CHARACTERISTICS incluye "Rhythmic", "Vocal-driven"

6. **CORRECCIÓN DE GÉNERO OBLIGATORIA:**
   - ANALIZA el género actual y determina si es CORRECTO
   - Si el género actual es incorrecto (ej: "Rock" para una canción de Italo Disco), DEBES corregirlo
   - LLM_GENRE debe ser el género REAL de la canción, no el género original
   - Ejemplos de correcciones: "Rock" → "Italo Disco", "Pop" → "Eurodance", "Alternative" → "Synthwave"

5. **VALIDACIÓN CRUZADA OBLIGATORIA:**
   - Revisa CADA valor numérico contra los demás
   - Asegúrate de que AI_MOOD coincida con AI_VALENCE
   - Verifica que AI_SUBGENRES sean coherentes con AI_CULTURAL_CONTEXT
   - AI_CONFIDENCE alto (0.85+) solo si todos los valores son coherentes

🎵 ESTRUCTURA JSON REQUERIDA (SOLO JSON, SIN TEXTO ADICIONAL):

{
  "LLM_DESCRIPTION": "Análisis detallado considerando: estilo, instrumentación, estructura, producción y contexto histórico (máximo 250 palabras)",
  "LLM_MOOD": "Mood principal (validado contra valencia y energía)",
  "LLM_GENRE": "OBLIGATORIO: Género principal CORREGIDO basado en análisis musical profesional (reemplaza género original si es incorrecto)",
  "LLM_SUBGENRE": "Subgénero específico (coherente con contexto cultural)",
  "LLM_CONTEXT": "Contexto cultural ESPECÍFICO (ej: 90s Belgian Eurodance, Modern UK Drill, Classic Detroit Techno)",
  "LLM_OCCASIONS": ["Ocasión1", "Ocasión2"],
  "LLM_ENERGY_LEVEL": "Alto/Medio/Bajo (coherente con AI_ENERGY)",
  "LLM_DANCEABILITY": "Alta/Media/Baja (coherente con AI_DANCEABILITY)",
  "LLM_RECOMMENDATIONS": "Recomendaciones DJ específicas basadas en características musicales",
  
  "AI_ENERGY": [FLOAT 0.0-1.0, coherente con género y mood],
  "AI_DANCEABILITY": [FLOAT 0.0-1.0, validado contra energy],
  "AI_VALENCE": [FLOAT 0.0-1.0, coherente con mood],
  "AI_MOOD": "[Mood coherente con valence: Happy(0.7+), Energetic(0.6+), Neutral(0.4-0.6), Melancholic(0.3-), Dark(0.2-)]",
  "AI_CULTURAL_CONTEXT": "[Contexto ESPECÍFICO, NO genérico]",
  "AI_SUBGENRES": ["Subgénero1", "Subgénero2"],
  "AI_ERA": "[Década específica coherente con contexto]",
  "AI_OCCASION": ["Ocasión1", "Ocasión2"],
  "AI_CHARACTERISTICS": ["Característica1", "Característica2", "Característica3"],
  "AI_CONFIDENCE": [FLOAT 0.7-1.0, alto solo si todos los valores son coherentes],
  "AI_ANALYZED": true,
  
  "_COHERENCIA_VALIDADA": true,
  "AI_ACOUSTICNESS": "VIENE DE ALGORITMO SPECTRAL_ANALYSIS",
  "AI_INSTRUMENTALNESS": "VIENE DE ALGORITMO VOCAL_DETECTION", 
  "AI_SPEECHINESS": "VIENE DE ALGORITMO SPEECH_DETECTION",
  "AI_LIVENESS": "VIENE DE ALGORITMO LIVENESS_DETECTION",
  "AI_BPM": "VIENE DE ALGORITMO BEAT_TRACKING",
  "AI_LOUDNESS": "VIENE DE ALGORITMO LOUDNESS_ANALYSIS",
  "AI_KEY": "VIENE DE ALGORITMO KEY_DETECTION",
  "AI_TIME_SIGNATURE": "VIENE DE ALGORITMO TIME_SIGNATURE_DETECTION",
  "AI_MODE": "VIENE DE ALGORITMO MODE_DETECTION"
}

⚠️ ANTES DE RESPONDER: VALIDA que todos los valores numéricos sean coherentes entre sí. Si hay inconsistencias, ajusta los valores para mantener coherencia musical.`;

        // Call Claude API
        const apiKey = process.env.ANTHROPIC_API_KEY;
        if (!apiKey || apiKey === 'your-claude-api-key-here') {
            throw new Error('API key not configured. Please set ANTHROPIC_API_KEY in your .env file');
        }
        
        const claudeResponse = await fetch('https://api.anthropic.com/v1/messages', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'x-api-key': apiKey,
                'anthropic-version': '2023-06-01'
            },
            body: JSON.stringify({
                model: 'claude-3-5-sonnet-20241022',
                max_tokens: 1000,
                messages: [{
                    role: 'user',
                    content: prompt
                }]
            })
        });

        if (!claudeResponse.ok) {
            const errorText = await claudeResponse.text();
            console.error('❌ Claude API Error:', claudeResponse.status, claudeResponse.statusText);
            console.error('❌ Error details:', errorText);
            throw new Error(`Claude API error: ${claudeResponse.status} ${claudeResponse.statusText}. Please check your API key.`);
        }

        const claudeData = await claudeResponse.json();
        const llmResults = JSON.parse(claudeData.content[0].text);

        console.log(`✅ Análisis LLM completado para ${path.basename(filePath)}`);

        // Save to database
        try {
            const dbFile = await database.getQuery('SELECT id FROM audio_files WHERE file_path = ?', [filePath]);
            
            if (dbFile) {
                // Save LLM metadata to database
                await database.insertLLMMetadata(dbFile.id, llmResults);
                console.log(`💾 Datos LLM guardados en base de datos para ${path.basename(filePath)}`);
                
                // Write LLM metadata to audio file using existing C++ system
                try {
                    if (metadataWriter) {
                        const writeResult = await metadataWriter.writeLLMMetadataSafe(filePath, llmResults, true);
                        if (writeResult.success) {
                            console.log(`🎵 Metadatos LLM escritos al archivo: ${path.basename(filePath)}`);
                        } else {
                            console.warn(`⚠️ No se pudieron escribir metadatos LLM al archivo: ${writeResult.error}`);
                        }
                    }
                } catch (fileWriteError) {
                    console.error(`❌ Error escribiendo metadatos LLM al archivo:`, fileWriteError);
                }
                
                // Invalidate cache
                cache.invalidateFile(filePath);
            }
        } catch (saveError) {
            console.error(`❌ Error guardando resultados LLM:`, saveError);
        }

        return {
            success: true,
            results: llmResults,
            description: llmResults.LLM_DESCRIPTION,
            mood: llmResults.LLM_MOOD,
            genre: llmResults.LLM_GENRE
        };

    } catch (error) {
        console.error(`❌ Error en análisis LLM:`, error);
        
        return {
            success: false,
            error: error.message
        };
    }
});

// REMOVED: Mock analysis code that was generating fake coherent results

// 🧠 IPC Handler: Análisis LLM únicamente con placeholders AI_*
ipcMain.handle('analyze-llm-only', async (event, filePath) => {
    try {
        console.log(`🧠 Análisis LLM únicamente: ${path.basename(filePath)}`);
        
        // ✅ VERIFICAR SI YA ESTÁ ANALIZADO (evitar re-análisis)
        try {
            const existingData = await database.getQuery(
                `SELECT af.id, lm.AI_ANALYZED, lm.LLM_ANALYSIS_DATE 
                 FROM audio_files af 
                 LEFT JOIN llm_metadata lm ON af.id = lm.file_id 
                 WHERE af.file_path = ?`, 
                [filePath]
            );
            
            if (existingData && existingData.AI_ANALYZED === 'true') {
                console.log(`⏭️ Archivo ya analizado, saltando: ${path.basename(filePath)}`);
                return {
                    success: true,
                    message: 'Archivo ya analizado previamente',
                    skipReason: 'already_analyzed',
                    analysisDate: existingData.LLM_ANALYSIS_DATE
                };
            }
        } catch (checkError) {
            console.warn(`⚠️ No se pudo verificar estado de análisis: ${checkError.message}`);
            // Continuar con análisis si no se puede verificar
        }
        
        // 📖 Leer metadatos existentes del archivo
        const fileMetadata = await metadataWriter.readExistingMetadata(filePath);
        
        // 🧠 PROMPT MEJORADO PARA ANÁLISIS LLM CON PLACEHOLDERS AI_*
        const prompt = `Como EXPERTO MUSICÓLOGO y DJ PROFESIONAL con conocimiento profundo de análisis musical, teoría armónica y psicoacústica, analiza este archivo de música.

📄 INFORMACIÓN DEL ARCHIVO:
- Archivo: ${path.basename(filePath)}
- Título: ${fileMetadata.metadata?.title || 'Desconocido'}
- Artista: ${fileMetadata.metadata?.artist || 'Desconocido'}
- Álbum: ${fileMetadata.metadata?.album || 'Desconocido'}
- Género actual: ${fileMetadata.metadata?.genre || 'Sin definir'} (IMPORTANTE: Analiza y corrige si es necesario)
- Año: ${fileMetadata.metadata?.year || 'Desconocido'}

🎯 REGLAS DE ANÁLISIS MUSICAL:
1. **CORRECCIÓN DE GÉNERO OBLIGATORIA** - Si el género actual es incorrecto, DEBES corregirlo
2. **COHERENCIA MUSICAL** - Todos los campos deben ser coherentes entre sí
3. **CONTEXTO CULTURAL ESPECÍFICO** - No usar términos genéricos
4. **ANÁLISIS PROFESIONAL** - Basado en conocimiento musical real

🎵 ESTRUCTURA JSON REQUERIDA (SOLO JSON, SIN TEXTO ADICIONAL):

{
  "LLM_DESCRIPTION": "Análisis musical detallado considerando estilo, instrumentación y contexto histórico (máximo 250 palabras)",
  "LLM_MOOD": "Estado de ánimo principal de la canción",
  "LLM_GENRE": "OBLIGATORIO: Género principal CORREGIDO basado en análisis musical profesional",
  "LLM_SUBGENRE": "Subgénero específico coherente con el contexto cultural",
  "LLM_CONTEXT": "Contexto cultural ESPECÍFICO (ej: 90s Belgian Eurodance, Classic Detroit Techno)",
  "LLM_OCCASIONS": ["Ocasión1", "Ocasión2"],
  "LLM_ENERGY_LEVEL": "Alto/Medio/Bajo",
  "LLM_DANCEABILITY": "Alta/Media/Baja",
  "LLM_RECOMMENDATIONS": "Recomendaciones específicas para DJs basadas en características musicales",
  
  "AI_ACOUSTICNESS": "ALGORITMO SPECTRAL_ANALYSIS",
  "AI_ANALYZED": "ALGORITMO COMPLETION_TRACKER",
  "AI_BPM": "ALGORITMO BEAT_TRACKING",
  "AI_CHARACTERISTICS": "ALGORITMO FEATURE_EXTRACTION",
  "AI_CONFIDENCE": "ALGORITMO CONFIDENCE_CALCULATOR",
  "AI_CULTURAL_CONTEXT": "ALGORITMO CULTURAL_ANALYZER",
  "AI_DANCEABILITY": "ALGORITMO DANCEABILITY_DETECTOR",
  "AI_ENERGY": "ALGORITMO ENERGY_ANALYZER",
  "AI_ERA": "ALGORITMO ERA_CLASSIFIER",
  "AI_INSTRUMENTALNESS": "ALGORITMO VOCAL_DETECTION",
  "AI_KEY": "ALGORITMO KEY_DETECTION",
  "AI_LIVENESS": "ALGORITMO LIVENESS_DETECTION",
  "AI_LOUDNESS": "ALGORITMO LOUDNESS_ANALYSIS",
  "AI_MODE": "ALGORITMO MODE_DETECTION",
  "AI_MOOD": "ALGORITMO MOOD_CLASSIFIER",
  "AI_OCCASION": "ALGORITMO OCCASION_PREDICTOR",
  "AI_SPEECHINESS": "ALGORITMO SPEECH_DETECTION",
  "AI_SUBGENRES": "ALGORITMO SUBGENRE_CLASSIFIER",
  "AI_TIME_SIGNATURE": "ALGORITMO TIME_SIGNATURE_DETECTION",
  "AI_VALENCE": "ALGORITMO VALENCE_ANALYZER"
}`;

        // Llamada a Claude API
        const apiKey = process.env.ANTHROPIC_API_KEY;
        if (!apiKey || apiKey === 'your-claude-api-key-here') {
            throw new Error('API key not configured. Please set ANTHROPIC_API_KEY in your .env file');
        }
        
        const claudeResponse = await fetch('https://api.anthropic.com/v1/messages', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'x-api-key': apiKey,
                'anthropic-version': '2023-06-01'
            },
            body: JSON.stringify({
                model: 'claude-3-5-sonnet-20241022',
                max_tokens: 1000,
                messages: [{
                    role: 'user',
                    content: prompt
                }]
            })
        });

        if (!claudeResponse.ok) {
            const errorText = await claudeResponse.text();
            console.error('❌ Claude API Error:', claudeResponse.status, claudeResponse.statusText);
            // REMOVED: Mock LLM fallback
            throw new Error('Claude API not available and mock analysis is prohibited');
        }

        const claudeData = await claudeResponse.json();
        const llmResults = JSON.parse(claudeData.content[0].text);

        console.log(`✅ Análisis LLM únicamente completado para ${path.basename(filePath)}`);
        
        return await processLLMOnlyResults(filePath, llmResults);

    } catch (error) {
        console.error('❌ Error en análisis LLM únicamente:', error);
        return {
            success: false,
            error: error.message,
            method: 'llm_only_error'
        };
    }
});

// 🛠️ Función helper para procesar resultados LLM únicamente
async function processLLMOnlyResults(filePath, llmResults) {
    try {
        // 💾 Guardar en base de datos
        const dbFile = await database.getQuery('SELECT id FROM audio_files WHERE file_path = ?', [filePath]);
        
        if (dbFile) {
            // Guardar metadatos LLM + placeholders AI_*
            await database.insertLLMMetadata(dbFile.id, llmResults);
            console.log(`💾 Datos LLM + placeholders AI_* guardados en BD para ${path.basename(filePath)}`);
            
            // ✅ CRÍTICO: ACTUALIZAR audio_files.genre CON LLM_GENRE (SIEMPRE)
            if (llmResults.LLM_GENRE) {
                await database.runQuery(
                    'UPDATE audio_files SET genre = ? WHERE id = ?',
                    [llmResults.LLM_GENRE, dbFile.id]
                );
                console.log(`🎵 GENRE actualizado en BD: ${llmResults.LLM_GENRE} (ID: ${dbFile.id})`);
            }
            
            // 🎵 Escribir metadatos LLM a archivo de audio
            try {
                if (metadataWriter) {
                    const writeResult = await metadataWriter.writeLLMMetadataSafe(filePath, llmResults, true);
                    if (writeResult.success) {
                        console.log(`🎵 Metadatos LLM + placeholders escritos al archivo: ${path.basename(filePath)}`);
                    } else {
                        console.warn(`⚠️ No se pudieron escribir metadatos al archivo: ${writeResult.error}`);
                    }
                }
            } catch (fileWriteError) {
                console.error(`❌ Error escribiendo metadatos al archivo:`, fileWriteError);
            }
            
            // 🔄 Invalidar cache
            cache.invalidateFile(filePath);
        }

        return {
            success: true,
            method: 'llm_only_analysis',
            results: llmResults,
            message: `✅ Análisis LLM completado con placeholders AI_* (Género: ${llmResults.LLM_GENRE})`
        };

    } catch (error) {
        console.error(`❌ Error procesando resultados LLM:`, error);
        return {
            success: false,
            error: error.message,
            method: 'llm_processing_error'
        };
    }
}

// 📊 IPC Handler: Obtener conteo exacto de registros en base de datos
ipcMain.handle('get-database-counts', async () => {
    try {
        const audioFilesCount = await database.runQuery('SELECT COUNT(*) as count FROM audio_files');
        const llmMetadataCount = await database.runQuery('SELECT COUNT(*) as count FROM llm_metadata');
        const foldersCount = await database.runQuery('SELECT COUNT(*) as count FROM folders');
        
        // Obtener estadísticas adicionales
        const totalSizeResult = await database.runQuery('SELECT SUM(file_size) as total_size FROM audio_files');
        const analyzedCount = await database.runQuery('SELECT COUNT(*) as count FROM audio_files af WHERE EXISTS (SELECT 1 FROM llm_metadata lm WHERE lm.file_id = af.id)');
        
        // Conteo por extensión
        const extensionStats = await database.runQuery(`
            SELECT 
                file_extension,
                COUNT(*) as count,
                SUM(file_size) as total_size
            FROM audio_files 
            GROUP BY file_extension 
            ORDER BY count DESC
        `);
        
        return {
            audioFiles: audioFilesCount.count || 0,
            llmMetadata: llmMetadataCount.count || 0,
            folders: foldersCount.count || 0,
            analyzedFiles: analyzedCount.count || 0,
            totalSizeBytes: totalSizeResult.total_size || 0,
            totalSizeGB: Math.round((totalSizeResult.total_size || 0) / (1024 * 1024 * 1024) * 100) / 100,
            extensionBreakdown: extensionStats || [],
            analysisProgress: audioFilesCount.count > 0 ? Math.round((analyzedCount.count / audioFilesCount.count) * 100) : 0
        };
    } catch (error) {
        console.error('❌ Error getting database counts:', error);
        return {
            audioFiles: 0,
            llmMetadata: 0,
            folders: 0,
            analyzedFiles: 0,
            totalSizeBytes: 0,
            totalSizeGB: 0,
            extensionBreakdown: [],
            analysisProgress: 0,
            error: error.message
        };
    }
});

// 🔧 IPC Handler: Limpiar datos duplicados AI_*
ipcMain.handle('clean-duplicate-ai-data', async (event) => {
    try {
        console.log('🔧 Iniciando limpieza de datos AI_* duplicados...');
        const result = await database.cleanDuplicateAIFields();
        console.log(`✅ Limpieza completada: ${result.cleanedRecords} registros limpiados`);
        return {
            success: true,
            cleanedRecords: result.cleanedRecords,
            message: `Se limpiaron ${result.cleanedRecords} registros con datos duplicados`
        };
    } catch (error) {
        console.error('❌ Error en limpieza de datos:', error);
        return {
            success: false,
            error: error.message
        };
    }
});

// 🔍 IPC Handler: Validar consistencia de datos AI
ipcMain.handle('validate-ai-consistency', async (event) => {
    try {
        console.log('🔍 Validando consistencia de datos AI...');
        const result = await database.validateAIConsistency();
        console.log(`🔍 Validación completada: ${result.inconsistencies.length} inconsistencias encontradas`);
        return {
            success: true,
            inconsistencies: result.inconsistencies,
            totalRecords: result.totalRecords,
            message: `Se encontraron ${result.inconsistencies.length} inconsistencias en ${result.totalRecords} registros`
        };
    } catch (error) {
        console.error('❌ Error validando consistencia:', error);
        return {
            success: false,
            error: error.message
        };
    }
});

app.whenReady().then(createWindow);

let isClosing = false;

app.on('window-all-closed', async () => {
    if (isClosing) return;
    isClosing = true;
    
    // Cerrar base de datos antes de salir
    if (database) {
        try {
            await database.close();
        } catch (error) {
            console.error('Error during database close:', error);
        }
        database = null;
    }
    
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
        createWindow();
    }
});

// Manejar cierre forzado
app.on('before-quit', async (event) => {
    if (isClosing) return;
    
    if (database) {
        event.preventDefault();
        isClosing = true;
        
        try {
            await database.close();
        } catch (error) {
            console.error('Error during forced close:', error);
        }
        
        database = null;
        app.quit();
    }
});