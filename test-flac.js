const MetadataWriter = require('./metadata-writer');
const path = require('path');

async function testFLACWriting() {
    console.log('🧪 Testing FLAC metadata writing...');
    
    const writer = new MetadataWriter();
    
    // Find a FLAC file to test
    const testFile = "/Users/freddymolina/Desktop/pro2/tests/fixtures/audio/TestAudio/Kool & The Gang - Celebremos.flac";
    
    // Test metadata
    const testMetadata = {
        bpm_llm: 128,
        energy: 7,
        mood: 'energetic',
        analyzed_by: 'Claude Test'
    };
    
    try {
        // Read existing metadata first
        const existing = await writer.readExistingMetadata(testFile);
        console.log('📖 Existing metadata read:', existing.exists);
        
        // Write LLM metadata
        const result = await writer.writeToFLAC(testFile, testMetadata, existing);
        console.log('✅ FLAC write result:', result);
        
    } catch (error) {
        console.error('❌ Test failed:', error.message);
    }
}

testFLACWriting();