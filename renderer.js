const { ipcRenderer } = require('electron');

console.log('🚀 Renderer.js loading...');

// Variables globales básicas
let audioFiles = [];
let currentPage = 1;
const filesPerPage = 100; // Mostrar 100 archivos por página

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
function renderBasicTable() {
    const tableBody = document.getElementById('filesTableBody');
    if (!tableBody) {
        console.log('❌ Table body not found (filesTableBody)');
        return;
    }
    
    if (audioFiles.length === 0) {
        console.log('⚠️ No files to render');
        tableBody.innerHTML = '<tr><td colspan="10" class="text-center">No hay archivos para mostrar</td></tr>';
        return;
    }
    
    console.log('🔄 Rendering basic table...');
    
    tableBody.innerHTML = '';
    
    // Calcular rango de archivos para la página actual
    const startIndex = (currentPage - 1) * filesPerPage;
    const endIndex = startIndex + filesPerPage;
    const filesToShow = audioFiles.slice(startIndex, endIndex);
    
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
    
    console.log(`✅ Basic table rendered - Página ${currentPage}, mostrando ${filesToShow.length} de ${audioFiles.length} archivos`);
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

// 📄 Función para cambiar de página
window.changePage = function(newPage) {
    const totalPages = Math.ceil(audioFiles.length / filesPerPage);
    
    if (newPage < 1 || newPage > totalPages) {
        console.warn(`⚠️ Página ${newPage} fuera de rango (1-${totalPages})`);
        return;
    }
    
    currentPage = newPage;
    console.log(`📄 Cambiando a página ${currentPage}`);
    renderBasicTable();
}

// 🤖 Función para analizar archivo individual
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

// 🧠 Función para análisis LLM masivo
async function runBatchLLMAnalysis() {
    try {
        console.log('🚀 Iniciando análisis LLM masivo...');
        
        if (audioFiles.length === 0) {
            alert('No hay archivos para analizar. Por favor carga archivos primero.');
            return;
        }
        
        document.getElementById('status').textContent = `🧠 Analizando ${audioFiles.length} archivos...`;
        
        let completed = 0;
        let errors = 0;
        
        // Procesar archivos en lotes pequeños para evitar sobrecarga
        const batchSize = 3;
        for (let i = 0; i < audioFiles.length; i += batchSize) {
            const batch = audioFiles.slice(i, i + batchSize);
            
            // Procesar lote actual
            const promises = batch.map(async (file) => {
                try {
                    console.log(`🧠 Analizando ${file.file_name}...`);
                    const result = await ipcRenderer.invoke('analyze-llm', file.file_path);
                    
                    if (result.success) {
                        completed++;
                        console.log(`✅ Completado: ${file.file_name}`);
                    } else {
                        errors++;
                        console.error(`❌ Error: ${file.file_name} - ${result.error}`);
                    }
                } catch (error) {
                    errors++;
                    console.error(`❌ Error procesando ${file.file_name}:`, error);
                }
            });
            
            // Esperar que termine el lote actual
            await Promise.all(promises);
            
            // Actualizar progreso
            const progress = Math.round(((i + batch.length) / audioFiles.length) * 100);
            document.getElementById('status').textContent = 
                `🧠 Progreso: ${completed}/${audioFiles.length} (${progress}%) - Errores: ${errors}`;
            
            // Pausa pequeña entre lotes para no sobrecargar
            await new Promise(resolve => setTimeout(resolve, 1000));
        }
        
        // Resultado final
        document.getElementById('status').textContent = 
            `✅ Análisis completado: ${completed} éxitos, ${errors} errores`;
        
        console.log(`🎉 Análisis LLM masivo completado: ${completed} éxitos, ${errors} errores`);
        
        // Recargar datos para mostrar resultados
        await loadExistingFiles();
        
    } catch (error) {
        console.error('❌ Error en análisis LLM masivo:', error);
        document.getElementById('status').textContent = '❌ Error en análisis masivo';
    }
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

console.log('✅ Renderer.js loaded successfully');