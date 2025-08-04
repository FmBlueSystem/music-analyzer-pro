const { ipcRenderer } = require('electron');

console.log('🚀 Minimal renderer loading...');

// 🔄 Función de navegación ULTRA SIMPLE
window.switchTab = function(targetTab) {
    console.log('🔄 switchTab called:', targetTab);
    
    try {
        // Update buttons
        document.querySelectorAll('.tab-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === targetTab);
        });
        
        // Update contents
        document.querySelectorAll('.tab-content').forEach(content => {
            const isActive = content.id === `${targetTab}-tab`;
            content.classList.toggle('active', isActive);
            content.style.display = isActive ? 'block' : 'none';
        });
        
        console.log('✅ Tab switched to:', targetTab);
        
    } catch (error) {
        console.error('❌ Error in switchTab:', error);
    }
};

// Función para estadísticas (SIN async/await para evitar problemas)
window.showDatabaseCounts = function() {
    console.log('📊 Showing database counts...');
    
    ipcRenderer.invoke('get-database-counts')
        .then(counts => {
            if (counts.error) {
                alert(`Error: ${counts.error}`);
                return;
            }
            
            const message = `📊 Database Statistics:
            
📁 Audio Files: ${counts.audioFiles.toLocaleString()}
🧠 With LLM Analysis: ${counts.analyzedFiles.toLocaleString()}
📊 LLM Metadata Records: ${counts.llmMetadata.toLocaleString()}
📂 Folders: ${counts.folders.toLocaleString()}
💾 Total Size: ${counts.totalSizeGB} GB`;
            
            alert(message);
        })
        .catch(error => {
            console.error('❌ Error getting database counts:', error);
            alert('Error getting statistics');
        });
};

// NO event listeners automáticos - solo funciones básicas
console.log('✅ Minimal renderer loaded - NO AUTO LOADING');