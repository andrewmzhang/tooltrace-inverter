importScripts('index.js');

let moduleReady = false;
let MyModulePromise = Module(); // Initialize Emscripten module

MyModulePromise.then(Module => {
    moduleReady = true;
    self.Module = Module;

    Module.postLog = (logBuffer) => {
        self.postMessage({type: 'log', log: Module.UTF8ToString(logBuffer)});
        Module._free(logBuffer);
    };

    self.onmessage = function (e) {
        const {text, tolerance} = e.data
        self.postMessage({type: 'blob', blob: convertSTEPBufferToSTLBlob(text, tolerance)});   // send result back to main thread
    };

    self.postMessage({type: 'ready'});
});

function formatBytes(bytes, decimals = 2) {
    // Source - https://stackoverflow.com/questions/15900485/correct-way-to-convert-size-in-bytes-to-kb-mb-gb-in-javascript
    // Posted by anon, modified by community. See post 'Timeline' for change history
    // Retrieved 2026-01-11, License - CC BY-SA 4.0

    if (!+bytes) return '0 Bytes'

    const k = 1024
    const dm = decimals < 0 ? 0 : decimals
    const sizes = ['Bytes', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB', 'EiB', 'ZiB', 'YiB']

    const i = Math.floor(Math.log(bytes) / Math.log(k))

    return `${parseFloat((bytes / Math.pow(k, i)).toFixed(dm))} ${sizes[i]}`
}

function convertSTEPBufferToSTLBlob(text, tolerance) {
    // Run computation off the main thread
    // Alloc space for the file, convert it to utf-8
    const inSize = Module.lengthBytesUTF8(text) + 1;

    self.postMessage({type: "log", log: `WebWorker: Received file of size ${inSize}`})

    // Allocate memory for the content pointer and length
    const inBuffer = Module._malloc(inSize);
    Module.stringToUTF8(text, inBuffer, inSize);
    var charBuffer = Module._generate_tool_positive(inBuffer, inSize, tolerance);
    var bufferStr = Module.UTF8ToString(charBuffer)
    self.postMessage({type: "log", log: "WebWorker: STL file size: " + bufferStr.length + " or " + formatBytes(bufferStr.length)})

    // Create a Blob from the content
    var blob = new Blob([bufferStr], {type: 'application/octet-stream'});

    // Cleanup allocations
    Module._free(inBuffer);
    Module._free(charBuffer);

    return blob
}

