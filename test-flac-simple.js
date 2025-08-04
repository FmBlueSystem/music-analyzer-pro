const flacMetadata = require('flac-metadata');
const fs = require('fs');

async function testSimpleFLAC() {
    console.log('🧪 Testing simple FLAC API...');
    
    const testFile = "/Users/freddymolina/Desktop/pro2/tests/fixtures/audio/TestAudio/Kool & The Gang - Celebremos.flac";
    
    try {
        // Try reading first
        const inputBuffer = fs.readFileSync(testFile);
        console.log('📁 File read, size:', inputBuffer.length);
        
        // Create a simple processor
        const processor = new flacMetadata.Processor({
            parseMetaDataBlocks: true
        });
        
        let hasVorbis = false;
        
        processor.on('preprocess', (mdb) => {
            console.log('📦 Block type:', mdb.type);
            console.log('📦 Block data:', Object.keys(mdb));
            
            if (mdb.type === 4) { // VORBIS_COMMENT type
                hasVorbis = true;
                console.log('🎵 Found Vorbis Comment block');
                console.log('📝 Comments:', mdb.comments);
            }
        });
        
        processor.on('finish', (outputBuffer) => {
            console.log('✅ Processing finished, output size:', outputBuffer.length);
            console.log('🎵 Had Vorbis block:', hasVorbis);
        });
        
        processor.on('error', (error) => {
            console.error('❌ Processor error:', error.message);
        });
        
        // Start processing
        console.log('🚀 Starting processing...');
        processor.push(inputBuffer);
        
    } catch (error) {
        console.error('❌ Test failed:', error.message);
    }
}

testSimpleFLAC();