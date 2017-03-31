//
"use strict";
const reReset = /BGJS_PERS_RESET ([^\s]*) file (.*) line (.*) func (.*)/;
const reNew = /BGJS_PERS_NEW(_PTR)? ([^\s]*) file (.*) line (.*) func (.*)/;

process.stdin.setEncoding('utf8');

var persistents = {};

const doAnalyze = function() {
    console.log("Found persistents:\n");
    for (let ptr in persistents) {
        const data = persistents[ptr];
        console.log(ptr, data[4], data[5]);
    }
};

process.stdin.on('readable', () => {
    var chunk = process.stdin.read();
    if (chunk !== null) {
        // process.stdout.write(`data: ${chunk}`);
        const lines = chunk.split(/\r?\n/);
        for (let lineNo in lines) {
            const line = lines[lineNo];
            const resetted = reReset.exec(line);
            if (resetted) {
                if (!persistents[resetted[1]]) {
                    console.warn("A persistent has been reset that I know nothing about:", resetted[1], resetted[2], resetted[3]);
                } else {
                    delete (persistents[resetted[1]]);
                    console.log("Reset", resetted[1]);
                }
            } else {
                const created = reNew.exec(line);
                if (created) {
                    console.log("Created", created[2]);
                    persistents[created[2]] = created;
                }
            }
        }
    }
});

process.stdin.on('end', () => {
    doAnalyze();
    process.stdout.write('end');
});

process.on('SIGINT', function() {
    console.log("Caught interrupt signal");
    doAnalyze();

    process.exit();
});