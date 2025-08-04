const { ipcRenderer } = require('electron');

console.log('🚀 Renderer.js loading...');

// Variables globales básicas
let audioFiles = [];
let filteredFiles = [];
let currentPage = 1;
const filesPerPage = 100; // Mostrar 100 archivos por página
let viewMode = 'cards'; // 'cards' o 'table'

// Lista de campos AI para análisis
const AI_FIELDS = [
    'AI_ACOUSTICNESS', 'AI_BPM', 'AI_DANCEABILITY', 'AI_ENERGY', 
    'AI_INSTRUMENTALNESS', 'AI_KEY', 'AI_LIVENESS', 'AI_LOUDNESS', 
    'AI_MODE', 'AI_MOOD', 'AI_SPEECHINESS', 'AI_TIME_SIGNATURE', 
    'AI_VALENCE', 'AI_CHARACTERISTICS', 'AI_CULTURAL_CONTEXT', 
    'AI_ERA', 'AI_OCCASION', 'AI_SUBGENRES', 'AI_CONFIDENCE'
];

// 🔍 Función para filtrar archivos
window.filterFiles = function() {
    const searchTerm = document.getElementById('searchInput').value.toLowerCase();
    const statusFilter = document.getElementById('filterStatus').value;
    const algorithmFilter = document.getElementById('filterAlgorithm').value;
    
    // Aplicar filtros
    filteredFiles = audioFiles.filter(file => {
        // Filtro de búsqueda
        if (searchTerm) {
            const searchableText = [
                file.file_name,
                file.artist,
                file.genre,
                file.AI_MOOD,
                file.existing_bpm?.toString(),
                file.AI_BPM?.toString()
            ].filter(Boolean).join(' ').toLowerCase();
            
            if (!searchableText.includes(searchTerm)) {
                return false;
            }
        }
        
        // Filtro de estado
        const analyzedCount = getAnalyzedFieldsCount(file);
        
        if (statusFilter === 'pending' && analyzedCount > 0) return false;
        if (statusFilter === 'partial' && (analyzedCount === 0 || analyzedCount === 19)) return false;
        if (statusFilter === 'complete' && analyzedCount !== 19) return false;
        
        // Filtro de algoritmo específico
        if (algorithmFilter !== 'all') {
            switch (algorithmFilter) {
                case 'missing-bpm':
                    if (file.AI_BPM) return false;
                    break;
                case 'missing-key':
                    if (file.AI_KEY) return false;
                    break;
                case 'missing-energy':
                    if (file.AI_ENERGY) return false;
                    break;
                case 'missing-mood':
                    if (file.AI_MOOD) return false;
                    break;
                case 'missing-danceability':
                    if (file.AI_DANCEABILITY) return false;
                    break;
            }
        }
        
        return true;
    });
    
    // Reset página y renderizar
    currentPage = 1;
    updateFileStats();
    
    if (viewMode === 'cards') {
        renderCardsView();
    } else {
        renderBasicTable();
    }
}

// 📊 Función para contar campos analizados
function getAnalyzedFieldsCount(file) {
    let count = 0;
    AI_FIELDS.forEach(field => {
        if (file[field] !== null && file[field] !== undefined && file[field] !== '') {
            count++;
        }
    });
    return count;
}

// 📊 Función para actualizar estadísticas
function updateFileStats() {
    const files = filteredFiles.length > 0 ? filteredFiles : audioFiles;
    
    let pendingCount = 0;
    let partialCount = 0;
    let completeCount = 0;
    
    files.forEach(file => {
        const analyzedCount = getAnalyzedFieldsCount(file);
        if (analyzedCount === 0) {
            pendingCount++;
        } else if (analyzedCount === 19) {
            completeCount++;
        } else {
            partialCount++;
        }
    });
    
    // Actualizar badges
    document.getElementById('fileCount').textContent = `${files.length} archivos`;
    document.getElementById('pendingCount').textContent = `${pendingCount} pendientes`;
    document.getElementById('partialCount').textContent = `${partialCount} parciales`;
    document.getElementById('completeCount').textContent = `${completeCount} completos`;
}

// 🔄 Función de navegación de pestañas SIMPLE
window.switchTab = function(targetTab) {
    console.log('🔄 switchTab called:', targetTab);
    
    try {
        // Buscar elementos
        const tabBtns = document.querySelectorAll('.tab-btn');
        const tabContents = document.querySelectorAll('.tab-content');
        
        console.log('Found - Buttons:', tabBtns.length, 'Contents:', tabContents.length);
        
        // Update buttons
        tabBtns.forEach(btn => {
            if (btn.dataset.tab === targetTab) {
                btn.classList.add('active');
                console.log('✅ Activated button:', btn.textContent);
            } else {
                btn.classList.remove('active');
            }
        });
        
        // Update contents
        tabContents.forEach(content => {
            if (content.id === `${targetTab}-tab`) {
                content.classList.add('active');
                content.style.display = 'block';
                console.log('✅ Activated content:', content.id);
            } else {
                content.classList.remove('active');
                content.style.display = 'none';
            }
        });
        
        console.log('✅ Tab switched successfully to:', targetTab);
        
    } catch (error) {
        console.error('❌ Error in switchTab:', error);
    }
};

// Inicialización simple
document.addEventListener('DOMContentLoaded', () => {
    console.log('📱 DOM Content Loaded');
    
    // 📁 Event listener para botón de seleccionar carpeta
    const folderBtn = document.getElementById('folderBtn');
    if (folderBtn) {
        folderBtn.addEventListener('click', async () => {
            console.log('📁 Seleccionar carpeta clicked');
            try {
                const folderPath = await ipcRenderer.invoke('select-folder');
                if (folderPath) {
                    console.log('📁 Carpeta seleccionada:', folderPath);
                    document.getElementById('status').textContent = 'Escaneando carpeta...';
                    
                    // Escanear carpeta
                    const files = await ipcRenderer.invoke('load-audio-files', folderPath);
                    console.log('📁 Archivos cargados:', files?.length || 0);
                    
                    if (files && files.length > 0) {
                        document.getElementById('status').textContent = `✅ ${files.length} archivos encontrados`;
                        // Actualizar la lista global y renderizar
                        audioFiles = files;
                        renderBasicTable();
                    } else {
                        document.getElementById('status').textContent = '❌ No se encontraron archivos de audio';
                    }
                }
            } catch (error) {
                console.error('❌ Error seleccionando carpeta:', error);
                document.getElementById('status').textContent = '❌ Error seleccionando carpeta';
            }
        });
        console.log('✅ Event listener agregado al botón de carpeta');
    } else {
        console.warn('⚠️ Botón folderBtn no encontrado');
    }
    
    // 🧠 Event listener para botón de análisis LLM masivo
    const analyzeLLMBtn = document.getElementById('analyzeLLMBtn');
    if (analyzeLLMBtn) {
        analyzeLLMBtn.addEventListener('click', async () => {
            console.log('🧠 Análisis LLM masivo iniciado');
            try {
                const confirmation = confirm('¿Analizar TODOS los archivos con LLM? Esto puede tomar mucho tiempo.');
                if (confirmation) {
                    await runBatchLLMAnalysis();
                }
            } catch (error) {
                console.error('❌ Error en análisis LLM masivo:', error);
                document.getElementById('status').textContent = '❌ Error en análisis masivo';
            }
        });
        console.log('✅ Event listener agregado al botón de análisis LLM');
    } else {
        console.warn('⚠️ Botón analyzeLLMBtn no encontrado');
    }
    
    // Cargar archivos existentes de la base de datos
    loadExistingFiles();
});

// Función simple para cargar archivos
async function loadExistingFiles() {
    try {
        console.log('📁 Loading existing files...');
        const files = await ipcRenderer.invoke('get-existing-files', 10000); // Cargar hasta 10K archivos
        audioFiles = files || [];
        console.log(`📁 Loaded ${audioFiles.length} files from database`);
        
        // Renderizar tabla básica si estamos en library tab
        renderBasicTable();
        
    } catch (error) {
        console.error('❌ Error loading files:', error);
    }
}

// Renderizar tabla básica
// 🎴 Función para renderizar vista de tarjetas modernas
function renderCardsView() {
    const container = document.getElementById('filesContainer');
    if (!container) {
        console.log('❌ Files container not found');
        return;
    }
    
    const files = filteredFiles.length > 0 ? filteredFiles : audioFiles;
    
    if (files.length === 0) {
        container.innerHTML = '<div class="no-files-message">No hay archivos para mostrar</div>';
        return;
    }
    
    console.log('🎴 Rendering modern cards view...');
    
    // Calcular rango de archivos para la página actual
    const startIndex = (currentPage - 1) * filesPerPage;
    const endIndex = startIndex + filesPerPage;
    const filesToShow = files.slice(startIndex, endIndex);
    
    // Crear contenedor de tarjetas
    const cardsHTML = filesToShow.map((file, index) => {
        const globalIndex = startIndex + index;
        
        // Determinar icono según tipo de archivo
        const extension = file.file_name?.split('.').pop()?.toLowerCase() || '';
        const fileIcon = {
            'mp3': '🎵',
            'flac': '🎼',
            'wav': '🎙️',
            'm4a': '🎧',
            'aac': '🎶'
        }[extension] || '🎵';
        
        // Calcular progreso de análisis
        const analyzedCount = getAnalyzedFieldsCount(file);
        
        const progressPercentage = Math.round((analyzedCount / 19) * 100);
        const progressStatus = analyzedCount === 0 ? 'pending' : analyzedCount === 19 ? 'completed' : 'partial';
        
        // Formatear metadatos
        const bpm = file.existing_bpm || file.AI_BPM || '-';
        const key = file.key || file.AI_KEY || '-';
        const energy = file.AI_ENERGY ? `${Math.round(file.AI_ENERGY * 100)}%` : '-';
        const mood = file.AI_MOOD || '-';
        
        return `
            <div class="music-card" data-file-path="${file.file_path}">
                <div class="card-header">
                    <div class="card-title">
                        <h3><span class="file-type-icon">${fileIcon}</span> ${file.file_name || 'Unknown'}</h3>
                        <p>${file.artist || 'Unknown Artist'}</p>
                    </div>
                    <span class="status-indicator ${progressStatus}">
                        ${progressStatus === 'pending' ? 'Sin analizar' : 
                          progressStatus === 'completed' ? 'Completo' : 
                          'En progreso'}
                    </span>
                </div>
                
                <div class="card-metadata">
                    <div class="metadata-item">
                        <div class="metadata-label">BPM</div>
                        <div class="metadata-value bpm">${bpm}</div>
                    </div>
                    <div class="metadata-item">
                        <div class="metadata-label">Key</div>
                        <div class="metadata-value key">${key}</div>
                    </div>
                    <div class="metadata-item">
                        <div class="metadata-label">Energy</div>
                        <div class="metadata-value">${energy}</div>
                    </div>
                    <div class="metadata-item">
                        <div class="metadata-label">Mood</div>
                        <div class="metadata-value">${mood}</div>
                    </div>
                </div>
                
                <div class="progress-section">
                    <div class="progress-header">
                        <span class="progress-label">Análisis AI</span>
                        <span class="progress-value">${analyzedCount}/19</span>
                    </div>
                    <div class="progress-bar-modern">
                        <div class="progress-fill-modern" style="width: ${progressPercentage}%"></div>
                    </div>
                </div>
                
                <div class="card-actions">
                    <button class="btn-card btn-analyze-card" onclick="analyzeFile('${file.file_path}')">
                        🤖 Analizar
                    </button>
                    <button class="btn-card btn-details-card" onclick="showFileDetails('${file.file_path}')">
                        📊 Detalles
                    </button>
                </div>
            </div>
        `;
    }).join('');
    
    container.innerHTML = `<div class="cards-container">${cardsHTML}</div>`;
    
    // Agregar controles de paginación
    renderPaginationControls();
    
    console.log(`✅ Cards view rendered - Página ${currentPage}, mostrando ${filesToShow.length} de ${files.length} archivos`);
}

// 📊 Función para cambiar entre vista de tarjetas y tabla
window.toggleViewMode = function() {
    viewMode = viewMode === 'cards' ? 'table' : 'cards';
    console.log('🔄 Switching to view mode:', viewMode);
    
    // Actualizar botones de vista
    document.querySelectorAll('.btn-view-mode').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.view === viewMode);
    });
    
    // Renderizar según el modo
    if (viewMode === 'cards') {
        renderCardsView();
    } else {
        renderBasicTable();
    }
};

function renderBasicTable() {
    const tableBody = document.getElementById('filesTableBody');
    if (!tableBody) {
        console.log('❌ Table body not found (filesTableBody)');
        return;
    }
    
    const files = filteredFiles.length > 0 ? filteredFiles : audioFiles;
    
    if (files.length === 0) {
        console.log('⚠️ No files to render');
        tableBody.innerHTML = '<tr><td colspan="10" class="text-center">No hay archivos para mostrar</td></tr>';
        return;
    }
    
    console.log('🔄 Rendering basic table...');
    
    tableBody.innerHTML = '';
    
    // Calcular rango de archivos para la página actual
    const startIndex = (currentPage - 1) * filesPerPage;
    const endIndex = startIndex + filesPerPage;
    const filesToShow = files.slice(startIndex, endIndex);
    
    filesToShow.forEach((file, index) => {
        const globalIndex = startIndex + index;
        const row = document.createElement('tr');
        
        // Formatear tamaño de archivo
        const fileSize = file.file_size ? (file.file_size / (1024 * 1024)).toFixed(1) + ' MB' : '-';
        
        // Calcular progreso de análisis (simplificado por ahora)
        const progress = file.LLM_ANALYZED ? '1/19' : '0/19';
        const progressClass = file.LLM_ANALYZED ? 'progress-partial' : 'progress-none';
        
        row.innerHTML = `
            <td class="col-index">${globalIndex + 1}</td>
            <td class="col-name" title="${file.file_path}">
                🎵 ${file.file_name || 'Unknown'}
            </td>
            <td class="col-size">${fileSize}</td>
            <td class="col-progress">
                <span class="progress-indicator ${progressClass}">${progress}</span>
            </td>
            <td class="col-bpm">${file.existing_bmp || '-'}</td>
            <td class="col-energy">${file.AI_ENERGY || '-'}</td>
            <td class="col-mood">${file.AI_MOOD || file.LLM_MOOD || '-'}</td>
            <td class="col-genre">${file.genre || file.LLM_GENRE || '-'}</td>
            <td class="col-subgenre">${file.LLM_SUBGENRE || '-'}</td>
            <td class="col-actions">
                <button class="btn-action" onclick="analyzeFile('${file.file_path}')" title="Analizar con LLM">
                    🤖
                </button>
            </td>
        `;
        tableBody.appendChild(row);
    });
    
    // Agregar controles de paginación
    renderPaginationControls();
    
    console.log(`✅ Basic table rendered - Página ${currentPage}, mostrando ${filesToShow.length} de ${files.length} archivos`);
}

// 📄 Función para renderizar controles de paginación
function renderPaginationControls() {
    // Buscar o crear contenedor de paginación
    let paginationContainer = document.getElementById('paginationControls');
    if (!paginationContainer) {
        paginationContainer = document.createElement('div');
        paginationContainer.id = 'paginationControls';
        paginationContainer.className = 'pagination-container';
        
        // Insertar después de la tabla
        const tableContainer = document.querySelector('.table-container-compact');
        if (tableContainer) {
            tableContainer.appendChild(paginationContainer);
        }
    }
    
    const totalPages = Math.ceil(audioFiles.length / filesPerPage);
    
    if (totalPages <= 1) {
        paginationContainer.innerHTML = '';
        return;
    }
    
    paginationContainer.innerHTML = `
        <div class="pagination-info">
            Mostrando ${((currentPage - 1) * filesPerPage) + 1}-${Math.min(currentPage * filesPerPage, audioFiles.length)} de ${audioFiles.length} archivos
        </div>
        <div class="pagination-buttons">
            <button class="btn-pagination" onclick="changePage(1)" ${currentPage === 1 ? 'disabled' : ''}>⏮️ Primera</button>
            <button class="btn-pagination" onclick="changePage(${currentPage - 1})" ${currentPage === 1 ? 'disabled' : ''}>⬅️ Anterior</button>
            <span class="page-info">Página ${currentPage} de ${totalPages}</span>
            <button class="btn-pagination" onclick="changePage(${currentPage + 1})" ${currentPage === totalPages ? 'disabled' : ''}>➡️ Siguiente</button>
            <button class="btn-pagination" onclick="changePage(${totalPages})" ${currentPage === totalPages ? 'disabled' : ''}>⏭️ Última</button>
        </div>
    `;
}

// 📁 Función simple para cargar archivos
window.loadFilesSimple = async function() {
    console.log('🎯 loadFilesSimple called');
    
    try {
        const result = await ipcRenderer.invoke('load-all-files');
        console.log('📁 IPC result:', result);
        
        if (result.success) {
            audioFiles = result.files || [];
            filteredFiles = []; // Reset filtros
            console.log(`✅ Files loaded: ${audioFiles.length}`);
            
            // Actualizar estadísticas
            updateFileStats();
            
            // Renderizar según el modo actual
            if (viewMode === 'cards') {
                renderCardsView();
            } else {
                renderBasicTable();
            }
            
            // Actualizar status
            const statusEl = document.getElementById('status');
            if (statusEl) {
                statusEl.textContent = `${audioFiles.length} archivos cargados`;
            }
        } else {
            console.error('❌ Error:', result.error);
            alert('Error al cargar archivos: ' + result.error);
        }
    } catch (error) {
        console.error('❌ loadFilesSimple error:', error);
        alert('Error al cargar archivos');
    }
}

// 📄 Función para cambiar de página
window.changePage = function(newPage) {
    const totalPages = Math.ceil(audioFiles.length / filesPerPage);
    
    if (newPage < 1 || newPage > totalPages) {
        console.warn(`⚠️ Página ${newPage} fuera de rango (1-${totalPages})`);
        return;
    }
    
    currentPage = newPage;
    console.log(`📄 Cambiando a página ${currentPage}`);
    
    // Renderizar según el modo actual
    if (viewMode === 'cards') {
        renderCardsView();
    } else {
        renderBasicTable();
    }
}

// 📊 Función para mostrar detalles del archivo
window.showFileDetails = function(filePath) {
    const file = audioFiles.find(f => f.file_path === filePath);
    if (!file) {
        console.error('❌ Archivo no encontrado:', filePath);
        return;
    }
    
    // Mostrar modal
    const modal = document.getElementById('fileDetailsModal');
    const modalFileName = document.getElementById('modalFileName');
    const modalBody = document.getElementById('modalBody');
    const analyzeBtn = document.getElementById('analyzeSelectedBtn');
    
    modalFileName.textContent = file.file_name || 'Archivo Desconocido';
    
    // Analizar estado de cada algoritmo
    const algorithmStatus = AI_FIELDS.map(field => {
        const hasValue = file[field] !== null && file[field] !== undefined && file[field] !== '';
        return {
            name: field.replace('AI_', '').replace(/_/g, ' '),
            field: field,
            status: hasValue ? 'completed' : 'pending',
            value: hasValue ? file[field] : null
        };
    });
    
    const completedCount = algorithmStatus.filter(a => a.status === 'completed').length;
    const pendingAlgorithms = algorithmStatus.filter(a => a.status === 'pending');
    
    // Construir HTML del modal
    let html = `
        <div class="file-details-info">
            <h3>📊 Información General</h3>
            <div class="info-grid">
                <div class="info-item">
                    <span class="info-label">Archivo:</span>
                    <span class="info-value">${file.file_name}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Ruta:</span>
                    <span class="info-value" style="font-size: 11px;">${file.file_path}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Tamaño:</span>
                    <span class="info-value">${file.file_size ? (file.file_size / (1024 * 1024)).toFixed(1) + ' MB' : '-'}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">BPM Original:</span>
                    <span class="info-value">${file.existing_bpm || '-'}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Key Original:</span>
                    <span class="info-value">${file.key || '-'}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Género:</span>
                    <span class="info-value">${file.genre || '-'}</span>
                </div>
            </div>
            
            <h3 style="margin-top: 24px;">🤖 Estado de Análisis AI (${completedCount}/19)</h3>
            <div class="progress-bar-modern" style="margin-bottom: 16px;">
                <div class="progress-fill-modern" style="width: ${(completedCount / 19) * 100}%"></div>
            </div>
            
            ${pendingAlgorithms.length > 0 ? `
                <h4 style="color: var(--bg-warning); margin-bottom: 12px;">⚠️ Algoritmos Pendientes (${pendingAlgorithms.length})</h4>
                <div class="algorithm-grid">
                    ${pendingAlgorithms.map(algo => `
                        <div class="algorithm-item">
                            <span class="algorithm-name">${algo.name}</span>
                            <span class="algorithm-status pending">Pendiente</span>
                        </div>
                    `).join('')}
                </div>
            ` : '<p style="color: var(--bg-success);">✅ Todos los algoritmos completados</p>'}
            
            ${completedCount > 0 ? `
                <h4 style="color: var(--bg-success); margin-top: 20px; margin-bottom: 12px;">✅ Algoritmos Completados (${completedCount})</h4>
                <div class="algorithm-grid">
                    ${algorithmStatus.filter(a => a.status === 'completed').map(algo => `
                        <div class="algorithm-item">
                            <span class="algorithm-name">${algo.name}</span>
                            <span class="algorithm-status completed">${
                                typeof algo.value === 'object' ? 'Datos' : algo.value
                            }</span>
                        </div>
                    `).join('')}
                </div>
            ` : ''}
        </div>
    `;
    
    modalBody.innerHTML = html;
    
    // Configurar botón de análisis
    if (pendingAlgorithms.length > 0) {
        analyzeBtn.textContent = `🤖 Analizar ${pendingAlgorithms.length} Algoritmos Faltantes`;
        analyzeBtn.style.display = 'block';
        analyzeBtn.onclick = async () => {
            analyzeBtn.disabled = true;
            analyzeBtn.textContent = '⏳ Analizando...';
            
            // Analizar solo los algoritmos faltantes
            const pendingFields = pendingAlgorithms.map(a => a.field);
            await analyzeFileWithAlgorithms(file.file_path, pendingFields);
            
            // Cerrar modal y recargar vista
            closeFileDetails();
            if (viewMode === 'cards') {
                renderCardsView();
            } else {
                renderBasicTable();
            }
        };
    } else {
        analyzeBtn.style.display = 'none';
    }
    
    modal.style.display = 'flex';
}

// 🔲 Función para cerrar modal de detalles
window.closeFileDetails = function() {
    const modal = document.getElementById('fileDetailsModal');
    modal.style.display = 'none';
}

// 🤖 Función para analizar archivo con algoritmos específicos
window.analyzeFileWithAlgorithms = async function(filePath, algorithms) {
    try {
        console.log('🤖 Analizando archivo con algoritmos específicos:', algorithms);
        
        const result = await ipcRenderer.invoke('analyze-file-with-algorithms', filePath, algorithms);
        
        if (result.success) {
            console.log('✅ Análisis completado exitosamente');
            
            // Recargar archivos para obtener datos actualizados
            await loadFilesSimple();
            
            // Actualizar status
            const statusEl = document.getElementById('status');
            if (statusEl) {
                statusEl.textContent = `✅ Análisis completado: ${algorithms.length} algoritmos procesados`;
            }
        } else {
            console.error('❌ Error en análisis:', result.error);
            alert('Error en análisis: ' + result.error);
        }
    } catch (error) {
        console.error('❌ analyzeFileWithAlgorithms error:', error);
        alert('Error ejecutando análisis');
    }
}

// 🤖 Función para analizar archivo individual (todos los algoritmos)
window.analyzeFile = async function(filePath) {
    try {
        console.log('🤖 Iniciando análisis LLM para:', filePath);
        
        // Actualizar UI
        document.getElementById('status').textContent = 'Analizando con LLM...';
        
        // Llamar al análisis LLM
        const result = await ipcRenderer.invoke('analyze-llm', filePath);
        
        if (result.success) {
            document.getElementById('status').textContent = '✅ Análisis completado';
            console.log('✅ Análisis LLM completado para:', filePath);
            
            // Recargar archivos para mostrar datos actualizados
            await loadExistingFiles();
        } else {
            document.getElementById('status').textContent = `❌ Error en análisis: ${result.error}`;
            console.error('❌ Error en análisis LLM:', result.error);
        }
        
    } catch (error) {
        console.error('❌ Error analizando archivo:', error);
        document.getElementById('status').textContent = '❌ Error en análisis';
    }
}

// 🎯 Función para mostrar menú de análisis
window.showAnalysisMenu = function() {
    const files = filteredFiles.length > 0 ? filteredFiles : audioFiles;
    
    if (files.length === 0) {
        alert('No hay archivos para analizar. Por favor carga archivos primero.');
        return;
    }
    
    // Calcular estadísticas
    let totalPending = 0;
    let totalPartial = 0;
    let totalComplete = 0;
    
    files.forEach(file => {
        const count = getAnalyzedFieldsCount(file);
        if (count === 0) totalPending++;
        else if (count === 19) totalComplete++;
        else totalPartial++;
    });
    
    const message = `🎯 MENÚ DE ANÁLISIS
    
📊 Estado actual:
• Total de archivos: ${files.length}
• ⏳ Sin analizar: ${totalPending}
• 🔄 Análisis parcial: ${totalPartial}
• ✅ Completados: ${totalComplete}

¿Qué deseas hacer?

1️⃣ Analizar TODOS los archivos (19 algoritmos)
2️⃣ Analizar solo archivos pendientes
3️⃣ Completar archivos parciales
4️⃣ Análisis LLM únicamente
5️⃣ Cancelar`;
    
    const choice = prompt(message);
    
    switch(choice) {
        case '1':
            runBatchAnalysis(files, 'all');
            break;
        case '2':
            const pendingFiles = files.filter(f => getAnalyzedFieldsCount(f) === 0);
            runBatchAnalysis(pendingFiles, 'pending');
            break;
        case '3':
            const partialFiles = files.filter(f => {
                const count = getAnalyzedFieldsCount(f);
                return count > 0 && count < 19;
            });
            runBatchAnalysis(partialFiles, 'partial');
            break;
        case '4':
            runLLMAnalysis(files);
            break;
    }
}

// 🚀 Función para ejecutar análisis por lotes mejorada
async function runBatchAnalysis(files, type) {
    if (files.length === 0) {
        alert('No hay archivos para procesar');
        return;
    }
    
    const statusEl = document.getElementById('status');
    let processed = 0;
    
    for (const file of files) {
        processed++;
        if (statusEl) {
            statusEl.textContent = `🔬 Analizando (${processed}/${files.length}): ${file.file_name}`;
        }
        
        // Determinar qué algoritmos necesita
        const missingAlgorithms = AI_FIELDS.filter(field => 
            !file[field] || file[field] === '' || file[field] === null
        );
        
        if (missingAlgorithms.length > 0) {
            await analyzeFileWithAlgorithms(file.file_path, missingAlgorithms);
        }
        
        // Pequeña pausa entre archivos
        await new Promise(resolve => setTimeout(resolve, 100));
    }
    
    if (statusEl) {
        statusEl.textContent = `✅ Análisis completado: ${processed} archivos procesados`;
    }
    
    // Recargar vista
    await loadFilesSimple();
}



// Función para mostrar estadísticas de base de datos
window.showDatabaseCounts = async function() {
    try {
        console.log('📊 Getting database counts...');
        const counts = await ipcRenderer.invoke('get-database-counts');
        
        if (counts.error) {
            alert(`Error: ${counts.error}`);
            return;
        }
        
        const message = `📊 Database Statistics:
        
📁 Audio Files: ${counts.audioFiles.toLocaleString()}
🧠 With LLM Analysis: ${counts.analyzedFiles.toLocaleString()}
📊 LLM Metadata Records: ${counts.llmMetadata.toLocaleString()}
📂 Folders: ${counts.folders.toLocaleString()}
💾 Total Size: ${counts.totalSizeGB} GB
📈 Analysis Progress: ${counts.analysisProgress}%`;
        
        alert(message);
        
    } catch (error) {
        console.error('❌ Error getting database counts:', error);
        alert('Error getting statistics');
    }
};

// Batch analysis variables
let batchCancelled = false;
let batchStartTime = null;
let batchInterval = null;

// Function to run batch analysis with real algorithms
async function runBatchAnalysisReal() {
    try {
        if (!currentFiles || currentFiles.length === 0) {
            alert('No hay archivos para analizar. Por favor carga archivos primero.');
            return;
        }

        // Show progress modal
        showBatchProgress();
        
        batchCancelled = false;
        batchStartTime = Date.now();
        let completed = 0;
        let errors = 0;
        
        // Update UI
        document.getElementById('files-total').textContent = currentFiles.length;
        
        // Start elapsed time timer
        batchInterval = setInterval(updateElapsedTime, 1000);
        
        // Process files one by one
        for (let i = 0; i < currentFiles.length; i++) {
            if (batchCancelled) {
                addProgressLog('⛔ Análisis cancelado por el usuario');
                break;
            }
            
            const file = currentFiles[i];
            
            // Update current file
            document.getElementById('current-file-name').textContent = file.name || file.file_name;
            
            try {
                addProgressLog(`🔄 Analizando: ${file.name || file.file_name}`);
                
                const result = await ipcRenderer.invoke('analyze-file-with-algorithms', file.path || file.file_path);
                
                if (result.success) {
                    completed++;
                    addProgressLog(`✅ Completado: ${file.name || file.file_name} (${result.algorithmsCompleted}/19 algoritmos)`);
                } else {
                    errors++;
                    addProgressLog(`❌ Error: ${file.name || file.file_name} - ${result.error}`);
                }
            } catch (error) {
                errors++;
                addProgressLog(`❌ Error procesando ${file.name || file.file_name}: ${error.message}`);
            }
            
            // Update progress
            const processed = i + 1;
            document.getElementById('files-processed').textContent = processed;
            
            const percentage = Math.round((processed / currentFiles.length) * 100);
            document.getElementById('progress-bar-fill').style.width = percentage + '%';
            document.getElementById('progress-percentage').textContent = percentage + '%';
            
            // Update time remaining
            updateTimeRemaining(processed, currentFiles.length);
        }
        
        // Clear interval
        clearInterval(batchInterval);
        
        // Final update
        const finalMessage = batchCancelled ? 
            `⛔ Análisis cancelado: ${completed} completados, ${errors} errores` :
            `✅ Análisis completado: ${completed} éxitos, ${errors} errores`;
            
        addProgressLog(finalMessage);
        updateStatus(finalMessage);
        
        // Show close button, hide cancel button
        document.getElementById('cancel-batch').style.display = 'none';
        document.getElementById('close-progress').style.display = 'inline-block';
        
        // Reload files to show updated data
        await loadFilesSimple();
        
    } catch (error) {
        console.error('❌ Error en análisis por lotes:', error);
        addProgressLog(`❌ Error crítico: ${error.message}`);
        clearInterval(batchInterval);
    }
}

// Show batch progress modal
function showBatchProgress() {
    document.getElementById('batch-progress-modal').style.display = 'flex';
    document.getElementById('progress-log').innerHTML = '';
    document.getElementById('files-processed').textContent = '0';
    document.getElementById('progress-bar-fill').style.width = '0%';
    document.getElementById('progress-percentage').textContent = '0%';
    document.getElementById('current-file-name').textContent = '-';
    document.getElementById('time-elapsed').textContent = '00:00';
    document.getElementById('time-remaining').textContent = 'Calculando...';
    document.getElementById('cancel-batch').style.display = 'inline-block';
    document.getElementById('close-progress').style.display = 'none';
}

// Close batch progress modal
function closeBatchProgress() {
    document.getElementById('batch-progress-modal').style.display = 'none';
    clearInterval(batchInterval);
}

// Cancel batch analysis
function cancelBatchAnalysis() {
    batchCancelled = true;
    addProgressLog('🛑 Cancelando análisis...');
}

// Add log entry to progress modal
function addProgressLog(message) {
    const log = document.getElementById('progress-log');
    const entry = document.createElement('div');
    entry.className = 'progress-log-entry';
    entry.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
    log.appendChild(entry);
    log.scrollTop = log.scrollHeight;
}

// Update elapsed time
function updateElapsedTime() {
    if (!batchStartTime) return;
    
    const elapsed = Date.now() - batchStartTime;
    const minutes = Math.floor(elapsed / 60000);
    const seconds = Math.floor((elapsed % 60000) / 1000);
    
    document.getElementById('time-elapsed').textContent = 
        `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
}

// Update time remaining estimate
function updateTimeRemaining(processed, total) {
    if (!batchStartTime || processed === 0) return;
    
    const elapsed = Date.now() - batchStartTime;
    const avgTimePerFile = elapsed / processed;
    const remaining = (total - processed) * avgTimePerFile;
    
    if (remaining <= 0) {
        document.getElementById('time-remaining').textContent = '00:00';
        return;
    }
    
    const minutes = Math.floor(remaining / 60000);
    const seconds = Math.floor((remaining % 60000) / 1000);
    
    document.getElementById('time-remaining').textContent = 
        `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
}

// Make functions globally available
window.runBatchAnalysisReal = runBatchAnalysisReal;
window.closeBatchProgress = closeBatchProgress;
window.cancelBatchAnalysis = cancelBatchAnalysis;

console.log('✅ Renderer.js loaded successfully');