const pathTools = require('path');
const os = require('os');
const process = require('process');

const documentationRoot = process.cwd();
const sphinxBuildPath = pathTools.join(documentationRoot, '..', 'docs');
const sphinxPath = pathTools.join(documentationRoot, 'sphinx');

module.exports = {
    sphinxBuildPath,
    documentationRoot,
    sphinxPath
}