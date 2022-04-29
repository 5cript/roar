const { spawn } = require('child_process');
const fs = require('fs');
const fsExtra = require('fs-extra');
const pathTools = require('path');
const { sphinxBuildPath, documentationRoot, sphinxPath } = require('/.constants');

// Copy doxygen docs over to target directory:
const createDoxygen = (onDoxygenDone) => {
    const doxygenDestination = pathTools.join(sphinxBuildPath, 'html', 'doxygen');
    if (fs.existsSync(doxygenDestination)) {
        console.log('Doxygen directory already exists and will not be created');
        onDoxygenDone();
        return;
    }
    fs.mkdirSync(doxygenDestination, { recursive: true });
    const doxygen = spawn('doxygen', [pathTools.join(documentationRoot, 'doxyfile')], {
        cwd: pathTools.dirname(documentationRoot),
        env: {
            'BUILD_DIR': pathTools.join(sphinxBuildPath, 'html', 'doxygen')
        }
    });
    doxygen.stdout.on('data', (data) => {
        console.log(`${data}`);
    })
    doxygen.stderr.on('data', (data) => {
        console.error(`${data}`);
    })
    doxygen.on('exit', () => {
        fsExtra.copySync(pathTools.join(doxygenDestination, "html"), doxygenDestination);
        fs.rmSync(pathTools.join(doxygenDestination, "html"), { recursive: true });
        onDoxygenDone()
    });
};

const runSphinx = () => {
    const make = spawn(
        'sphinx-build',
        ['-M', 'html', sphinxPath, sphinxBuildPath],
        {
            cwd: sphinxBuildPath
        }
    );
    make.stdout.on('data', (data) => {
        console.log(`${data}`);
    })
    make.stderr.on('data', (data) => {
        console.error(`${data}`);
    })
}

const build = () => {
    if (!fs.existsSync(sphinxBuildPath))
        fs.mkdirSync(sphinxBuildPath);

    createDoxygen(() => {
        runSphinx();
    });
}

build();
