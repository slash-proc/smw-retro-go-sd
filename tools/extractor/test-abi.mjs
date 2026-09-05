// ABI behaviour tests: the error paths, flag handling, cancellation and
// stepped/one-shot equivalence that the happy path never reaches.
//
// Needs a real ROM, so this cannot run on a public CI runner. check.sh runs it
// whenever it is given one.
//
//   node test-abi.mjs <rom.sfc>

import { extract } from "./extract.mjs";
import { readFileSync } from "node:fs";
import { createHash } from "node:crypto";

const wasm = new Uint8Array(readFileSync("./target/wasm32-unknown-unknown/release/smw_restool.wasm"));
const rom = new Uint8Array(readFileSync(process.argv[2]));
let bad = 0;
const check = (n, c, d="") => { console.log(c ? `  ok   ${n}` : `  FAIL ${n} ${d}`); if(!c) bad++; };

// garbage input must be refused, not crash
try {
  await extract(wasm, new Uint8Array(1024));
  check("rejects a non-SMW file", false, "-> it accepted it");
} catch (e) { check("rejects a non-SMW file", true, e.message); }

// the input list: SMW takes exactly one file, and both the empty and the
// over-supplied case must be refused with a message rather than guessed at
try {
  await extract(wasm, []);
  check("rejects an empty input list", false, "-> accepted");
} catch (e) { check("rejects an empty input list", /no input/i.test(e.message), `-> ${e.message}`); }

try {
  await extract(wasm, [rom, rom]);
  check("rejects more inputs than the project takes", false, "-> accepted");
} catch (e) {
  check("rejects more inputs than the project takes", /exactly one input/i.test(e.message), `-> ${e.message}`);
}

// a one-file list and a bare buffer are the same request
const asList = await extract(wasm, [rom]);
const asBare = await extract(wasm, rom);
check("a one-file list matches the bare-buffer shorthand",
  Buffer.compare(Buffer.from(asList.outputs[0].data), Buffer.from(asBare.outputs[0].data)) === 0);

// reserved flag bits must be refused rather than ignored
try {
  await extract(wasm, rom, { flags: 1 << 5 });
  check("rejects reserved flag bits", false, "-> accepted");
} catch (e) { check("rejects reserved flag bits", /flag/i.test(e.message), `-> ${e.message}`); }

// A modified ROM converts, with no flag involved. Admission is the host's
// call, made from the input's `strict` flag before the module sees anything;
// what the module owes is a run and a warning, not a refusal.
const hacked = rom.slice(); hacked[0x7FD0] ^= 0xff;
try {
  const r = await extract(wasm, hacked, { flags: 0 });
  check("runs a modified ROM", r.outputs.length === 1);
  check("warns that the ROM is not the one it knows",
    r.warnings.some((w) => /not the Super Mario World \(USA\) ROM/.test(w)),
    `-> ${JSON.stringify(r.warnings)}`);
} catch (e) { check("runs a modified ROM", false, `-> ${e.message}`); }

// Bit 0 carried that decision and is retired. An old caller still setting it
// must be told, not silently given whatever bit 0 means now.
try {
  await extract(wasm, rom, { flags: 1 });
  check("rejects the retired hash-check bit", false, "-> accepted");
} catch (e) { check("rejects the retired hash-check bit", /flag/i.test(e.message), `-> ${e.message}`); }

// --no-include-rom must actually shrink the output
const withRom = await extract(wasm, rom, { flags: 0 });
const without = await extract(wasm, rom, { flags: 2 });
check("noIncludeRom omits the ROM", without.outputs[0].data.length < withRom.outputs[0].data.length,
  `-> ${withRom.outputs[0].data.length} vs ${without.outputs[0].data.length}`);

// manifest-declared output names are enforced
try {
  await extract(wasm, rom, { expectedOutputs: ["something_else.dat"] });
  check("enforces the manifest's output list", false, "-> accepted");
} catch (e) { check("enforces the manifest's output list", /manifest declares/.test(e.message)); }

// the stepped and one-shot routes must agree
const stages = [];
const stepped = await extract(wasm, rom, { onProgress: p => stages.push(p.name) });
const sha = b => createHash("sha256").update(b).digest("hex");
check("progress reported every stage", stages.length >= 23, `-> ${stages.length}`);
check("stepped output equals the reference", sha(stepped.outputs[0].data) === "86274eb42561664d68710b8912294dd6d3cc84c4e4a7cbe9d26a8ca6256cc6b6");

// cancellation: stop asking for steps
try {
  let n = 0;
  await extract(wasm, rom, { shouldCancel: () => ++n > 3 });
  check("cancellation stops the run", false, "-> ran to completion");
} catch (e) { check("cancellation stops the run", /cancel/i.test(e.message), `-> ${e.message}`); }

console.log(bad === 0 ? "\nAll edge-case tests passed." : `\n${bad} failed.`);
process.exit(bad ? 1 : 0);
