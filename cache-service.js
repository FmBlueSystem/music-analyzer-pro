const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

class CacheService {
    constructor() {
        this.memoryCache = new Map(); // Cache en memoria para acceso ultra-rápido
        this.folderCache = new Map(); // Cache de carpetas
        this.searchCache = new Map(); // Cache de búsquedas
        this.maxMemoryItems = 5000; // Máximo 5K archivos en memoria
        this.maxSearchCache = 100; // Máximo 100 búsquedas en cache
        this.cacheHits = 0;
        this.cacheMisses = 0;
    }

    /**
     * 🔑 Generar clave de cache basada en ruta de archivo
     */
    generateFileKey(filePath) {
        return crypto.createHash('md5').update(filePath).digest('hex');
    }

    /**
     * 🔍 Generar clave de búsqueda
     */
    generateSearchKey(searchParams) {
        const normalized = JSON.stringify(searchParams, Object.keys(searchParams).sort());
        return crypto.createHash('md5').update(normalized).digest('hex');
    }

    /**
     * 💾 Agregar archivo al cache de memoria
     */
    setFile(filePath, fileData) {
        const key = this.generateFileKey(filePath);
        
        // Si el cache está lleno, eliminar el elemento más antiguo (LRU)
        if (this.memoryCache.size >= this.maxMemoryItems) {
            const firstKey = this.memoryCache.keys().next().value;
            this.memoryCache.delete(firstKey);
        }
        
        // Agregar timestamp para LRU
        const cacheEntry = {
            ...fileData,
            cachedAt: Date.now(),
            accessCount: 1
        };
        
        this.memoryCache.set(key, cacheEntry);
        console.log(`📦 Cache: Archivo agregado ${path.basename(filePath)} (${this.memoryCache.size}/${this.maxMemoryItems})`);
    }

    /**
     * 📖 Obtener archivo del cache
     */
    getFile(filePath) {
        const key = this.generateFileKey(filePath);
        const cached = this.memoryCache.get(key);
        
        if (cached) {
            // Actualizar estadísticas de acceso
            cached.accessCount++;
            cached.lastAccessed = Date.now();
            
            // Mover al final (LRU)
            this.memoryCache.delete(key);
            this.memoryCache.set(key, cached);
            
            this.cacheHits++;
            return cached;
        }
        
        this.cacheMisses++;
        return null;
    }

    /**
     * 📁 Cache de carpeta completa
     */
    setFolder(folderPath, files) {
        const folderKey = this.generateFileKey(folderPath);
        
        const folderData = {
            files,
            totalFiles: files.length,
            analyzedFiles: files.filter(f => f.analysis_status === 'completed').length,
            mixedInKeyFiles: files.filter(f => f.mixed_in_key_detected).length,
            cachedAt: Date.now(),
            lastModified: this.getFolderLastModified(folderPath)
        };
        
        this.folderCache.set(folderKey, folderData);
        
        // También agregar archivos individuales al cache
        files.forEach(file => {
            this.setFile(file.file_path, file);
        });
        
        console.log(`📁 Cache: Carpeta cacheada ${path.basename(folderPath)} (${files.length} archivos)`);
    }

    /**
     * 📂 Obtener carpeta del cache
     */
    getFolder(folderPath) {
        const folderKey = this.generateFileKey(folderPath);
        const cached = this.folderCache.get(folderKey);
        
        if (cached) {
            // Verificar si la carpeta ha sido modificada
            const currentModified = this.getFolderLastModified(folderPath);
            if (currentModified > cached.lastModified) {
                // Cache invalidado
                this.folderCache.delete(folderKey);
                return null;
            }
            
            this.cacheHits++;
            return cached;
        }
        
        this.cacheMisses++;
        return null;
    }

    /**
     * 🕒 Obtener última modificación de carpeta
     */
    getFolderLastModified(folderPath) {
        try {
            if (!fs.existsSync(folderPath)) return 0;
            
            const stats = fs.statSync(folderPath);
            return stats.mtime.getTime();
        } catch (error) {
            return 0;
        }
    }

    /**
     * 🔍 Cache de resultados de búsqueda
     */
    setSearchResults(searchParams, results) {
        const searchKey = this.generateSearchKey(searchParams);
        
        // Limpiar cache de búsqueda si está lleno
        if (this.searchCache.size >= this.maxSearchCache) {
            const oldestKey = this.searchCache.keys().next().value;
            this.searchCache.delete(oldestKey);
        }
        
        const searchData = {
            results,
            resultCount: results.length,
            cachedAt: Date.now(),
            searchParams: { ...searchParams }
        };
        
        this.searchCache.set(searchKey, searchData);
        console.log(`🔍 Cache: Búsqueda cacheada (${results.length} resultados)`);
    }

    /**
     * 🎯 Obtener resultados de búsqueda del cache
     */
    getSearchResults(searchParams) {
        const searchKey = this.generateSearchKey(searchParams);
        const cached = this.searchCache.get(searchKey);
        
        if (cached) {
            // Cache de búsqueda válido por 5 minutos
            const cacheAge = Date.now() - cached.cachedAt;
            if (cacheAge < 5 * 60 * 1000) { // 5 minutos
                this.cacheHits++;
                return cached.results;
            } else {
                // Cache expirado
                this.searchCache.delete(searchKey);
            }
        }
        
        this.cacheMisses++;
        return null;
    }

    /**
     * 🔄 Invalidar cache de archivo específico
     */
    invalidateFile(filePath) {
        const key = this.generateFileKey(filePath);
        const removed = this.memoryCache.delete(key);
        
        if (removed) {
            console.log(`🗑️ Cache: Archivo invalidado ${path.basename(filePath)}`);
        }
        
        return removed;
    }

    /**
     * 📁 Invalidar cache de carpeta
     */
    invalidateFolder(folderPath) {
        const folderKey = this.generateFileKey(folderPath);
        this.folderCache.delete(folderKey);
        
        // También invalidar archivos de esa carpeta
        for (const [key, data] of this.memoryCache.entries()) {
            if (data.folder_path === folderPath) {
                this.memoryCache.delete(key);
            }
        }
        
        console.log(`🗑️ Cache: Carpeta invalidada ${path.basename(folderPath)}`);
    }

    /**
     * 🧹 Limpiar cache de búsquedas
     */
    clearSearchCache() {
        const count = this.searchCache.size;
        this.searchCache.clear();
        console.log(`🧹 Cache: ${count} búsquedas eliminadas del cache`);
    }

    /**
     * 🚀 Cache inteligente para carga masiva
     */
    async smartCache(database, folderPath, batchSize = 1000) {
        console.log(`🚀 Iniciando cache inteligente para: ${folderPath}`);
        
        // Verificar si ya está en cache y es válido
        const cachedFolder = this.getFolder(folderPath);
        if (cachedFolder) {
            console.log(`⚡ Cache hit: Carpeta ya cacheada (${cachedFolder.totalFiles} archivos)`);
            return cachedFolder.files;
        }
        
        try {
            let allFiles = [];
            let offset = 0;
            let batch;
            
            // Cargar en lotes para evitar bloquear la UI
            do {
                batch = await database.getFilesFromFolder(folderPath, batchSize, offset);
                allFiles = allFiles.concat(batch);
                offset += batchSize;
                
                console.log(`📦 Cargando lote: ${offset}/${allFiles.length} archivos...`);
                
                // Pequeña pausa para no bloquear el event loop
                await new Promise(resolve => setImmediate(resolve));
                
            } while (batch.length === batchSize);
            
            // Cachear la carpeta completa
            this.setFolder(folderPath, allFiles);
            
            console.log(`✅ Cache completo: ${allFiles.length} archivos cacheados`);
            return allFiles;
            
        } catch (error) {
            console.error('❌ Error en cache inteligente:', error);
            throw error;
        }
    }

    /**
     * 📊 Obtener estadísticas del cache
     */
    getStats() {
        const totalRequests = this.cacheHits + this.cacheMisses;
        const hitRate = totalRequests > 0 ? ((this.cacheHits / totalRequests) * 100).toFixed(1) : 0;
        
        // Calcular uso de memoria estimado
        const avgEntrySize = 2048; // Estimado 2KB por entrada
        const memoryUsageKB = (this.memoryCache.size * avgEntrySize) / 1024;
        
        return {
            memoryCache: {
                size: this.memoryCache.size,
                maxSize: this.maxMemoryItems,
                usagePercent: ((this.memoryCache.size / this.maxMemoryItems) * 100).toFixed(1)
            },
            folderCache: {
                size: this.folderCache.size
            },
            searchCache: {
                size: this.searchCache.size,
                maxSize: this.maxSearchCache
            },
            performance: {
                hits: this.cacheHits,
                misses: this.cacheMisses,
                hitRate: `${hitRate}%`,
                totalRequests
            },
            memory: {
                estimatedUsageKB: Math.round(memoryUsageKB),
                estimatedUsageMB: (memoryUsageKB / 1024).toFixed(1)
            }
        };
    }

    /**
     * 🧮 Optimizar cache (eliminar entradas menos usadas)
     */
    optimize() {
        console.log('🧮 Optimizando cache...');
        
        // Convertir a array para poder ordenar
        const entries = Array.from(this.memoryCache.entries());
        
        // Ordenar por frecuencia de acceso y tiempo
        entries.sort((a, b) => {
            const scoreA = (a[1].accessCount || 1) * (1 / (Date.now() - (a[1].lastAccessed || a[1].cachedAt)));
            const scoreB = (b[1].accessCount || 1) * (1 / (Date.now() - (b[1].lastAccessed || b[1].cachedAt)));
            return scoreB - scoreA;
        });
        
        // Mantener solo los más utilizados (75% del tamaño máximo)
        const keepSize = Math.floor(this.maxMemoryItems * 0.75);
        const toKeep = entries.slice(0, keepSize);
        
        // Reconstruir cache
        this.memoryCache.clear();
        toKeep.forEach(([key, value]) => {
            this.memoryCache.set(key, value);
        });
        
        const removed = entries.length - toKeep.length;
        console.log(`✅ Cache optimizado: ${removed} entradas eliminadas, ${toKeep.length} mantenidas`);
        
        return {
            removed,
            kept: toKeep.length,
            currentSize: this.memoryCache.size
        };
    }

    /**
     * 🗑️ Limpiar todo el cache
     */
    clear() {
        const stats = this.getStats();
        
        this.memoryCache.clear();
        this.folderCache.clear();
        this.searchCache.clear();
        
        // Resetear estadísticas
        this.cacheHits = 0;
        this.cacheMisses = 0;
        
        console.log('🗑️ Cache completamente limpiado');
        return stats;
    }

    /**
     * 💡 Sugerir archivos relacionados basado en cache
     */
    getSimilarFiles(filePath, limit = 10) {
        const sourceFile = this.getFile(filePath);
        if (!sourceFile) return [];
        
        const similar = [];
        
        // Buscar archivos similares en cache
        for (const [key, file] of this.memoryCache.entries()) {
            if (file.file_path === filePath) continue;
            
            let score = 0;
            
            // Similarity scoring
            if (file.artist === sourceFile.artist) score += 3;
            if (file.genre === sourceFile.genre) score += 2;
            if (file.album === sourceFile.album) score += 1;
            
            // BPM similarity (±5 BPM)
            if (file.bpm_llm && sourceFile.bpm_llm) {
                const bpmDiff = Math.abs(file.bpm_llm - sourceFile.bpm_llm);
                if (bpmDiff <= 5) score += 2;
            }
            
            // Energy similarity
            if (file.energy && sourceFile.energy) {
                const energyDiff = Math.abs(file.energy - sourceFile.energy);
                if (energyDiff <= 1) score += 1;
            }
            
            if (score > 0) {
                similar.push({ file, score });
            }
        }
        
        // Ordenar por score y devolver top results
        return similar
            .sort((a, b) => b.score - a.score)
            .slice(0, limit)
            .map(item => item.file);
    }
}

module.exports = CacheService;