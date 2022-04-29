const liveServer = require('live-server');
const process = require('process');
const pathTools = require('path');
const os = require('os');

const root = pathTools.join(os.tmpdir(), 'roar-doc', 'html');
console.log(root);

const params = {
    port: 3000,
    root: root,
    quiet: true,
    open: true, // Open browser
    ignore: '', // comma-separated string for paths to ignore
    file: 'index.html', // When set, serve this file (server root relative) for every 404 (useful for single-page applications)
    wait: 1000, // Waits for all changes, before reloading. 
    mount: [['../components', '../node_modules']], // Mount a directory to a route.
    logLevel: 1, // 0 = errors only, 1 = some, 2 = lots
    middleware: [function (req, res, next) { next(); }]
};
liveServer.start(params);
