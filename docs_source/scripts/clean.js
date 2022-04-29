const fs = require('fs');

const { sphinxBuildPath } = require('/.constants');

clean = () => {
    fs.rmSync(pathTools.join(sphinxBuildPath), { recursive: true });
}

clean();
