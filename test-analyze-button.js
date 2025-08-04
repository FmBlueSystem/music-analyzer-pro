#!/usr/bin/env node
/**
 * 🧪 Test: Verificar funcionalidad del botón de análisis
 */

const MusicDatabase = require('./database');

async function testAnalyzeButton() {
    console.log('🧪 Testing Analyze Button Functionality...\n');
    
    const database = new MusicDatabase();
    await database.initialize();
    
    // 1. Check files available for analysis
    const totalFiles = await database.getQuery('SELECT COUNT(*) as count FROM audio_files');
    console.log(`📊 Total files available: ${totalFiles.count.toLocaleString()}`);
    
    // 2. Check current analysis status
    const pendingFiles = await database.getQuery('SELECT COUNT(*) as count FROM audio_files WHERE analysis_status = "pending"');
    const analyzingFiles = await database.getQuery('SELECT COUNT(*) as count FROM audio_files WHERE analysis_status = "analyzing"');
    const completedFiles = await database.getQuery('SELECT COUNT(*) as count FROM audio_files WHERE analysis_status = "completed"');
    
    console.log(`📋 Analysis Status:`);
    console.log(`   ⏸️  Pending: ${pendingFiles.count.toLocaleString()}`);
    console.log(`   🔄 Analyzing: ${analyzingFiles.count.toLocaleString()}`);  
    console.log(`   ✅ Completed: ${completedFiles.count.toLocaleString()}`);
    
    // 3. Check LLM metadata records
    const llmRecords = await database.getQuery('SELECT COUNT(*) as count FROM llm_metadata');
    console.log(`🧠 LLM Metadata Records: ${llmRecords.count.toLocaleString()}`);
    
    // 4. Get sample files for button testing
    const sampleFiles = await database.allQuery(`
        SELECT file_name, file_path, analysis_status 
        FROM audio_files 
        ORDER BY date_added DESC
        LIMIT 5
    `);
    
    console.log(`\n🎵 Sample Files Ready for Analysis:`);
    sampleFiles.forEach((file, index) => {
        console.log(`${index + 1}. ${file.file_name}`);
        console.log(`   Status: ${file.analysis_status}`);
        console.log(`   Path: ${file.file_path.substring(0, 80)}...`);
    });
    
    await database.close();
    
    console.log('\n✅ Button Test Environment Ready!');
    console.log('💡 Instructions:');
    console.log('   1. Open the Electron app');
    console.log('   2. Click on any "🤖 Analizar" button in the table');
    console.log('   3. Watch for "⏳ Analizando..." button state');
    console.log('   4. Check console logs for analysis progress');
    console.log('   5. Verify table updates after analysis');
}

testAnalyzeButton().catch(console.error);