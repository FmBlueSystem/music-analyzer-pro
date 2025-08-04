const fs = require('fs');
const path = require('path');
const NodeID3 = require('node-id3');
const { execSync } = require('child_process');

class MetadataWriter {
    constructor(cppAddon = null) {
        this.supportedFormats = ['.mp3', '.flac', '.m4a', '.ogg', '.wav'];
        this.musicMetadata = null;
        this.cppAddon = cppAddon;
        this.initMusicMetadata();
    }

    /**
     * 🎵 Inicializar music-metadata de forma asíncrona
     */
    async initMusicMetadata() {
        try {
            this.musicMetadata = await import('music-metadata');
            console.log('✅ music-metadata loaded successfully');
        } catch (error) {
            // music-metadata will be loaded via fallback
        }
    }

    /**
     * 🔍 Leer metadatos existentes con detección de Mixed In Key
     * @param {string} filePath - Ruta del archivo de audio
     * @returns {Object} Metadatos existentes y detección de Mixed In Key
     */
    async readExistingMetadata(filePath) {
        try {
            // 🎵 Asegurar que music-metadata esté cargado
            if (!this.musicMetadata) {
                await this.initMusicMetadata();
            }
            
            if (!this.musicMetadata) {
                // Fallback básico sin music-metadata
                return this.readBasicMetadata(filePath);
            }
            
            const metadata = await this.musicMetadata.parseFile(filePath);
            const fileName = path.basename(filePath);
            
            // 🔍 Detectar patrones de Mixed In Key en el nombre del archivo
            const mixedInKeyPatterns = [
                /\b(\d+[AB])\b/i,                    // Notación Camelot (e.g., "7A", "12B")
                /\b(\d+\.\d+)\s*BPM\b/i,            // BPM específico (e.g., "128.5 BPM")
                /Key:\s*([A-G][#b]?\s*[Mm]?[ai]?[jn]?[or]?)/i,  // Formato Key
                /\b(Energy\s*\d+)\b/i               // Energy level
            ];
            
            const hasMixedInKeyPattern = mixedInKeyPatterns.some(pattern => 
                pattern.test(fileName)
            );
            
            // 🔍 Verificar metadatos existentes para BPM y Key profesionales
            const existingBPM = metadata.common.bpm;
            const existingKey = metadata.common.key;
            
            // 🛡️ Detectar si hay datos profesionales (Mixed In Key, Serato, etc.)
            const hasProfessionalBPM = existingBPM && (
                existingBPM % 1 !== 0 || // BPM con decimales (típico de Mixed In Key)
                existingBPM > 60 && existingBPM < 200 // Rango típico de música electrónica
            );
            
            const hasProfessionalKey = existingKey && (
                existingKey.includes('m') || // Notación menor (e.g., "Am")
                /[A-G][#b]?\s*(maj|min|major|minor)?/i.test(existingKey) ||
                /\d+[AB]/i.test(existingKey) // Camelot notation
            );

            return {
                exists: true,
                fileName,
                filePath,
                hasMixedInKeyPattern,
                metadata: metadata.common,
                format: metadata.format,
                
                // 🛡️ Datos de preservación
                preservation: {
                    existingBPM: hasProfessionalBPM ? existingBPM : null,
                    existingKey: hasProfessionalKey ? existingKey : null,
                    source: hasMixedInKeyPattern || hasProfessionalBPM || hasProfessionalKey ? 
                           'mixed_in_key_detected' : 'standard_metadata',
                    shouldPreserve: hasMixedInKeyPattern || hasProfessionalBPM || hasProfessionalKey
                },
                
                // 📊 Metadatos LLM existentes (si los hay)
                llmMetadata: this.extractLLMMetadata(metadata)
            };
        } catch (error) {
            console.error('Error reading metadata:', error);
            return { 
                exists: false, 
                error: error.message,
                preservation: { shouldPreserve: false }
            };
        }
    }

    /**
     * 📄 Leer metadatos básicos sin music-metadata (fallback)
     * @param {string} filePath - Ruta del archivo
     * @returns {Object} Metadatos básicos
     */
    readBasicMetadata(filePath) {
        const fileName = path.basename(filePath);
        
        // 🔍 Detectar patrones de Mixed In Key en el nombre del archivo
        const mixedInKeyPatterns = [
            /\b(\d+[AB])\b/i,                    // Notación Camelot (e.g., "7A", "12B")
            /\b(\d+\.\d+)\s*BPM\b/i,            // BPM específico (e.g., "128.5 BPM")
            /Key:\s*([A-G][#b]?\s*[Mm]?[ai]?[jn]?[or]?)/i,  // Formato Key
            /\b(Energy\s*\d+)\b/i               // Energy level
        ];
        
        const hasMixedInKeyPattern = mixedInKeyPatterns.some(pattern => 
            pattern.test(fileName)
        );

        return {
            exists: true,
            fileName,
            filePath,
            hasMixedInKeyPattern,
            metadata: {}, // Sin metadatos detallados en modo fallback
            format: { container: path.extname(filePath).substring(1) },
            
            // 🛡️ Datos de preservación
            preservation: {
                existingBPM: null,
                existingKey: null,
                source: hasMixedInKeyPattern ? 'mixed_in_key_pattern' : 'none',
                shouldPreserve: hasMixedInKeyPattern
            },
            
            // 📊 Sin metadatos LLM en modo fallback
            llmMetadata: {}
        };
    }

    /**
     * 📄 Extraer metadatos LLM existentes de campos TXXX
     * @param {Object} metadata - Metadatos parseados
     * @returns {Object} Metadatos LLM encontrados
     */
    extractLLMMetadata(metadata) {
        const llmFields = {};
        
        // Buscar en comentarios personalizados (TXXX frames)
        if (metadata.native && metadata.native['ID3v2.4']) {
            const id3Tags = metadata.native['ID3v2.4'];
            
            id3Tags.forEach(tag => {
                if (tag.id === 'TXXX' && tag.value && tag.value.description) {
                    const description = tag.value.description.toLowerCase();
                    
                    // 🤖 Campos LLM conocidos
                    const llmFieldMap = {
                        'bpm_llm': 'bpm_llm',
                        'energy_llm': 'energy',
                        'mood_llm': 'mood',
                        'danceability_llm': 'danceability',
                        'valence_llm': 'valence',
                        'ai_confidence': 'ai_confidence',
                        'analyzed_by': 'analyzed_by',
                        'hamms_vector': 'hamms_vector'
                    };
                    
                    if (llmFieldMap[description]) {
                        llmFields[llmFieldMap[description]] = tag.value.text;
                    }
                }
            });
        }
        
        return llmFields;
    }

    /**
     * 💾 Escribir metadatos LLM de forma segura (preservando Mixed In Key)
     * @param {string} filePath - Ruta del archivo
     * @param {Object} llmMetadata - Metadatos LLM a escribir
     * @param {boolean} preserveMixedInKey - Si preservar datos profesionales
     * @returns {Object} Resultado de la operación
     */
    async writeLLMMetadataSafe(filePath, llmMetadata, preserveMixedInKey = true) {
        try {
            // 📖 PASO 1: Leer metadatos existentes
            const existing = await this.readExistingMetadata(filePath);
            
            if (!existing.exists) {
                throw new Error('No se pudo leer el archivo de audio');
            }

            // 🛡️ PASO 2: Preparar metadatos seguros
            const safeMetadata = { ...llmMetadata };
            
            if (preserveMixedInKey && existing.preservation.shouldPreserve) {
                console.log(`🛡️ Preservando datos profesionales en: ${path.basename(filePath)}`);
                
                // ❌ NO sobrescribir BPM/Key profesionales
                delete safeMetadata.bpm;
                delete safeMetadata.key;
                
                // ✅ Usar campo separado para BPM LLM
                if (llmMetadata.bpm && !safeMetadata.bpm_llm) {
                    safeMetadata.bpm_llm = llmMetadata.bpm;
                }
            }

            // 🎵 PASO 3: Intentar escritura directa a archivo de audio
            const directWriteResult = await this.writeDirectToAudioFile(filePath, safeMetadata, existing);
            
            if (directWriteResult.success) {
                // ✅ Escritura directa exitosa
                return {
                    success: true,
                    method: directWriteResult.method,
                    preserved: existing.preservation.shouldPreserve,
                    preservedFields: existing.preservation.shouldPreserve ? 
                        ['BPM', 'Key'] : [],
                    llmFieldsWritten: directWriteResult.fieldsWritten,
                    message: '✅ Metadatos escritos directamente al archivo de audio'
                };
            } else {
                // 📄 Fallback a JSON si la escritura directa falla
                // Using JSON fallback for compatibility
                const jsonResult = await this.writeToJSON(filePath, safeMetadata, existing);
                
                return {
                    success: true,
                    method: 'json_fallback',
                    preserved: existing.preservation.shouldPreserve,
                    preservedFields: existing.preservation.shouldPreserve ? 
                        ['BPM', 'Key'] : [],
                    path: jsonResult.jsonPath,
                    llmFieldsWritten: Object.keys(safeMetadata).length,
                    warning: 'Usado fallback JSON: ' + directWriteResult.error
                };
            }
            
        } catch (error) {
            console.error('Error writing safe metadata:', error);
            throw new Error(`Error escribiendo metadata: ${error.message}`);
        }
    }

    /**
     * 🎵 Escribir metadatos directamente al archivo de audio
     * @param {string} filePath - Ruta del archivo de audio
     * @param {Object} llmMetadata - Metadatos LLM a escribir
     * @param {Object} existing - Metadatos existentes
     * @returns {Object} Resultado de la escritura directa
     */
    async writeDirectToAudioFile(filePath, llmMetadata, existing) {
        const ext = path.extname(filePath).toLowerCase();
        console.log(`🔧 Intentando escritura directa en ${ext}: ${path.basename(filePath)}`);
        
        try {
            if (ext === '.mp3') {
                console.log(`🎵 Procesando MP3: ${path.basename(filePath)}`);
                return await this.writeToMP3(filePath, llmMetadata, existing);
            } else if (['.flac', '.ogg'].includes(ext)) {
                console.log(`🎼 FLAC/OGG no implementado: ${path.basename(filePath)}`);
                return await this.writeToVorbis(filePath, llmMetadata, existing);
            } else if (ext === '.m4a') {
                console.log(`🎶 M4A no implementado: ${path.basename(filePath)}`);
                return await this.writeToMP4(filePath, llmMetadata, existing);
            } else {
                console.log(`❌ Formato no soportado ${ext}: ${path.basename(filePath)}`);
                return {
                    success: false,
                    error: `Formato ${ext} no soportado para escritura directa`,
                    method: 'unsupported_format'
                };
            }
        } catch (error) {
            console.error(`❌ Error en escritura directa ${ext}:`, error.message);
            return {
                success: false,
                error: error.message,
                method: 'direct_write_error'
            };
        }
    }

    /**
     * 🎵 Escribir metadatos a archivo MP3 usando node-id3
     * @param {string} filePath - Ruta del archivo MP3
     * @param {Object} llmMetadata - Metadatos LLM
     * @param {Object} existing - Metadatos existentes
     * @returns {Object} Resultado de escritura
     */
    async writeToMP3(filePath, llmMetadata, existing) {
        try {
            console.log(`🎵 Iniciando escritura MP3: ${path.basename(filePath)}`);
            
            // 📖 Leer tags existentes para preservar datos importantes
            const existingTags = NodeID3.read(filePath) || {};
            console.log(`📖 Tags existentes leídos: ${Object.keys(existingTags).length} campos`);
            
            // 🛡️ Preservar Mixed In Key si está detectado
            const preservedTags = {};
            if (existing.preservation.shouldPreserve) {
                // Mantener BPM y Key originales
                if (existingTags.bpm) preservedTags.bpm = existingTags.bpm;
                if (existingTags.key) preservedTags.key = existingTags.key;
            }
            
            // 🏷️ Preparar tags ID3v2.4 con metadatos LLM
            const id3Tags = {
                // 📋 Mantener metadatos básicos existentes si no se proporcionan nuevos
                title: llmMetadata.title || existingTags.title || '',
                artist: llmMetadata.artist || existingTags.artist || '',
                album: llmMetadata.album || existingTags.album || '',
                // ✅ GENRE: PRIORIDAD 1: LLM_GENRE (análisis profesional), FALLBACK: existente
                genre: llmMetadata.LLM_GENRE || existingTags.genre || '',
                year: llmMetadata.year || existingTags.year || '',
                
                // 🛡️ Preservar campos profesionales
                ...preservedTags,
                
                // 🤖 Metadatos LLM como campos TXXX (personalizados)
                userDefinedText: [
                    // ✅ CAMPOS AI_* PRINCIPALES
                    ...(llmMetadata.AI_ACOUSTICNESS !== undefined ? [{ description: 'AI_ACOUSTICNESS', value: llmMetadata.AI_ACOUSTICNESS.toString() }] : []),
                    ...(llmMetadata.AI_ANALYZED !== undefined ? [{ description: 'AI_ANALYZED', value: llmMetadata.AI_ANALYZED.toString() }] : []),
                    ...(llmMetadata.AI_BPM !== undefined ? [{ description: 'AI_BPM', value: llmMetadata.AI_BPM.toString() }] : []),
                    ...(llmMetadata.AI_CHARACTERISTICS ? [{ description: 'AI_CHARACTERISTICS', value: Array.isArray(llmMetadata.AI_CHARACTERISTICS) ? llmMetadata.AI_CHARACTERISTICS.join(',') : llmMetadata.AI_CHARACTERISTICS }] : []),
                    ...(llmMetadata.AI_CONFIDENCE !== undefined ? [{ description: 'AI_CONFIDENCE', value: llmMetadata.AI_CONFIDENCE.toString() }] : []),
                    ...(llmMetadata.AI_CULTURAL_CONTEXT ? [{ description: 'AI_CULTURAL_CONTEXT', value: llmMetadata.AI_CULTURAL_CONTEXT }] : []),
                    ...(llmMetadata.AI_DANCEABILITY !== undefined ? [{ description: 'AI_DANCEABILITY', value: llmMetadata.AI_DANCEABILITY.toString() }] : []),
                    ...(llmMetadata.AI_ENERGY !== undefined ? [{ description: 'AI_ENERGY', value: llmMetadata.AI_ENERGY.toString() }] : []),
                    ...(llmMetadata.AI_ERA ? [{ description: 'AI_ERA', value: llmMetadata.AI_ERA }] : []),
                    ...(llmMetadata.AI_INSTRUMENTALNESS !== undefined ? [{ description: 'AI_INSTRUMENTALNESS', value: llmMetadata.AI_INSTRUMENTALNESS.toString() }] : []),
                    ...(llmMetadata.AI_KEY ? [{ description: 'AI_KEY', value: llmMetadata.AI_KEY }] : []),
                    ...(llmMetadata.AI_LIVENESS !== undefined ? [{ description: 'AI_LIVENESS', value: llmMetadata.AI_LIVENESS.toString() }] : []),
                    ...(llmMetadata.AI_LOUDNESS !== undefined ? [{ description: 'AI_LOUDNESS', value: llmMetadata.AI_LOUDNESS.toString() }] : []),
                    ...(llmMetadata.AI_MODE ? [{ description: 'AI_MODE', value: llmMetadata.AI_MODE }] : []),
                    ...(llmMetadata.AI_MOOD ? [{ description: 'AI_MOOD', value: llmMetadata.AI_MOOD }] : []),
                    ...(llmMetadata.AI_OCCASION ? [{ description: 'AI_OCCASION', value: Array.isArray(llmMetadata.AI_OCCASION) ? llmMetadata.AI_OCCASION.join(',') : llmMetadata.AI_OCCASION }] : []),
                    ...(llmMetadata.AI_SPEECHINESS !== undefined ? [{ description: 'AI_SPEECHINESS', value: llmMetadata.AI_SPEECHINESS.toString() }] : []),
                    ...(llmMetadata.AI_SUBGENRES ? [{ description: 'AI_SUBGENRES', value: Array.isArray(llmMetadata.AI_SUBGENRES) ? llmMetadata.AI_SUBGENRES.join(',') : llmMetadata.AI_SUBGENRES }] : []),
                    ...(llmMetadata.AI_TIME_SIGNATURE !== undefined ? [{ description: 'AI_TIME_SIGNATURE', value: llmMetadata.AI_TIME_SIGNATURE.toString() }] : []),
                    ...(llmMetadata.AI_VALENCE !== undefined ? [{ description: 'AI_VALENCE', value: llmMetadata.AI_VALENCE.toString() }] : []),
                    
                    // 🔄 CAMPOS DE COMPATIBILIDAD (para sistemas legacy)
                    ...(llmMetadata.bpm_llm ? [{ description: 'BPM_LLM', value: llmMetadata.bpm_llm.toString() }] : []),
                    ...(llmMetadata.energy ? [{ description: 'ENERGY_LLM', value: llmMetadata.energy.toString() }] : []),
                    ...(llmMetadata.mood ? [{ description: 'MOOD_LLM', value: llmMetadata.mood }] : []),
                    ...(llmMetadata.danceability ? [{ description: 'DANCEABILITY_LLM', value: llmMetadata.danceability.toString() }] : []),
                    ...(llmMetadata.valence ? [{ description: 'VALENCE_LLM', value: llmMetadata.valence.toString() }] : []),
                    ...(llmMetadata.ai_confidence ? [{ description: 'AI_CONFIDENCE', value: llmMetadata.ai_confidence.toString() }] : []),
                    ...(llmMetadata.analyzed_by ? [{ description: 'ANALYZED_BY', value: llmMetadata.analyzed_by }] : []),
                    
                    // Campos HAMMS
                    ...(llmMetadata.hamms_vector ? [{ 
                        description: 'HAMMS_VECTOR', 
                        value: typeof llmMetadata.hamms_vector === 'string' ? 
                               llmMetadata.hamms_vector : 
                               JSON.stringify(llmMetadata.hamms_vector) 
                    }] : []),
                    
                    // Metadatos adicionales de DJ
                    ...(llmMetadata.subgenre ? [{ description: 'SUBGENRE_LLM', value: llmMetadata.subgenre }] : []),
                    ...(llmMetadata.era ? [{ description: 'ERA_LLM', value: llmMetadata.era }] : []),
                    ...(llmMetadata.vocal_presence ? [{ description: 'VOCAL_PRESENCE', value: llmMetadata.vocal_presence }] : []),
                    ...(llmMetadata.structure ? [{ description: 'STRUCTURE_LLM', value: llmMetadata.structure }] : []),
                    ...(llmMetadata.drop_time ? [{ description: 'DROP_TIME_LLM', value: llmMetadata.drop_time }] : []),
                    ...(llmMetadata.energy_curve ? [{ description: 'ENERGY_CURVE', value: llmMetadata.energy_curve }] : []),
                    ...(llmMetadata.crowd_response ? [{ description: 'CROWD_RESPONSE', value: llmMetadata.crowd_response }] : []),
                    ...(llmMetadata.occasion ? [{ description: 'OCCASION_LLM', value: llmMetadata.occasion }] : []),
                    ...(llmMetadata.characteristics ? [{ description: 'CHARACTERISTICS', value: llmMetadata.characteristics }] : []),
                    
                    // Metadatos técnicos
                    ...(llmMetadata.tempo_stability ? [{ description: 'TEMPO_STABILITY', value: llmMetadata.tempo_stability.toString() }] : []),
                    ...(llmMetadata.production_quality ? [{ description: 'PRODUCTION_QUALITY', value: llmMetadata.production_quality.toString() }] : []),
                    ...(llmMetadata.mastering_loudness ? [{ description: 'MASTERING_LOUDNESS', value: llmMetadata.mastering_loudness.toString() }] : []),
                    ...(llmMetadata.dynamic_range ? [{ description: 'DYNAMIC_RANGE', value: llmMetadata.dynamic_range.toString() }] : []),
                    
                    // Tags personalizados
                    ...(llmMetadata.custom_tags && Array.isArray(llmMetadata.custom_tags) ? 
                        [{ description: 'CUSTOM_TAGS_LLM', value: llmMetadata.custom_tags.join(', ') }] : []),
                    
                    // 📊 METADATOS TÉCNICOS
                    ...(llmMetadata.analysis_timestamp ? [{ description: 'AI_ANALYSIS_TIMESTAMP', value: llmMetadata.analysis_timestamp }] : []),
                    
                    // 🚀 TIMESTAMP AUTOMÁTICO
                    { description: 'AI_LAST_UPDATED', value: new Date().toISOString() },
                    { description: 'AI_ANALYZER_VERSION', value: '2.0.0' }
                ].filter(tag => tag.value !== undefined && tag.value !== null && tag.value !== '')
            };

            // ✍️ Escribir tags al archivo MP3 (orden correcto: filePath, tags)
            console.log(`✍️ Escribiendo ${id3Tags.userDefinedText?.length || 0} campos LLM a MP3`);
            const success = NodeID3.write(filePath, id3Tags);
            console.log(`📝 Resultado NodeID3.write: ${success}`);
            
            if (success) {
                const fieldsWritten = id3Tags.userDefinedText ? id3Tags.userDefinedText.length : 0;
                
                return {
                    success: true,
                    method: 'mp3_id3v2_direct',
                    fieldsWritten: fieldsWritten + 4, // +4 por title, artist, album, genre
                    preservedFields: existing.preservation.shouldPreserve ? ['BPM', 'Key'] : []
                };
            } else {
                return {
                    success: false,
                    error: 'NodeID3.write returned false',
                    method: 'mp3_write_failed'
                };
            }
            
        } catch (error) {
            console.error('Error writing to MP3:', error);
            return {
                success: false,
                error: error.message,
                method: 'mp3_write_error'
            };
        }
    }

    /**
     * 🎵 Escribir metadatos a archivos Vorbis (FLAC/OGG) - Placeholder
     * @param {string} filePath - Ruta del archivo
     * @param {Object} llmMetadata - Metadatos LLM
     * @param {Object} existing - Metadatos existentes
     * @returns {Object} Resultado
     */
    async writeToVorbis(filePath, llmMetadata, existing) {
        const ext = path.extname(filePath).toLowerCase();
        
        if (ext === '.flac') {
            return await this.writeToFLAC(filePath, llmMetadata, existing);
        } else {
            // OGG no implementado aún
            return {
                success: false,
                error: 'Escritura a OGG no implementada aún. Usando fallback JSON.',
                method: 'ogg_not_implemented'
            };
        }
    }

    /**
     * 🎼 Escribir metadatos a archivo FLAC usando Vorbis comments directamente
     * @param {string} filePath - Ruta del archivo FLAC
     * @param {Object} llmMetadata - Metadatos LLM
     * @param {Object} existing - Metadatos existentes
     * @returns {Object} Resultado de escritura
     */
    async writeToFLAC(filePath, llmMetadata, existing) {
        console.log(`🎼 Iniciando escritura directa FLAC: ${path.basename(filePath)}`);
        
        try {
            // 🔧 PRIORIDAD 1: Usar addon C++ si está disponible
            if (this.cppAddon) {
                console.log(`🚀 Usando addon C++ para FLAC: ${path.basename(filePath)}`);
                return await this.writeFLACWithCpp(filePath, llmMetadata, existing);
            }
            
            // 🎵 PRIORIDAD 2: Escribir directamente con metaflac (herramienta estándar)
            const directResult = await this.writeFLACDirect(filePath, llmMetadata, existing);
            if (directResult.success) {
                return directResult;
            }
            
            // 📄 FALLBACK: JSON sidecar como último recurso
            console.warn(`⚠️ Fallback a JSON para ${path.basename(filePath)}: ${directResult.error}`);
            const jsonResult = await this.writeToJSON(filePath, llmMetadata, existing);
            return {
                success: true,
                method: 'flac_json_fallback',
                preservedFields: existing.preservation.shouldPreserve ? ['BPM', 'INITIALKEY'] : [],
                path: jsonResult.jsonPath,
                llmFieldsWritten: Object.keys(llmMetadata).length,
                warning: `Fallback JSON usado: ${directResult.error}`
            };
            
        } catch (error) {
            console.error(`❌ Error escribiendo FLAC ${path.basename(filePath)}:`, error);
            return {
                success: false,
                error: error.message,
                method: 'flac_write_error'
            };
        }
    }
    
    /**
     * 🎵 Escribir metadatos FLAC directamente usando metaflac
     * @param {string} filePath - Ruta del archivo FLAC
     * @param {Object} llmMetadata - Metadatos LLM
     * @param {Object} existing - Metadatos existentes
     * @returns {Object} Resultado de escritura
     */
    async writeFLACDirect(filePath, llmMetadata, existing) {
        try {
            console.log(`🔧 Escribiendo Vorbis comments directamente: ${path.basename(filePath)}`);
            
            // 1. Verificar que metaflac está disponible
            try {
                execSync('which metaflac', { stdio: 'ignore' });
            } catch (error) {
                return {
                    success: false,
                    error: 'metaflac no está instalado en el sistema',
                    method: 'metaflac_not_found'
                };
            }
            
            // 2. Preparar metadatos para Vorbis comments
            const vorbisComments = this.prepareVorbisComments(llmMetadata, existing);
            
            if (vorbisComments.length === 0) {
                return {
                    success: false,
                    error: 'No hay metadatos válidos para escribir',
                    method: 'no_metadata'
                };
            }
            
            // 3. Crear una copia temporal del archivo para seguridad
            const tempFile = filePath + '.tmp';
            fs.copyFileSync(filePath, tempFile);
            
            try {
                // 4. ✅ PRIMERO: Eliminar todos los tags AI_* existentes para evitar concatenación
                const aiFieldsToRemove = [
                    'AI_ACOUSTICNESS', 'AI_ANALYZED', 'AI_BPM', 'AI_CHARACTERISTICS', 'AI_CONFIDENCE',
                    'AI_CULTURAL_CONTEXT', 'AI_DANCEABILITY', 'AI_ENERGY', 'AI_ERA', 'AI_INSTRUMENTALNESS',
                    'AI_KEY', 'AI_LIVENESS', 'AI_LOUDNESS', 'AI_MODE', 'AI_MOOD', 'AI_OCCASION',
                    'AI_SPEECHINESS', 'AI_SUBGENRES', 'AI_TIME_SIGNATURE', 'AI_VALENCE',
                    // También campos adicionales que pueden estar duplicados
                    'AI_DESCRIPTION', 'AI_MOOD_LLM', 'AI_GENRE', 'AI_SUBGENRE_LLM', 'AI_CONTEXT',
                    'AI_ENERGY_LEVEL', 'AI_DANCEABILITY_LLM', 'AI_RECOMMENDATIONS', 'AI_OCCASIONS',
                    'AI_ANALYSIS_DATE', 'AI_ANALYSIS_ENGINE',
                    // ✅ TAMBIÉN ELIMINAR GENRE estándar para actualizar con LLM
                    'GENRE'
                ];
                
                for (const fieldToRemove of aiFieldsToRemove) {
                    try {
                        const removeCommand = `metaflac --remove-tag="${fieldToRemove}" "${filePath}"`;
                        execSync(removeCommand, { stdio: 'ignore' });
                    } catch (removeError) {
                        // Ignorar errores de tags que no existen
                    }
                }
                
                // 5. ✅ SEGUNDO: Escribir los nuevos tags (ahora sin concatenación)
                for (const comment of vorbisComments) {
                    const command = `metaflac --set-tag="${comment}" "${filePath}"`;
                    execSync(command, { stdio: 'ignore' });
                }
                
                // 5. Eliminar archivo temporal si todo salió bien
                fs.unlinkSync(tempFile);
                
                console.log(`✅ ${vorbisComments.length} Vorbis comments escritos en: ${path.basename(filePath)}`);
                
                return {
                    success: true,
                    method: 'flac_direct_vorbis',
                    fieldsWritten: vorbisComments.length,
                    preservedFields: existing.preservation.shouldPreserve ? ['BPM', 'INITIALKEY'] : [],
                    message: `✅ Metadatos escritos directamente como Vorbis comments`
                };
                
            } catch (metaflacError) {
                // Restaurar archivo original si algo sale mal
                if (fs.existsSync(tempFile)) {
                    fs.copyFileSync(tempFile, filePath);
                    fs.unlinkSync(tempFile);
                }
                throw metaflacError;
            }
            
        } catch (error) {
            console.error(`❌ Error en escritura directa FLAC:`, error);
            return {
                success: false,
                error: error.message,
                method: 'flac_direct_error'
            };
        }
    }
    
    /**
     * 🏷️ Preparar metadatos LLM como Vorbis comments para FLAC  
     * @param {Object} llmMetadata - Metadatos LLM
     * @param {Object} existing - Metadatos existentes  
     * @returns {Array} Array de strings con format "FIELD=value"
     */
    prepareVorbisComments(llmMetadata, existing) {
        const comments = [];
        
        // ✅ SOLO CAMPOS AI_* - SIN DUPLICADOS (como solicitó el usuario)
        const aiFields = [
            'AI_ACOUSTICNESS', 'AI_ANALYZED', 'AI_BPM', 'AI_CHARACTERISTICS', 'AI_CONFIDENCE',
            'AI_CULTURAL_CONTEXT', 'AI_DANCEABILITY', 'AI_ENERGY', 'AI_ERA', 'AI_INSTRUMENTALNESS',
            'AI_KEY', 'AI_LIVENESS', 'AI_LOUDNESS', 'AI_MODE', 'AI_MOOD', 'AI_OCCASION',
            'AI_SPEECHINESS', 'AI_SUBGENRES', 'AI_TIME_SIGNATURE', 'AI_VALENCE'
        ];
        
        // Procesar cada campo AI_* directamente - SIN DUPLICADOS
        for (const aiField of aiFields) {
            if (llmMetadata[aiField] !== undefined && llmMetadata[aiField] !== null) {
                let value = llmMetadata[aiField];
                
                // Convertir arrays a strings
                if (Array.isArray(value)) {
                    value = value.join(', ');
                }
                
                // Convertir boolean a string
                if (typeof value === 'boolean') {
                    value = value.toString();
                }
                
                // Escapar comillas para metaflac  
                const escapedValue = String(value).replace(/"/g, '\\"');
                comments.push(`${aiField}=${escapedValue}`);
            }
        }
        
        // 🔄 CONVERTIR campos LLM_* a AI_* para evitar duplicados
        const llmToAiMapping = {
            'LLM_DESCRIPTION': 'AI_DESCRIPTION',
            'LLM_MOOD': 'AI_MOOD_LLM', 
            'LLM_GENRE': 'AI_GENRE',
            'LLM_SUBGENRE': 'AI_SUBGENRE_LLM',
            'LLM_CONTEXT': 'AI_CONTEXT',
            'LLM_OCCASIONS': 'AI_OCCASIONS',
            'LLM_ENERGY_LEVEL': 'AI_ENERGY_LEVEL',
            'LLM_DANCEABILITY': 'AI_DANCEABILITY_LLM',
            'LLM_RECOMMENDATIONS': 'AI_RECOMMENDATIONS'
        };
        
        for (const [llmField, aiFieldName] of Object.entries(llmToAiMapping)) {
            if (llmMetadata[llmField] !== undefined && llmMetadata[llmField] !== null) {
                let value = llmMetadata[llmField];
                
                // Convertir arrays a strings
                if (Array.isArray(value)) {
                    value = value.join(', ');
                }
                
                // Escapar comillas para metaflac
                const escapedValue = String(value).replace(/"/g, '\\"');
                comments.push(`${aiFieldName}=${escapedValue}`);
            }
        }
        
        // ✅ ACTUALIZAR GENRE estándar con datos del LLM
        if (llmMetadata.LLM_GENRE) {
            const escapedGenre = String(llmMetadata.LLM_GENRE).replace(/"/g, '\\"');
            comments.push(`GENRE=${escapedGenre}`);
        }
        
        // Agregar timestamp de análisis - TODO con prefijo AI_
        comments.push(`AI_ANALYSIS_DATE=${new Date().toISOString()}`);
        comments.push(`AI_ANALYSIS_ENGINE=Music Analyzer Pro v2.2`);
        
        return comments;
    }
    
    /**
     * 🚀 Escribir metadatos FLAC usando addon C++
     * @param {string} filePath - Ruta del archivo FLAC
     * @param {Object} llmMetadata - Metadatos LLM
     * @param {Object} existing - Metadatos existentes
     * @returns {Object} Resultado de escritura
     */
    async writeFLACWithCpp(filePath, llmMetadata, existing) {
        try {
            // 🛡️ Preservar metadatos profesionales si Mixed In Key está detectado
            const safeMetadata = { ...llmMetadata };
            if (existing.preservation.shouldPreserve) {
                console.log(`🛡️ Preservando datos profesionales en FLAC (C++)`);
                // No sobrescribir BPM/Key profesionales - usar campo separado
                delete safeMetadata.bpm;
                delete safeMetadata.key;
                if (llmMetadata.bpm && !safeMetadata.bmp_llm) {
                    safeMetadata.bpm_llm = llmMetadata.bpm;
                }
            }

            return new Promise((resolve, reject) => {
                // 🚀 Llamar al addon C++ para escribir metadatos FLAC
                this.cppAddon.writeMetadata(filePath, safeMetadata, (error, success) => {
                    if (error) {
                        console.error(`❌ Error C++ escribiendo FLAC:`, error);
                        reject(new Error(`Error C++ escribiendo FLAC: ${error}`));
                    } else if (success) {
                        console.log(`✅ Archivo FLAC actualizado con addon C++`);
                        
                        const llmFieldsCount = Object.keys(safeMetadata).length;
                        resolve({
                            success: true,
                            method: 'flac_cpp_direct',
                            fieldsWritten: llmFieldsCount,
                            preservedFields: existing.preservation.shouldPreserve ? ['BPM', 'INITIALKEY'] : []
                        });
                    } else {
                        console.error(`❌ Addon C++ retornó false para FLAC`);
                        reject(new Error('Addon C++ retornó false al escribir FLAC'));
                    }
                });
            });

        } catch (error) {
            console.error(`❌ Error en writeFLACWithCpp:`, error.message);
            throw new Error(`Error escribiendo FLAC con C++: ${error.message}`);
        }
    }
    
    /**
     * 🎼 Escribir metadatos a archivo FLAC usando flac-metadata (CÓDIGO LEGACY - NO USAR)
     * @param {string} filePath - Ruta del archivo FLAC
     * @param {Object} llmMetadata - Metadatos LLM
     * @param {Object} existing - Metadatos existentes
     * @returns {Object} Resultado de escritura
     */
    async writeToFLACLegacy(filePath, llmMetadata, existing) {
        try {
            console.log(`🎼 Iniciando escritura FLAC: ${path.basename(filePath)}`);

            return new Promise((resolve, reject) => {
                let vorbisComments = {};
                let foundVorbisBlock = false;
                
                // 🔄 Crear processor para leer metadatos FLAC
                const processor = new flacMetadata.Processor({ parseMetaDataBlocks: true });
                
                processor.on('preprocess', (mdb) => {
                    // 📖 Leer bloque de comentarios Vorbis existente
                    if (mdb.type === flacMetadata.data.MetaDataBlock.BlockType.VORBIS_COMMENT) {
                        foundVorbisBlock = true;
                        
                        // Preservar comentarios existentes
                        if (mdb.comments) {
                            for (const comment of mdb.comments) {
                                const [key, value] = comment.split('=');
                                if (key && value) {
                                    vorbisComments[key.toUpperCase()] = value;
                                }
                            }
                        }
                        
                        console.log(`📖 Comentarios Vorbis existentes: ${Object.keys(vorbisComments).length} campos`);
                        
                        // 🛡️ Preservar campos profesionales si Mixed In Key está detectado
                        if (existing.preservation.shouldPreserve) {
                            console.log(`🛡️ Preservando metadatos profesionales en FLAC`);
                            // Mantener BPM y Key originales (no los sobrescribir)
                            // Los campos LLM irán con sufijo _LLM
                        }

                        // 🏷️ Preparar comentarios Vorbis con metadatos LLM
                        const newComments = {
                            // 📋 Mantener metadatos básicos existentes
                            ...vorbisComments,
                            
                            // 🤖 Metadatos LLM como comentarios Vorbis personalizados
                            ...(llmMetadata.bpm_llm && { 'BPM_LLM': llmMetadata.bpm_llm.toString() }),
                            ...(llmMetadata.energy && { 'ENERGY_LLM': llmMetadata.energy.toString() }),
                            ...(llmMetadata.mood && { 'MOOD_LLM': llmMetadata.mood }),
                            ...(llmMetadata.danceability && { 'DANCEABILITY_LLM': llmMetadata.danceability.toString() }),
                            ...(llmMetadata.valence && { 'VALENCE_LLM': llmMetadata.valence.toString() }),
                            ...(llmMetadata.ai_confidence && { 'AI_CONFIDENCE': llmMetadata.ai_confidence.toString() }),
                            ...(llmMetadata.analyzed_by && { 'ANALYZED_BY': llmMetadata.analyzed_by }),
                            
                            // Campos HAMMS
                            ...(llmMetadata.hamms_vector && { 
                                'HAMMS_VECTOR': typeof llmMetadata.hamms_vector === 'string' ? 
                                               llmMetadata.hamms_vector : 
                                               JSON.stringify(llmMetadata.hamms_vector) 
                            }),
                            
                            // Metadatos adicionales de DJ
                            ...(llmMetadata.subgenre && { 'SUBGENRE_LLM': llmMetadata.subgenre }),
                            ...(llmMetadata.era && { 'ERA_LLM': llmMetadata.era }),
                            ...(llmMetadata.vocal_presence && { 'VOCAL_PRESENCE': llmMetadata.vocal_presence }),
                            ...(llmMetadata.structure && { 'STRUCTURE_LLM': llmMetadata.structure }),
                            ...(llmMetadata.drop_time && { 'DROP_TIME_LLM': llmMetadata.drop_time }),
                            ...(llmMetadata.energy_curve && { 'ENERGY_CURVE': llmMetadata.energy_curve }),
                            ...(llmMetadata.crowd_response && { 'CROWD_RESPONSE': llmMetadata.crowd_response }),
                            ...(llmMetadata.occasion && { 'OCCASION_LLM': llmMetadata.occasion }),
                            ...(llmMetadata.characteristics && { 'CHARACTERISTICS': llmMetadata.characteristics }),
                            
                            // Metadatos técnicos
                            ...(llmMetadata.tempo_stability && { 'TEMPO_STABILITY': llmMetadata.tempo_stability.toString() }),
                            ...(llmMetadata.production_quality && { 'PRODUCTION_QUALITY': llmMetadata.production_quality.toString() }),
                            ...(llmMetadata.mastering_loudness && { 'MASTERING_LOUDNESS': llmMetadata.mastering_loudness.toString() }),
                            ...(llmMetadata.dynamic_range && { 'DYNAMIC_RANGE': llmMetadata.dynamic_range.toString() }),
                            
                            // Tags personalizados
                            ...(llmMetadata.custom_tags && Array.isArray(llmMetadata.custom_tags) && { 
                                'CUSTOM_TAGS_LLM': llmMetadata.custom_tags.join(', ') 
                            }),
                            
                            // Timestamp del análisis
                            'LLM_ANALYZED_AT': new Date().toISOString(),
                            'LLM_VERSION': '1.0.0'
                        };

                        // 🔄 Crear nuevos comentarios en formato correcto
                        const commentArray = Object.entries(newComments).map(([key, value]) => `${key}=${value}`);
                        
                        // Actualizar el bloque con los nuevos comentarios
                        mdb.remove();
                        
                        // Crear nuevo bloque de comentarios Vorbis
                        const newVorbisBlock = flacMetadata.data.MetaDataBlockVorbisComment.create({
                            vendor: 'MusicAnalyzerPro',
                            comments: commentArray
                        });
                        
                        processor.push(newVorbisBlock.publish());
                        console.log(`✍️ Escribiendo ${commentArray.length} campos a FLAC`);
                    }
                });
                
                processor.on('postprocess', (mdb) => {
                    // Si no encontramos bloque Vorbis, crear uno nuevo
                    if (!foundVorbisBlock && mdb.isLast) {
                        console.log(`🆕 Creando nuevo bloque de comentarios Vorbis`);
                        
                        // Preparar comentarios LLM
                        const commentEntries = [];
                        
                        if (llmMetadata.bpm_llm) commentEntries.push(`BPM_LLM=${llmMetadata.bpm_llm}`);
                        if (llmMetadata.energy) commentEntries.push(`ENERGY_LLM=${llmMetadata.energy}`);
                        if (llmMetadata.mood) commentEntries.push(`MOOD_LLM=${llmMetadata.mood}`);
                        if (llmMetadata.analyzed_by) commentEntries.push(`ANALYZED_BY=${llmMetadata.analyzed_by}`);
                        
                        commentEntries.push(`LLM_ANALYZED_AT=${new Date().toISOString()}`);
                        commentEntries.push(`LLM_VERSION=1.0.0`);
                        
                        const newVorbisBlock = flacMetadata.data.MetaDataBlockVorbisComment.create({
                            vendor: 'MusicAnalyzerPro',
                            comments: commentEntries
                        });
                        
                        processor.push(newVorbisBlock.publish());
                        foundVorbisBlock = true;
                    }
                });
                
                processor.on('finish', (outputBuffer) => {
                    try {
                        // ✍️ Escribir archivo FLAC modificado
                        fs.writeFileSync(filePath, outputBuffer);
                        console.log(`📝 Archivo FLAC actualizado exitosamente`);

                        const llmFieldsCount = Object.keys(llmMetadata).length + 2; // +2 por timestamp y version

                        resolve({
                            success: true,
                            method: 'flac_vorbis_processor',
                            fieldsWritten: llmFieldsCount,
                            preservedFields: existing.preservation.shouldPreserve ? ['BPM', 'INITIALKEY'] : []
                        });
                    } catch (writeError) {
                        reject(new Error(`Error escribiendo archivo: ${writeError.message}`));
                    }
                });
                
                processor.on('error', (error) => {
                    reject(new Error(`Error procesando FLAC: ${error.message}`));
                });
                
                // 📖 Leer archivo FLAC y procesarlo
                const inputBuffer = fs.readFileSync(filePath);
                processor.push(inputBuffer);
            });

        } catch (error) {
            console.error(`❌ Error escribiendo FLAC:`, error.message);
            return {
                success: false,
                error: `Error escribiendo archivo FLAC: ${error.message}`,
                method: 'flac_write_failed'
            };
        }
    }

    /**
     * 🎵 Escribir metadatos a archivos MP4/M4A - Placeholder
     * @param {string} filePath - Ruta del archivo
     * @param {Object} llmMetadata - Metadatos LLM
     * @param {Object} existing - Metadatos existentes
     * @returns {Object} Resultado
     */
    async writeToMP4(filePath, llmMetadata, existing) {
        // TODO: Implementar escritura para MP4/M4A usando iTunes tags
        return {
            success: false,
            error: 'Escritura a MP4/M4A no implementada aún. Usando fallback JSON.',
            method: 'mp4_not_implemented'
        };
    }

    /**
     * 📄 Escribir metadatos a archivo JSON sidecar
     * @param {string} filePath - Archivo de audio original
     * @param {Object} llmMetadata - Metadatos LLM
     * @param {Object} existing - Metadatos existentes
     * @returns {Object} Resultado con ruta del JSON
     */
    async writeToJSON(filePath, llmMetadata, existing) {
        const jsonPath = filePath.replace(/\.[^/.]+$/, '.musicpro.json');
        
        const jsonData = {
            // 📄 Información del archivo
            originalFile: path.basename(filePath),
            filePath: filePath,
            lastModified: fs.statSync(filePath).mtime.toISOString(),
            
            // 🛡️ Información de preservación
            preservation: {
                mixedInKeyDetected: existing.preservation.shouldPreserve,
                preservedBPM: existing.preservation.existingBPM,
                preservedKey: existing.preservation.existingKey,
                source: existing.preservation.source
            },
            
            // 🎵 Metadatos originales (preservados)
            originalMetadata: {
                title: existing.metadata.title,
                artist: existing.metadata.artist,
                album: existing.metadata.album,
                genre: existing.metadata.genre,
                year: existing.metadata.year,
                bpm: existing.preservation.existingBPM,
                key: existing.preservation.existingKey
            },
            
            // 🤖 Metadatos LLM (nuevos)
            llmMetadata: {
                ...llmMetadata,
                analyzedAt: new Date().toISOString(),
                version: '1.0.0',
                
                // 📊 Vector HAMMS (si existe)
                ...(llmMetadata.hamms_vector && {
                    hamms_vector: typeof llmMetadata.hamms_vector === 'string' ? 
                        JSON.parse(llmMetadata.hamms_vector) : llmMetadata.hamms_vector
                })
            },
            
            // 🔗 Compatibilidad con software DJ
            djCompatibility: {
                format: 'MusicAnalyzerPro',
                version: '1.0.0',
                mixedInKeySafe: existing.preservation.shouldPreserve,
                exportable: true
            }
        };
        
        // 💾 Escribir archivo JSON
        fs.writeFileSync(jsonPath, JSON.stringify(jsonData, null, 2), 'utf8');
        
        return { jsonPath, success: true };
    }

    /**
     * 🔍 Verificar si se puede escribir a formato específico
     * @param {string} filePath - Ruta del archivo
     * @returns {boolean} Si se puede escribir
     */
    canWriteCustomFields(filePath) {
        const ext = path.extname(filePath).toLowerCase();
        return this.supportedFormats.includes(ext);
    }

    /**
     * 📊 Obtener estadísticas de metadatos
     * @param {string} folderPath - Carpeta a analizar
     * @returns {Object} Estadísticas
     */
    async getMetadataStats(folderPath) {
        const files = fs.readdirSync(folderPath)
            .filter(file => this.supportedFormats.includes(path.extname(file).toLowerCase()))
            .map(file => path.join(folderPath, file));
        
        let mixedInKeyCount = 0;
        let llmAnalyzedCount = 0;
        let totalFiles = files.length;
        
        for (const file of files) {
            try {
                const existing = await this.readExistingMetadata(file);
                if (existing.preservation.shouldPreserve) mixedInKeyCount++;
                if (Object.keys(existing.llmMetadata).length > 0) llmAnalyzedCount++;
            } catch (error) {
                console.warn(`Error analyzing ${file}:`, error.message);
            }
        }
        
        return {
            totalFiles,
            mixedInKeyFiles: mixedInKeyCount,
            llmAnalyzedFiles: llmAnalyzedCount,
            needsAnalysis: totalFiles - llmAnalyzedCount,
            preservationRate: totalFiles > 0 ? (mixedInKeyCount / totalFiles * 100).toFixed(1) : 0
        };
    }
}

module.exports = MetadataWriter;