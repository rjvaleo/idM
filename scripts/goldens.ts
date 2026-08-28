// Regenerates the cross-language conformance fixtures.
//
//   npx vite-node scripts/goldens.ts           write the files
//   npx vite-node scripts/goldens.ts --check   fail if they would change
//
// These files are the contract between the TypeScript engine and the Rust one.
// The Rust side reads exactly these bytes and must reproduce them; that is what
// makes the port a checkable task rather than a rewrite.

import { writeFileSync, readFileSync, existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

import { goldenFiles } from "../src/engine/goldenFixtures";

const goldens = join(dirname(fileURLToPath(import.meta.url)), "..", "src", "engine", "__goldens__");
const check = process.argv.includes("--check");
let drift = 0;

for (const [name, content] of Object.entries(goldenFiles())) {
  const path = join(goldens, name);
  const existing = existsSync(path) ? readFileSync(path, "utf8") : null;

  if (existing === content) {
    console.log(`  unchanged  ${name}`);
    continue;
  }

  drift++;

  if (check) {
    console.error(`  DRIFTED    ${name}${existing === null ? " (missing)" : ""}`);
  } else {
    writeFileSync(path, content);
    console.log(`  written    ${name}`);
  }
}

if (check && drift > 0) {
  console.error(`\n${drift} golden(s) would change. Run: npx vite-node scripts/goldens.ts`);
  process.exit(1);
}
